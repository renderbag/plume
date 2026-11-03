//
// plume
//

#include "plume_render_interface.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include "shaders/testColorVert.hlsl.dxil.h"
#include "shaders/testColorFrag.hlsl.dxil.h"
#include "shaders/testFullscreenVert.hlsl.dxil.h"
#include "shaders/testTextureFrag.hlsl.dxil.h"
#endif
#include "shaders/testColorVert.hlsl.spirv.h"
#include "shaders/testColorFrag.hlsl.spirv.h"
#include "shaders/testFullscreenVert.hlsl.spirv.h"
#include "shaders/testTextureFrag.hlsl.spirv.h"
#ifdef __APPLE__
#include "shaders/testColorVert.hlsl.metal.h"
#include "shaders/testColorFrag.hlsl.metal.h"
#include "shaders/testFullscreenVert.hlsl.metal.h"
#include "shaders/testTextureFrag.hlsl.metal.h"
#endif

namespace plume {
#ifdef __APPLE__
    extern std::unique_ptr<RenderInterface> CreateMetalInterface();
#endif
#ifdef _WIN32
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
#endif
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

#ifdef __APPLE__
#define MSL_BLOB(name) name##BlobMSL, sizeof(name##BlobMSL)
#else
#define MSL_BLOB(name) nullptr, 0
#endif

#ifdef _WIN32
#define DXIL_BLOB(name) name##BlobDXIL, sizeof(name##BlobDXIL)
#else
#define DXIL_BLOB(name) nullptr, 0
#endif

#define SHADER_BLOBS(name) name##BlobSPIRV, sizeof(name##BlobSPIRV), MSL_BLOB(name), DXIL_BLOB(name)

using namespace plume;

static const RenderFormat ColorFormat = RenderFormat::R8G8B8A8_UNORM;
static const uint32_t TargetSize = 16;

// D3D12 requires 256-byte aligned rows for texture copies.
static const uint32_t RowPitch = 256;

struct Pixel {
    uint8_t r, g, b, a;
};

static const Pixel Untouched = { 0xEE, 0xEE, 0xEE, 0xEE };

struct TestContext {
    RenderDevice *device = nullptr;
    RenderCommandQueue *queue = nullptr;
    RenderShaderFormat shaderFormat = RenderShaderFormat::UNKNOWN;
};

static void submit(TestContext &ctx, const std::function<void(RenderCommandList *)> &record) {
    std::unique_ptr<RenderCommandList> commandList = ctx.queue->createCommandList();
    std::unique_ptr<RenderCommandFence> fence = ctx.device->createCommandFence();
    commandList->begin();
    record(commandList.get());
    commandList->end();

    const RenderCommandList *commandLists[] = { commandList.get() };
    ctx.queue->executeCommandLists(commandLists, 1, nullptr, 0, nullptr, 0, fence.get());
    ctx.queue->waitForCommandFence(fence.get());
}

static std::unique_ptr<RenderShader> createShader(TestContext &ctx, const char *entryPoint, const void *spirv, uint64_t spirvSize, const void *msl, uint64_t mslSize, const void *dxil, uint64_t dxilSize) {
    switch (ctx.shaderFormat) {
        case RenderShaderFormat::SPIRV:
            return ctx.device->createShader(spirv, spirvSize, entryPoint, ctx.shaderFormat);
        case RenderShaderFormat::METAL:
            return ctx.device->createShader(msl, mslSize, entryPoint, ctx.shaderFormat);
        case RenderShaderFormat::DXIL:
            return ctx.device->createShader(dxil, dxilSize, entryPoint, ctx.shaderFormat);
        default:
            return nullptr;
    }
}

static void uploadTexture(TestContext &ctx, RenderTexture *texture, uint32_t mipLevel, uint32_t width, uint32_t height, const std::vector<Pixel> &texels) {
    std::unique_ptr<RenderBuffer> buffer = ctx.device->createBuffer(RenderBufferDesc::UploadBuffer(RowPitch * height));
    uint8_t *mapped = static_cast<uint8_t *>(buffer->map());
    for (uint32_t y = 0; y < height; y++) {
        memcpy(mapped + y * RowPitch, &texels[y * width], width * sizeof(Pixel));
    }

    buffer->unmap();

    submit(ctx, [&](RenderCommandList *commandList) {
        commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture, RenderTextureLayout::COPY_DEST));
        commandList->copyTextureRegion(
            RenderTextureCopyLocation::Subresource(texture, mipLevel),
            RenderTextureCopyLocation::PlacedFootprint(buffer.get(), ColorFormat, width, height, 1, RowPitch / sizeof(Pixel)));
        commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(texture, RenderTextureLayout::SHADER_READ));
    });
}

