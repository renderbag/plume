//
// plume - Cube Texture Example
//
// Demonstrates cube texture creation and sampling
//

#include "plume_render_interface.h"

#include <cassert>
#include <cstring>
#include <chrono>
#include <functional>
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>
#include <thread>
#include <vector>
#include <iostream>
#include <cmath>

#ifdef _WIN64
#include "shaders/cubeVert.hlsl.dxil.h"
#include "shaders/cubeFrag.hlsl.dxil.h"
#endif
#include "shaders/cubeVert.hlsl.spirv.h"
#include "shaders/cubeFrag.hlsl.spirv.h"
#ifdef __APPLE__
#include "shaders/cubeVert.hlsl.metal.h"
#include "shaders/cubeFrag.hlsl.metal.h"
#endif

namespace plume {
    extern std::unique_ptr<RenderInterface> CreateMetalInterface();
    extern std::unique_ptr<RenderInterface> CreateD3D12Interface();
    #if PLUME_SDL_VULKAN_ENABLED
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow sdlWindow);
    #else
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
    #endif

    static const uint32_t BufferCount = 2;
    static const RenderFormat SwapchainFormat = RenderFormat::B8G8R8A8_UNORM;
    static const uint32_t CubeMapSize = 256;

    // Toggle between TextureTypeCube (1 cube = 6 faces) and TextureTypeCubeArray (2 cubes = 12 faces)
    // This validates that both code paths in mapTextureType work correctly
    static const bool UseCubeArray = false;
    static const uint32_t NumCubes = UseCubeArray ? 2 : 1;
    static const uint32_t NumFaces = NumCubes * 6;

    // Simple 4x4 matrix for inverse view-projection
    struct Mat4 {
        float m[16];

        static Mat4 identity() {
            Mat4 result = {};
            result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
            return result;
        }

        static Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
            Mat4 result = {};
            float tanHalfFov = std::tan(fovY / 2.0f);
            result.m[0] = 1.0f / (aspect * tanHalfFov);
            result.m[5] = 1.0f / tanHalfFov;
            result.m[10] = -(farZ + nearZ) / (farZ - nearZ);
            result.m[11] = -1.0f;
            result.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
            return result;
        }

        static Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
                           float centerX, float centerY, float centerZ,
                           float upX, float upY, float upZ) {
            // Forward vector
            float fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
            float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
            fx /= flen; fy /= flen; fz /= flen;

            // Right vector (cross product of forward and up)
            float rx = fy * upZ - fz * upY;
            float ry = fz * upX - fx * upZ;
            float rz = fx * upY - fy * upX;
            float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
            rx /= rlen; ry /= rlen; rz /= rlen;

            // True up vector
            float ux = ry * fz - rz * fy;
            float uy = rz * fx - rx * fz;
            float uz = rx * fy - ry * fx;

            Mat4 result = {};
            result.m[0] = rx;  result.m[4] = ry;  result.m[8]  = rz;
            result.m[1] = ux;  result.m[5] = uy;  result.m[9]  = uz;
            result.m[2] = -fx; result.m[6] = -fy; result.m[10] = -fz;
            result.m[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
            result.m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
            result.m[14] = (fx*eyeX + fy*eyeY + fz*eyeZ);
            result.m[15] = 1.0f;
            return result;
        }

        Mat4 operator*(const Mat4& other) const {
            Mat4 result = {};
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    result.m[i + j*4] = 0;
                    for (int k = 0; k < 4; k++) {
                        result.m[i + j*4] += m[i + k*4] * other.m[k + j*4];
                    }
                }
            }
            return result;
        }

        Mat4 inverse() const {
            Mat4 inv = {};
            float det;

            inv.m[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
            inv.m[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
            inv.m[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
            inv.m[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
            inv.m[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
            inv.m[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
            inv.m[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
            inv.m[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
            inv.m[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
            inv.m[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
            inv.m[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
            inv.m[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
            inv.m[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
            inv.m[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
            inv.m[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
            inv.m[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

            det = m[0]*inv.m[0] + m[1]*inv.m[4] + m[2]*inv.m[8] + m[3]*inv.m[12];
            if (det == 0) return identity();

            det = 1.0f / det;
            for (int i = 0; i < 16; i++) inv.m[i] *= det;
            return inv;
        }
    };

    struct CubeContext {
        const RenderInterface *m_renderInterface = nullptr;
        std::string m_apiName;
        RenderWindow m_renderWindow = {};
        std::unique_ptr<RenderDevice> m_device;
        std::unique_ptr<RenderCommandQueue> m_commandQueue;
        std::unique_ptr<RenderCommandList> m_commandList;
        std::unique_ptr<RenderCommandFence> m_fence;
        std::unique_ptr<RenderSwapChain> m_swapChain;
        std::unique_ptr<RenderCommandSemaphore> m_acquireSemaphore;
        std::vector<std::unique_ptr<RenderCommandSemaphore>> m_releaseSemaphores;
        std::vector<std::unique_ptr<RenderFramebuffer>> m_framebuffers;

        // Cube texture resources
        std::unique_ptr<RenderPipeline> m_pipeline;
        std::unique_ptr<RenderPipelineLayout> m_pipelineLayout;
        std::unique_ptr<RenderTexture> m_cubeTexture;
        std::unique_ptr<RenderTextureView> m_cubeTextureView;
        std::unique_ptr<RenderSampler> m_sampler;
        std::unique_ptr<RenderDescriptorSet> m_descriptorSet;
        std::unique_ptr<RenderBuffer> m_constantBuffer;

        // Camera
        float m_cameraYaw = 0.0f;
        float m_cameraPitch = 0.0f;
        float m_time = 0.0f;
    };

    // Generate a procedural color for each cube face
    // cubeIndex: which cube in the array (0 or 1)
    // faceIndex: which face of the cube (0-5: +X, -X, +Y, -Y, +Z, -Z)
    void generateCubeFaceData(uint32_t cubeIndex, uint32_t faceIndex, uint32_t size, std::vector<uint8_t>& data) {
        data.resize(size * size * 4);

        // Different color schemes for each cube
        // Cube 0: warm colors, Cube 1: cool colors (inverted/shifted)
        float baseColors[2][6][3] = {
            // Cube 0: Original warm scheme
            {
                {1.0f, 0.3f, 0.3f},  // +X: Red
                {0.3f, 1.0f, 0.3f},  // -X: Green
                {0.3f, 0.3f, 1.0f},  // +Y: Blue (sky)
                {0.4f, 0.25f, 0.1f}, // -Y: Brown (ground)
                {1.0f, 1.0f, 0.3f},  // +Z: Yellow
                {1.0f, 0.3f, 1.0f},  // -Z: Magenta
            },
            // Cube 1: Cool/inverted scheme (for cube array testing)
            {
                {0.3f, 1.0f, 1.0f},  // +X: Cyan
                {1.0f, 0.3f, 0.5f},  // -X: Pink
                {0.5f, 0.8f, 0.3f},  // +Y: Lime
                {0.2f, 0.2f, 0.3f},  // -Y: Dark blue-gray
                {0.8f, 0.5f, 0.2f},  // +Z: Orange
                {0.4f, 0.3f, 0.8f},  // -Z: Purple
            }
        };

        const auto& colors = baseColors[cubeIndex % 2];

        for (uint32_t y = 0; y < size; y++) {
            for (uint32_t x = 0; x < size; x++) {
                uint32_t idx = (y * size + x) * 4;

                float brightness = 1.0f;

                data[idx + 0] = (uint8_t)(colors[faceIndex][0] * brightness * 255.0f);
                data[idx + 1] = (uint8_t)(colors[faceIndex][1] * brightness * 255.0f);
                data[idx + 2] = (uint8_t)(colors[faceIndex][2] * brightness * 255.0f);
                data[idx + 3] = 255;
            }
        }
    }

    void createFramebuffers(CubeContext& ctx) {
        ctx.m_framebuffers.clear();

        for (uint32_t i = 0; i < ctx.m_swapChain->getTextureCount(); i++) {
            const RenderTexture* colorAttachment = ctx.m_swapChain->getTexture(i);

            RenderFramebufferDesc fbDesc;
            fbDesc.colorAttachments = &colorAttachment;
            fbDesc.colorAttachmentsCount = 1;
            fbDesc.depthAttachment = nullptr;

            auto framebuffer = ctx.m_device->createFramebuffer(fbDesc);
            ctx.m_framebuffers.push_back(std::move(framebuffer));
        }
    }

    void createCubeTexture(CubeContext& ctx) {
        const char* textureType = UseCubeArray ? "cube array" : "cube";
        std::cout << "Creating " << textureType << " texture (" << CubeMapSize << "x" << CubeMapSize
                  << " per face, " << NumCubes << " cube(s), " << NumFaces << " total faces)..." << std::endl;

        // Create cube texture (or cube array if NumFaces > 6)
        RenderTextureDesc cubeDesc = RenderTextureDesc::Texture(
            RenderTextureDimension::TEXTURE_2D,
            CubeMapSize, CubeMapSize, 1,
            1,         // mip levels
            NumFaces,  // 6 faces per cube * NumCubes
            RenderFormat::R8G8B8A8_UNORM,
            RenderTextureFlag::CUBE
        );

        ctx.m_cubeTexture = ctx.m_device->createTexture(cubeDesc);
        assert(ctx.m_cubeTexture != nullptr && "Failed to create cube texture");

        // Create a cube texture view
        // When using cube array, view the SECOND cube (index 6-11) to prove the array works
        RenderTextureViewDesc viewDesc = RenderTextureViewDesc::TextureCube(RenderFormat::R8G8B8A8_UNORM);
        viewDesc.arrayIndex = UseCubeArray ? 6 : 0;  // Start at cube 1 if array, else cube 0
        viewDesc.arraySize = 6;  // View one cube (6 faces)
        std::cout << "  Creating texture view starting at face " << viewDesc.arrayIndex
                  << " (cube " << (viewDesc.arrayIndex / 6) << ")" << std::endl;
        ctx.m_cubeTextureView = ctx.m_cubeTexture->createTextureView(viewDesc);
        assert(ctx.m_cubeTextureView != nullptr && "Failed to create cube texture view");

        // Create staging buffer and upload face data
        size_t faceSize = CubeMapSize * CubeMapSize * 4;
        auto stagingBuffer = ctx.m_device->createBuffer(
            RenderBufferDesc::UploadBuffer(faceSize * NumFaces)
        );

        uint8_t* mappedData = static_cast<uint8_t*>(stagingBuffer->map());

        for (uint32_t face = 0; face < NumFaces; face++) {
            uint32_t cubeIndex = face / 6;
            uint32_t faceIndex = face % 6;
            std::vector<uint8_t> faceData;
            generateCubeFaceData(cubeIndex, faceIndex, CubeMapSize, faceData);
            std::memcpy(mappedData + face * faceSize, faceData.data(), faceSize);
        }

        stagingBuffer->unmap();

        // Upload to GPU
        ctx.m_commandList->begin();

        ctx.m_commandList->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(ctx.m_cubeTexture.get(), RenderTextureLayout::COPY_DEST));

        for (uint32_t face = 0; face < NumFaces; face++) {
            uint32_t cubeIndex = face / 6;
            uint32_t faceIndex = face % 6;
            std::cout << "  Uploading cube " << cubeIndex << " face " << faceIndex
                      << " (slice=" << face << ", offset=" << (face * faceSize) << ")" << std::endl;

            RenderTextureCopyLocation srcLocation = RenderTextureCopyLocation::PlacedFootprint(
                stagingBuffer.get(),
                RenderFormat::R8G8B8A8_UNORM,
                CubeMapSize, CubeMapSize, 1,
                CubeMapSize,  // rowWidth is in pixels, not bytes
                face * faceSize
            );

            RenderTextureCopyLocation dstLocation = RenderTextureCopyLocation::Subresource(
                ctx.m_cubeTexture.get(),
                0,     // mipLevel
                face   // arrayIndex (each face is a different array slice)
            );

            ctx.m_commandList->copyTextureRegion(dstLocation, srcLocation, 0, 0, 0, nullptr);
        }

        ctx.m_commandList->barriers(RenderBarrierStage::GRAPHICS,
            RenderTextureBarrier(ctx.m_cubeTexture.get(), RenderTextureLayout::SHADER_READ));

        ctx.m_commandList->end();

        const RenderCommandList* cmdList = ctx.m_commandList.get();
        ctx.m_commandQueue->executeCommandLists(&cmdList, 1, nullptr, 0, nullptr, 0, ctx.m_fence.get());
        ctx.m_commandQueue->waitForCommandFence(ctx.m_fence.get());

        std::cout << "  Cube texture created and uploaded successfully!" << std::endl;
    }

    void createPipeline(CubeContext& ctx) {
        // Create sampler
        RenderSamplerDesc samplerDesc;
        samplerDesc.minFilter = RenderFilter::LINEAR;
        samplerDesc.magFilter = RenderFilter::LINEAR;
        samplerDesc.addressU = RenderTextureAddressMode::CLAMP;
        samplerDesc.addressV = RenderTextureAddressMode::CLAMP;
        samplerDesc.addressW = RenderTextureAddressMode::CLAMP;
        ctx.m_sampler = ctx.m_device->createSampler(samplerDesc);

        // Create constant buffer for inverse view-projection matrix
        ctx.m_constantBuffer = ctx.m_device->createBuffer(
            RenderBufferDesc::UploadBuffer(sizeof(Mat4))
        );

        // Create descriptor set layout and set
        RenderDescriptorRange ranges[2] = {
            RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 0, 1),  // binding 0
            RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 1, 1)   // binding 1
        };
        RenderDescriptorSetDesc descSetDesc(ranges, 2);

        ctx.m_descriptorSet = ctx.m_device->createDescriptorSet(descSetDesc);
        ctx.m_descriptorSet->setTexture(0, ctx.m_cubeTexture.get(), RenderTextureLayout::SHADER_READ, ctx.m_cubeTextureView.get());
        ctx.m_descriptorSet->setSampler(1, ctx.m_sampler.get());

        // Create pipeline layout
        RenderPipelineLayoutDesc layoutDesc;
        layoutDesc.descriptorSetDescs = &descSetDesc;
        layoutDesc.descriptorSetDescsCount = 1;

        RenderPushConstantRange pushConstantRange;
        pushConstantRange.size = sizeof(Mat4);
        pushConstantRange.stageFlags = RenderShaderStageFlag::VERTEX;
        layoutDesc.pushConstantRanges = &pushConstantRange;
        layoutDesc.pushConstantRangesCount = 1;

        ctx.m_pipelineLayout = ctx.m_device->createPipelineLayout(layoutDesc);

        // Create shaders
        RenderShaderFormat shaderFormat = ctx.m_renderInterface->getCapabilities().shaderFormat;

        std::unique_ptr<RenderShader> vertexShader;
        std::unique_ptr<RenderShader> fragmentShader;

        switch (shaderFormat) {
#ifdef __APPLE__
            case RenderShaderFormat::METAL:
                vertexShader = ctx.m_device->createShader(cubeVertBlobMSL, sizeof(cubeVertBlobMSL), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(cubeFragBlobMSL, sizeof(cubeFragBlobMSL), "PSMain", shaderFormat);
                break;
#endif
            case RenderShaderFormat::SPIRV:
                vertexShader = ctx.m_device->createShader(cubeVertBlobSPIRV, sizeof(cubeVertBlobSPIRV), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(cubeFragBlobSPIRV, sizeof(cubeFragBlobSPIRV), "PSMain", shaderFormat);
                break;
#ifdef _WIN64
            case RenderShaderFormat::DXIL:
                vertexShader = ctx.m_device->createShader(cubeVertBlobDXIL, sizeof(cubeVertBlobDXIL), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(cubeFragBlobDXIL, sizeof(cubeFragBlobDXIL), "PSMain", shaderFormat);
                break;
#endif
            default:
                assert(false && "Unknown shader format");
        }

        // Create graphics pipeline (no vertex input - fullscreen triangle)
        RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.pipelineLayout = ctx.m_pipelineLayout.get();
        pipelineDesc.vertexShader = vertexShader.get();
        pipelineDesc.pixelShader = fragmentShader.get();
        pipelineDesc.renderTargetFormat[0] = SwapchainFormat;
        pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;

        ctx.m_pipeline = ctx.m_device->createGraphicsPipeline(pipelineDesc);
    }

    void initializeRenderResources(CubeContext& ctx, RenderInterface* renderInterface) {
        ctx.m_device = renderInterface->createDevice();
        ctx.m_commandQueue = ctx.m_device->createCommandQueue(RenderCommandListType::DIRECT);
        ctx.m_fence = ctx.m_device->createCommandFence();
        ctx.m_swapChain = ctx.m_commandQueue->createSwapChain(RenderSwapChainDesc(ctx.m_renderWindow, SwapchainFormat, BufferCount));
        ctx.m_swapChain->resize();
        ctx.m_commandList = ctx.m_commandQueue->createCommandList();
        ctx.m_acquireSemaphore = ctx.m_device->createCommandSemaphore();

        createFramebuffers(ctx);
        createCubeTexture(ctx);
        createPipeline(ctx);
    }

    void createContext(CubeContext& ctx, RenderInterface* renderInterface, RenderWindow window, const std::string& apiName) {
        ctx.m_renderInterface = renderInterface;
        ctx.m_renderWindow = window;
        ctx.m_apiName = apiName;
        initializeRenderResources(ctx, renderInterface);
    }

    void resize(CubeContext& ctx, int width, int height) {
        std::cout << "Resizing cube example to " << width << "x" << height << std::endl;

        if (ctx.m_swapChain) {
            ctx.m_framebuffers.clear();
            ctx.m_swapChain->resize();
            createFramebuffers(ctx);
        }
    }

    void render(CubeContext& ctx) {
        static int counter = 0;
        if (counter++ % 60 == 0) {
            std::cout << "Rendering frame " << counter << " using " << ctx.m_apiName << " backend" << std::endl;
        }

        // Update time for camera rotation
        ctx.m_time += 0.016f;  // ~60fps
        ctx.m_cameraYaw = ctx.m_time * 0.3f;
        ctx.m_cameraPitch = std::sin(ctx.m_time * 0.2f) * 0.3f;

        // Acquire swapchain image
        uint32_t imageIndex = 0;
        ctx.m_swapChain->acquireTexture(ctx.m_acquireSemaphore.get(), &imageIndex);

        // Calculate inverse view-projection matrix
        const uint32_t width = ctx.m_swapChain->getWidth();
        const uint32_t height = ctx.m_swapChain->getHeight();
        float aspect = (float)width / (float)height;

        // Camera looking from origin with yaw/pitch rotation
        float camX = std::cos(ctx.m_cameraPitch) * std::sin(ctx.m_cameraYaw);
        float camY = std::sin(ctx.m_cameraPitch);
        float camZ = std::cos(ctx.m_cameraPitch) * std::cos(ctx.m_cameraYaw);

        Mat4 view = Mat4::lookAt(0, 0, 0, camX, camY, camZ, 0, 1, 0);
        Mat4 proj = Mat4::perspective(1.2f, aspect, 0.1f, 100.0f);
        Mat4 viewProj = proj * view;
        Mat4 invViewProj = viewProj.inverse();

        // Update constant buffer
        void* mapped = ctx.m_constantBuffer->map();
        std::memcpy(mapped, invViewProj.m, sizeof(Mat4));
        ctx.m_constantBuffer->unmap();

        // Begin command recording
        ctx.m_commandList->begin();

        RenderTexture* swapChainTexture = ctx.m_swapChain->getTexture(imageIndex);
        ctx.m_commandList->barriers(RenderBarrierStage::GRAPHICS,
            RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));

        const RenderFramebuffer* framebuffer = ctx.m_framebuffers[imageIndex].get();
        ctx.m_commandList->setFramebuffer(framebuffer);

        const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
        const RenderRect scissor(0, 0, width, height);
        ctx.m_commandList->setViewports(viewport);
        ctx.m_commandList->setScissors(scissor);

        // Draw cube texture
        ctx.m_commandList->setGraphicsPipelineLayout(ctx.m_pipelineLayout.get());
        ctx.m_commandList->setPipeline(ctx.m_pipeline.get());
        ctx.m_commandList->setGraphicsDescriptorSet(ctx.m_descriptorSet.get(), 0);
        ctx.m_commandList->setGraphicsPushConstants(0, invViewProj.m);

        // Draw fullscreen triangle (3 vertices, no vertex buffer)
        ctx.m_commandList->drawInstanced(3, 1, 0, 0);

        ctx.m_commandList->barriers(RenderBarrierStage::NONE,
            RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));

        ctx.m_commandList->end();

        // Create semaphores if needed
        while (ctx.m_releaseSemaphores.size() < ctx.m_swapChain->getTextureCount()) {
            ctx.m_releaseSemaphores.emplace_back(ctx.m_device->createCommandSemaphore());
        }

        // Submit and present
        const RenderCommandList* cmdList = ctx.m_commandList.get();
        RenderCommandSemaphore* waitSemaphore = ctx.m_acquireSemaphore.get();
        RenderCommandSemaphore* signalSemaphore = ctx.m_releaseSemaphores[imageIndex].get();

        ctx.m_commandQueue->executeCommandLists(&cmdList, 1, &waitSemaphore, 1, &signalSemaphore, 1, ctx.m_fence.get());
        ctx.m_swapChain->present(imageIndex, &signalSemaphore, 1);
        ctx.m_commandQueue->waitForCommandFence(ctx.m_fence.get());
    }

    void CubeExample(RenderInterface* renderInterface, SDL_Window* window, const std::string& apiName) {
        std::string windowTitle = "Plume Cube Texture Example (" + apiName + ")";
        SDL_SetWindowTitle(window, windowTitle.c_str());

        CubeContext ctx;
#if PLUME_SDL_VULKAN_ENABLED
        createContext(ctx, renderInterface, window, apiName);
#elif defined(__linux__)
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        createContext(ctx, renderInterface, { wmInfo.info.x11.display, wmInfo.info.x11.window }, apiName);
#elif defined(__APPLE__)
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        SDL_MetalView view = SDL_Metal_CreateView(window);
#if TARGET_OS_OSX
        auto windowHandle = wmInfo.info.cocoa.window;
#else
        auto windowHandle = wmInfo.info.uikit.window;
#endif
        createContext(ctx, renderInterface, { windowHandle, SDL_Metal_GetLayer(view) }, apiName);
#elif defined(WIN32)
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        createContext(ctx, renderInterface, { wmInfo.info.win.window }, apiName);
#endif

        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            resize(ctx, event.window.data1, event.window.data2);
                        }
                        break;
                }
            }

            render(ctx);
        }

        // Cleanup
        uint32_t imageIndex = 0;
        if (!ctx.m_swapChain->isEmpty() && ctx.m_swapChain->acquireTexture(ctx.m_acquireSemaphore.get(), &imageIndex)) {
            RenderTexture* swapChainTexture = ctx.m_swapChain->getTexture(imageIndex);
            ctx.m_commandList->begin();
            ctx.m_commandList->barriers(RenderBarrierStage::NONE,
                RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
            ctx.m_commandList->end();

            const RenderCommandList* cmdList = ctx.m_commandList.get();
            RenderCommandSemaphore* waitSemaphore = ctx.m_acquireSemaphore.get();
            ctx.m_commandQueue->executeCommandLists(&cmdList, 1, &waitSemaphore, 1, nullptr, 0, ctx.m_fence.get());
            ctx.m_commandQueue->waitForCommandFence(ctx.m_fence.get());
        }

#if defined(__APPLE__)
        SDL_Metal_DestroyView(view);
#endif
    }
}

std::unique_ptr<plume::RenderInterface> CreateRenderInterface(SDL_Window* window, std::string& apiName) {
    const bool useVulkan = false;
#if defined(_WIN32)
    if (useVulkan) {
        apiName = "Vulkan";
#if PLUME_SDL_VULKAN_ENABLED
        return plume::CreateVulkanInterface(window);
#else
        return plume::CreateVulkanInterface();
#endif
    } else {
        apiName = "D3D12";
        return plume::CreateD3D12Interface();
    }
#elif defined(__APPLE__)
    if (useVulkan) {
        apiName = "Vulkan";
#if PLUME_SDL_VULKAN_ENABLED
        return plume::CreateVulkanInterface(window);
#else
        return plume::CreateVulkanInterface();
#endif
    } else {
        apiName = "Metal";
        return plume::CreateMetalInterface();
    }
#else
    apiName = "Vulkan";
#if PLUME_SDL_VULKAN_ENABLED
    return plume::CreateVulkanInterface(window);
#else
    return plume::CreateVulkanInterface();
#endif
#endif
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    uint32_t flags = SDL_WINDOW_RESIZABLE;
#if PLUME_SDL_VULKAN_ENABLED
    flags |= SDL_WINDOW_VULKAN;
#elif defined(__APPLE__)
    flags |= SDL_WINDOW_METAL;
#endif

    SDL_Window* window = SDL_CreateWindow("Plume Cube Texture Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    std::string apiName = "Unknown";
    auto renderInterface = CreateRenderInterface(window, apiName);
    if (!renderInterface) {
        fprintf(stderr, "Failed to create render interface\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    plume::CubeExample(renderInterface.get(), window, apiName);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