static std::vector<Pixel> readTexture(TestContext &ctx, RenderTexture *texture, uint32_t mipLevel, uint32_t width, uint32_t height, uint32_t dstX = 0, uint32_t dstY = 0, const RenderBox *srcBox = nullptr) {
    std::unique_ptr<RenderBuffer> buffer = ctx.device->createBuffer(RenderBufferDesc::ReadbackBuffer(RowPitch * height));
    Pixel *mapped = static_cast<Pixel *>(buffer->map());
    std::fill(mapped, mapped + (RowPitch / sizeof(Pixel)) * height, Untouched);
    buffer->unmap();

    submit(ctx, [&](RenderCommandList *commandList) {
        commandList->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture, RenderTextureLayout::COPY_SOURCE));
        commandList->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(buffer.get(), ColorFormat, width, height, 1, RowPitch / sizeof(Pixel)),
            RenderTextureCopyLocation::Subresource(texture, mipLevel), dstX, dstY, 0, srcBox);
    });

    std::vector<Pixel> pixels(width * height);
    const uint8_t *readback = static_cast<const uint8_t *>(buffer->map());
    for (uint32_t y = 0; y < height; y++) {
        memcpy(&pixels[y * width], readback + y * RowPitch, width * sizeof(Pixel));
    }

    buffer->unmap();
    return pixels;
}

static std::string checkPixel(const std::vector<Pixel> &pixels, uint32_t width, uint32_t x, uint32_t y, Pixel expected, int tolerance = 1) {
    const Pixel &actual = pixels[y * width + x];
    auto matches = [tolerance](uint8_t a, uint8_t b) { return std::abs(int(a) - int(b)) <= tolerance; };
    if (matches(actual.r, expected.r) && matches(actual.g, expected.g) && matches(actual.b, expected.b) && matches(actual.a, expected.a)) {
        return {};
    }

    char error[128];
    snprintf(error, sizeof(error), "pixel (%u, %u) is (%u, %u, %u, %u), expected (%u, %u, %u, %u)", x, y, actual.r, actual.g, actual.b, actual.a, expected.r, expected.g, expected.b, expected.a);
    return error;
}

static std::string checkPixels(const std::vector<Pixel> &pixels, uint32_t width, uint32_t height, const std::function<Pixel(uint32_t, uint32_t)> &expected, int tolerance = 1) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            std::string error = checkPixel(pixels, width, x, y, expected(x, y), tolerance);
            if (!error.empty()) {
                return error;
            }
        }
    }

    return {};
}

static Pixel patternTexel(uint32_t x, uint32_t y, uint32_t mipLevel) {
    return { uint8_t(x * 16 + 1), uint8_t(y * 16 + 2), uint8_t(mipLevel * 64 + 3), 0xFF };
}

static std::vector<Pixel> patternTexels(uint32_t width, uint32_t height, uint32_t mipLevel) {
    std::vector<Pixel> texels(width * height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            texels[y * width + x] = patternTexel(x, y, mipLevel);
        }
    }

    return texels;
}

struct Target {
    std::unique_ptr<RenderTexture> color;
    std::unique_ptr<RenderTexture> depth;
    std::unique_ptr<RenderFramebuffer> framebuffer;
    bool stencil = false;

    Target(TestContext &ctx, RenderFormat depthFormat = RenderFormat::UNKNOWN) {
        color = ctx.device->createTexture(RenderTextureDesc::ColorTarget(TargetSize, TargetSize, ColorFormat));
        if (depthFormat != RenderFormat::UNKNOWN) {
            depth = ctx.device->createTexture(RenderTextureDesc::DepthTarget(TargetSize, TargetSize, depthFormat));
            stencil = (depthFormat == RenderFormat::D32_FLOAT_S8_UINT);
        }

        const RenderTexture *colorTexture = color.get();
        framebuffer = ctx.device->createFramebuffer(RenderFramebufferDesc(&colorTexture, 1, depth.get()));
    }

    void begin(RenderCommandList *commandList, RenderColor clearColor) {
        commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
        if (depth != nullptr) {
            commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(depth.get(), RenderTextureLayout::DEPTH_WRITE));
        }

        commandList->setFramebuffer(framebuffer.get());
        commandList->setViewports(RenderViewport(0.0f, 0.0f, float(TargetSize), float(TargetSize)));
        commandList->setScissors(RenderRect(0, 0, TargetSize, TargetSize));
        commandList->clearColor(0, clearColor);
        if (depth != nullptr) {
            commandList->clearDepthStencil(true, stencil, 1.0f, 0);
        }
    }

    std::vector<Pixel> read(TestContext &ctx) {
        return readTexture(ctx, color.get(), 0, TargetSize, TargetSize);
    }
};

struct Vertex {
    float position[3];
    float color[4];
};

static const float Fullscreen[6] = { -1.0f, -1.0f, -1.0f, 3.0f, 3.0f, -1.0f };
static const float LowerLeftHalf[6] = { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f };

static const float FullscreenCounterClockwise[6] = { -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f };

static void addTriangle(std::vector<Vertex> &vertices, const float (&points)[6], float depth, RenderColor color) {
    for (uint32_t i = 0; i < 3; i++) {
        vertices.push_back({ { points[i * 2], points[i * 2 + 1], depth }, { color.r, color.g, color.b, color.a } });
    }
}

struct ColorDraw {
    std::unique_ptr<RenderShader> vertexShader;
    std::unique_ptr<RenderShader> pixelShader;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::unique_ptr<RenderBuffer> vertexBuffer;
    RenderInputSlot inputSlot = RenderInputSlot(0, sizeof(Vertex));
    RenderInputElement inputElements[2] = {
        RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32B32_FLOAT, 0, offsetof(Vertex, position)),
        RenderInputElement("COLOR", 0, 1, RenderFormat::R32G32B32A32_FLOAT, 0, offsetof(Vertex, color))
    };

    uint32_t vertexCount = 0;

    ColorDraw(TestContext &ctx, const std::vector<Vertex> &vertices) {
        vertexShader = createShader(ctx, "VSMain", SHADER_BLOBS(testColorVert));
        pixelShader = createShader(ctx, "PSMain", SHADER_BLOBS(testColorFrag));

        RenderPipelineLayoutDesc layoutDesc;
        layoutDesc.allowInputLayout = true;
        layout = ctx.device->createPipelineLayout(layoutDesc);

        vertexCount = uint32_t(vertices.size());
        vertexBuffer = ctx.device->createBuffer(RenderBufferDesc::VertexBuffer(vertexCount * sizeof(Vertex), RenderHeapType::UPLOAD));
        memcpy(vertexBuffer->map(), vertices.data(), vertexCount * sizeof(Vertex));
        vertexBuffer->unmap();
    }

    RenderGraphicsPipelineDesc pipelineDesc() const {
        RenderGraphicsPipelineDesc desc;
        desc.inputSlots = &inputSlot;
        desc.inputSlotsCount = 1;
        desc.inputElements = inputElements;
        desc.inputElementsCount = 2;
        desc.pipelineLayout = layout.get();
        desc.vertexShader = vertexShader.get();
        desc.pixelShader = pixelShader.get();
        desc.renderTargetFormat[0] = ColorFormat;
        desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        desc.renderTargetCount = 1;
        desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
        return desc;
    }

    void draw(RenderCommandList *commandList, const RenderPipeline *pipeline, uint32_t firstVertex, uint32_t count) {
        const RenderVertexBufferView view(vertexBuffer.get(), vertexCount * sizeof(Vertex));
        commandList->setGraphicsPipelineLayout(layout.get());
        commandList->setPipeline(pipeline);
        commandList->setVertexBuffers(0, &view, 1, &inputSlot);
        commandList->drawInstanced(count, 1, firstVertex, 0);
    }
};

static const RenderColor Black(0.0f, 0.0f, 0.0f, 1.0f);
static const RenderColor White(1.0f, 1.0f, 1.0f, 1.0f);
static const RenderColor Red(1.0f, 0.0f, 0.0f, 1.0f);
static const RenderColor Green(0.0f, 1.0f, 0.0f, 1.0f);
static const RenderColor Blue(0.0f, 0.0f, 1.0f, 1.0f);

static std::string testClearColor(TestContext &ctx) {
    Target target(ctx);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, RenderColor(0.25f, 0.5f, 0.75f, 1.0f));
    });

    return checkPixels(target.read(ctx), TargetSize, TargetSize, [](uint32_t, uint32_t) { return Pixel{ 64, 128, 191, 255 }; });
}

static std::string testTextureReadbackRegion(TestContext &ctx) {
    const uint32_t width = 16;
    const uint32_t height = 16;
    std::unique_ptr<RenderTexture> texture = ctx.device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, ColorFormat));
    uploadTexture(ctx, texture.get(), 0, width, height, patternTexels(width, height, 0));

    const RenderBox box(8, 4, 12, 6);
    const uint32_t dstX = 3;
    const uint32_t dstY = 2;
    const uint32_t footprintWidth = 16;
    const uint32_t footprintHeight = 8;
    std::vector<Pixel> pixels = readTexture(ctx, texture.get(), 0, footprintWidth, footprintHeight, dstX, dstY, &box);
    return checkPixels(pixels, footprintWidth, footprintHeight, [&](uint32_t x, uint32_t y) {
        const bool copied = (x >= dstX) && (x < dstX + 4) && (y >= dstY) && (y < dstY + 2);
        return copied ? patternTexel(box.left + x - dstX, box.top + y - dstY, 0) : Untouched;
    }, 0);
}

static std::string testTextureReadbackMip(TestContext &ctx) {
    std::unique_ptr<RenderTexture> texture = ctx.device->createTexture(RenderTextureDesc::Texture2D(16, 8, 2, ColorFormat));
    uploadTexture(ctx, texture.get(), 1, 8, 4, patternTexels(8, 4, 1));

    std::vector<Pixel> pixels = readTexture(ctx, texture.get(), 1, 16, 8);
    return checkPixels(pixels, 16, 8, [](uint32_t x, uint32_t y) {
        return ((x < 8) && (y < 4)) ? patternTexel(x, y, 1) : Untouched;
    }, 0);
}

static std::string testDepthTest(TestContext &ctx) {
    std::vector<Vertex> vertices;
    addTriangle(vertices, Fullscreen, 0.25f, Blue);
    addTriangle(vertices, Fullscreen, 0.75f, Red);
    ColorDraw colorDraw(ctx, vertices);

    RenderGraphicsPipelineDesc desc = colorDraw.pipelineDesc();
    desc.depthEnabled = true;
    desc.depthWriteEnabled = true;
    desc.depthFunction = RenderComparisonFunction::LESS;
    desc.depthTargetFormat = RenderFormat::D32_FLOAT;
    std::unique_ptr<RenderPipeline> pipeline = ctx.device->createGraphicsPipeline(desc);

    Target target(ctx, RenderFormat::D32_FLOAT);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, Black);
        colorDraw.draw(commandList, pipeline.get(), 0, 6);
    });

    return checkPixels(target.read(ctx), TargetSize, TargetSize, [](uint32_t, uint32_t) { return Pixel{ 0, 0, 255, 255 }; });
}

static std::string testStencilReference(TestContext &ctx) {
    std::vector<Vertex> vertices;
    addTriangle(vertices, LowerLeftHalf, 0.0f, Black);
    addTriangle(vertices, Fullscreen, 0.0f, White);
    ColorDraw colorDraw(ctx, vertices);

    RenderGraphicsPipelineDesc desc = colorDraw.pipelineDesc();
    desc.depthTargetFormat = RenderFormat::D32_FLOAT_S8_UINT;
    desc.stencilEnabled = true;
    desc.stencilReference = 1;

    RenderGraphicsPipelineDesc markDesc = desc;
    markDesc.stencilFrontFace.compareFunction = RenderComparisonFunction::ALWAYS;
    markDesc.stencilFrontFace.passOp = RenderStencilOp::REPLACE;
    markDesc.stencilBackFace = markDesc.stencilFrontFace;
    std::unique_ptr<RenderPipeline> markPipeline = ctx.device->createGraphicsPipeline(markDesc);

    RenderGraphicsPipelineDesc fillDesc = desc;
    fillDesc.stencilWriteMask = 0;
    fillDesc.stencilFrontFace.compareFunction = RenderComparisonFunction::EQUAL;
    fillDesc.stencilBackFace = fillDesc.stencilFrontFace;
    std::unique_ptr<RenderPipeline> fillPipeline = ctx.device->createGraphicsPipeline(fillDesc);

    Target target(ctx, RenderFormat::D32_FLOAT_S8_UINT);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, Black);
        colorDraw.draw(commandList, markPipeline.get(), 0, 3);
        colorDraw.draw(commandList, fillPipeline.get(), 3, 3);
    });

    // The diagonal sits on the edge of the marked triangle.
    std::vector<Pixel> pixels = target.read(ctx);
    for (uint32_t y = 0; y < TargetSize; y++) {
        for (uint32_t x = 0; x < TargetSize; x++) {
            if (x == y) {
                continue;
            }

            std::string error = checkPixel(pixels, TargetSize, x, y, (x < y) ? Pixel{ 255, 255, 255, 255 } : Pixel{ 0, 0, 0, 255 });
            if (!error.empty()) {
                return error;
            }
        }
    }

    return {};
}

static std::string testBlendAlpha(TestContext &ctx) {
    std::vector<Vertex> vertices;
    addTriangle(vertices, Fullscreen, 0.0f, RenderColor(1.0f, 0.0f, 0.0f, 0.25f));
    ColorDraw colorDraw(ctx, vertices);

    RenderGraphicsPipelineDesc desc = colorDraw.pipelineDesc();
    desc.renderTargetBlend[0] = RenderBlendDesc::AlphaBlend();
    std::unique_ptr<RenderPipeline> pipeline = ctx.device->createGraphicsPipeline(desc);

    Target target(ctx);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, RenderColor(0.0f, 0.0f, 1.0f, 0.5f));
        colorDraw.draw(commandList, pipeline.get(), 0, 3);
    });

    return checkPixels(target.read(ctx), TargetSize, TargetSize, [](uint32_t, uint32_t) { return Pixel{ 64, 0, 191, 159 }; });
}

static std::string testCullBackFaces(TestContext &ctx) {
    std::vector<Vertex> vertices;
    addTriangle(vertices, Fullscreen, 0.0f, Green);
    addTriangle(vertices, FullscreenCounterClockwise, 0.0f, Red);
    ColorDraw colorDraw(ctx, vertices);

    RenderGraphicsPipelineDesc desc = colorDraw.pipelineDesc();
    desc.cullMode = RenderCullMode::BACK;
    desc.frontFace = RenderFrontFace::CLOCKWISE;
    std::unique_ptr<RenderPipeline> pipeline = ctx.device->createGraphicsPipeline(desc);

    Target target(ctx);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, Black);
        colorDraw.draw(commandList, pipeline.get(), 0, 6);
    });

    return checkPixels(target.read(ctx), TargetSize, TargetSize, [](uint32_t, uint32_t) { return Pixel{ 0, 255, 0, 255 }; });
}

static std::string testSamplerLinear(TestContext &ctx) {
    // Linear filtering ramps between the texel centers.
    std::unique_ptr<RenderTexture> texture = ctx.device->createTexture(RenderTextureDesc::Texture2D(2, 1, 1, ColorFormat));
    uploadTexture(ctx, texture.get(), 0, 2, 1, { { 0, 0, 0, 255 }, { 255, 255, 255, 255 } });

    RenderSamplerDesc samplerDesc;
    samplerDesc.minFilter = RenderFilter::LINEAR;
    samplerDesc.magFilter = RenderFilter::LINEAR;
    samplerDesc.addressU = RenderTextureAddressMode::CLAMP;
    samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
    std::unique_ptr<RenderSampler> sampler = ctx.device->createSampler(samplerDesc);

    RenderDescriptorSetBuilder setBuilder;
    setBuilder.begin();
    setBuilder.addTexture(0);
    setBuilder.addSampler(1);
    setBuilder.end();
    std::unique_ptr<RenderDescriptorSet> descriptorSet = setBuilder.create(ctx.device);
    descriptorSet->setTexture(0, texture.get(), RenderTextureLayout::SHADER_READ);
    descriptorSet->setSampler(1, sampler.get());

    RenderPipelineLayoutBuilder layoutBuilder;
    layoutBuilder.begin();
    layoutBuilder.addDescriptorSet(setBuilder);
    layoutBuilder.end();
    std::unique_ptr<RenderPipelineLayout> layout = layoutBuilder.create(ctx.device);

    std::unique_ptr<RenderShader> vertexShader = createShader(ctx, "VSMain", SHADER_BLOBS(testFullscreenVert));
    std::unique_ptr<RenderShader> pixelShader = createShader(ctx, "PSMain", SHADER_BLOBS(testTextureFrag));
    RenderGraphicsPipelineDesc desc;
    desc.pipelineLayout = layout.get();
    desc.vertexShader = vertexShader.get();
    desc.pixelShader = pixelShader.get();
    desc.renderTargetFormat[0] = ColorFormat;
    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
    desc.renderTargetCount = 1;
    desc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
    std::unique_ptr<RenderPipeline> pipeline = ctx.device->createGraphicsPipeline(desc);

    Target target(ctx);
    submit(ctx, [&](RenderCommandList *commandList) {
        target.begin(commandList, Black);
        commandList->setGraphicsPipelineLayout(layout.get());
        commandList->setPipeline(pipeline.get());
        commandList->setGraphicsDescriptorSet(descriptorSet.get(), 0);
        commandList->drawInstanced(3, 1, 0, 0);
    });

    return checkPixels(target.read(ctx), TargetSize, TargetSize, [](uint32_t x, uint32_t) {
        const float u = (x + 0.5f) / TargetSize;
        const float value = std::clamp(u * 2.0f - 0.5f, 0.0f, 1.0f);
        const uint8_t gray = uint8_t(value * 255.0f + 0.5f);
        return Pixel{ gray, gray, gray, 255 };
    }, 2);
}

struct Test {
    const char *name;
    std::string (*run)(TestContext &ctx);
};

static const Test Tests[] = {
    { "clear_color", testClearColor },
    { "texture_readback_region", testTextureReadbackRegion },
    { "texture_readback_mip", testTextureReadbackMip },
    { "depth_test", testDepthTest },
    { "stencil_reference", testStencilReference },
    { "blend_alpha", testBlendAlpha },
    { "cull_back_faces", testCullBackFaces },
    { "sampler_linear", testSamplerLinear },
};

static std::unique_ptr<RenderInterface> createInterface(const std::string &backend) {
#ifdef __APPLE__
    if (backend.empty() || (backend == "metal")) {
        return CreateMetalInterface();
    }
#endif
#ifdef _WIN32
    if (backend.empty() || (backend == "d3d12")) {
        return CreateD3D12Interface();
    }
#endif
    if (backend.empty() || (backend == "vulkan")) {
        return CreateVulkanInterface();
    }

    return nullptr;
}

int main(int argc, char *argv[]) {
    std::string backend;
    std::string deviceName;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--backend") == 0) && (i + 1 < argc)) {
            backend = argv[++i];
        }
        else if ((strcmp(argv[i], "--device") == 0) && (i + 1 < argc)) {
            deviceName = argv[++i];
        }
        else {
            fprintf(stderr, "Usage: plume_tests [--backend metal|vulkan|d3d12] [--device name]\n");
            return 1;
        }
    }

    std::unique_ptr<RenderInterface> renderInterface = createInterface(backend);
    if (renderInterface == nullptr) {
        fprintf(stderr, "Unable to create the render interface.\n");
        return 1;
    }

    std::unique_ptr<RenderDevice> device = renderInterface->createDevice(deviceName);
    if (device == nullptr) {
        fprintf(stderr, "Unable to create a render device.\n");
        return 1;
    }

    // The requested device is only a preference.
    const std::string &name = device->getDescription().name;
    if (!deviceName.empty() && (name != deviceName)) {
        fprintf(stderr, "Requested device \"%s\" but got \"%s\".\n", deviceName.c_str(), name.c_str());
        return 1;
    }

    std::unique_ptr<RenderCommandQueue> queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    TestContext ctx = { device.get(), queue.get(), renderInterface->getCapabilities().shaderFormat };
    printf("Device: %s\n", name.c_str());

    int failures = 0;
    for (const Test &test : Tests) {
        std::string error = test.run(ctx);
        if (error.empty()) {
            printf("PASS %s\n", test.name);
        }
        else {
            printf("FAIL %s: %s\n", test.name, error.c_str());
            failures++;
        }
    }

    printf("%d of %d tests failed\n", failures, int(std::size(Tests)));
    return (failures == 0) ? 0 : 1;
}
