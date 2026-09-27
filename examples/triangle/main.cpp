//
// plume
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
#include <random>
#include <vector>
#include <iostream>

#ifdef _WIN64
#include "shaders/triangleVert.hlsl.dxil.h"
#include "shaders/triangleFrag.hlsl.dxil.h"
#endif
#include "shaders/triangleVert.hlsl.spirv.h"
#include "shaders/triangleFrag.hlsl.spirv.h"
#ifdef __APPLE__
#include "shaders/triangleVert.hlsl.metal.h"
#include "shaders/triangleFrag.hlsl.metal.h"
#endif

// Function prototype for creating the Metal interface on Apple platforms
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

    struct TestContext {
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
        std::unique_ptr<RenderCommandFence> m_commandFence;
        std::vector<std::unique_ptr<RenderFramebuffer>> m_framebuffers;

        // Pipeline and buffer resources
        std::unique_ptr<RenderPipeline> m_pipeline;
        std::unique_ptr<RenderPipelineLayout> m_pipelineLayout;
        std::unique_ptr<RenderBuffer> m_vertexBuffer;
        RenderVertexBufferView m_vertexBufferView;
        RenderInputSlot m_inputSlot;
    };

    // MARK: - Helpers

    void createFramebuffers(TestContext& ctx) {
        // Create framebuffers for each swap chain image
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

    void createPipeline(TestContext& ctx) {
        // Create a pipeline layout (without any descriptor sets or push constants)
        RenderPipelineLayoutDesc layoutDesc;
        layoutDesc.allowInputLayout = true;

        ctx.m_pipelineLayout = ctx.m_device->createPipelineLayout(layoutDesc);

        // Get the shader format from the render interface
        RenderShaderFormat shaderFormat = ctx.m_renderInterface->getCapabilities().shaderFormat;

        // Create shader objects
        std::unique_ptr<RenderShader> vertexShader;
        std::unique_ptr<RenderShader> fragmentShader;

        // Different entry point names depending on shader format
        switch (shaderFormat) {
#ifdef __APPLE__
            case RenderShaderFormat::METAL:
                vertexShader = ctx.m_device->createShader(triangleVertBlobMSL, sizeof(triangleVertBlobMSL), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(triangleFragBlobMSL, sizeof(triangleFragBlobMSL), "PSMain", shaderFormat);
                break;
#endif
            case RenderShaderFormat::SPIRV:
                vertexShader = ctx.m_device->createShader(triangleVertBlobSPIRV, sizeof(triangleVertBlobSPIRV), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(triangleFragBlobSPIRV, sizeof(triangleFragBlobSPIRV), "PSMain", shaderFormat);
                break;
#ifdef _WIN64
            case RenderShaderFormat::DXIL:
                vertexShader = ctx.m_device->createShader(triangleVertBlobDXIL, sizeof(triangleVertBlobDXIL), "VSMain", shaderFormat);
                fragmentShader = ctx.m_device->createShader(triangleFragBlobDXIL, sizeof(triangleFragBlobDXIL), "PSMain", shaderFormat);
                break;
#endif
            default:
                assert(false && "Unknown shader format");
        }

        // Define vertex input layout
        // The vertex format has position (vec3) and color (vec4)
        ctx.m_inputSlot = RenderInputSlot(0, sizeof(float) * 7); // 3 floats for position + 4 floats for color

        std::vector<RenderInputElement> inputElements = {
            RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32B32_FLOAT, 0, 0),
            RenderInputElement("COLOR", 0, 1, RenderFormat::R32G32B32A32_FLOAT, 0, sizeof(float) * 3)
        };

        // Create graphics pipeline
        RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.inputSlots = &ctx.m_inputSlot;
        pipelineDesc.inputSlotsCount = 1;
        pipelineDesc.inputElements = inputElements.data();
        pipelineDesc.inputElementsCount = static_cast<uint32_t>(inputElements.size());
        pipelineDesc.pipelineLayout = ctx.m_pipelineLayout.get();
        pipelineDesc.vertexShader = vertexShader.get();
        pipelineDesc.pixelShader = fragmentShader.get();
        pipelineDesc.renderTargetFormat[0] = RenderFormat::B8G8R8A8_UNORM;
        pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;

        ctx.m_pipeline = ctx.m_device->createGraphicsPipeline(pipelineDesc);
    }

    void createVertexBuffer(TestContext& ctx) {
        // Define triangle vertices: position (x, y, z) and color (r, g, b, a)
        const float vertices[] = {
            0.0f,  0.5f, 0.0f,    1.0f, 0.0f, 0.0f, 1.0f, // Top vertex (red)
            -0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f, 1.0f, // Bottom left vertex (green)
            0.5f, -0.5f, 0.0f,    0.0f, 0.0f, 1.0f, 1.0f  // Bottom right vertex (blue)
        };

        // Create vertex buffer
        ctx.m_vertexBuffer = ctx.m_device->createBuffer(RenderBufferDesc::VertexBuffer(sizeof(vertices), RenderHeapType::UPLOAD));

        // Map buffer and copy vertex data
        void* bufferData = ctx.m_vertexBuffer->map();
        std::memcpy(bufferData, vertices, sizeof(vertices));
        ctx.m_vertexBuffer->unmap();

        // Create vertex buffer view
        ctx.m_vertexBufferView = RenderVertexBufferView(ctx.m_vertexBuffer.get(), sizeof(vertices));
    }

    static void initializeRenderResources(TestContext& ctx, RenderInterface* renderInterface) {
        // Create device
        ctx.m_device = renderInterface->createDevice();

        // Create command queue for graphics
        ctx.m_commandQueue = ctx.m_device->createCommandQueue(RenderCommandListType::DIRECT);

        // Create a command fence
        ctx.m_fence = ctx.m_device->createCommandFence();

        // Create a swap chain for the window using the render window from init
        ctx.m_swapChain = ctx.m_commandQueue->createSwapChain(RenderSwapChainDesc(ctx.m_renderWindow, SwapchainFormat, BufferCount));

        // Explicitly resize the swapchain to create the textures
        ctx.m_swapChain->resize();

        // Create command list
        ctx.m_commandList = ctx.m_commandQueue->createCommandList();

        // Create acquire semaphore for swap chain synchronization
        ctx.m_acquireSemaphore = ctx.m_device->createCommandSemaphore();

        // Create command fence for synchronization
        ctx.m_commandFence = ctx.m_device->createCommandFence();

        // Create framebuffers for each swap chain image
        createFramebuffers(ctx);

        // Create the graphics pipeline
        createPipeline(ctx);

        // Create the vertex buffer with triangle data
        createVertexBuffer(ctx);
    }


    // MARK: - Lifecycle Methods

    static void createContext(TestContext& ctx, RenderInterface* renderInterface, RenderWindow window, const std::string &apiName) {
        ctx.m_renderInterface = renderInterface;
        ctx.m_renderWindow = window;
        ctx.m_apiName = apiName;

        initializeRenderResources(ctx, renderInterface);
    }

    static void resize(TestContext& ctx, int width, int height) {
        std::cout << "Resizing triangle example to " << width << "x" << height << std::endl;

        // Simply resize the swapchain
        if (ctx.m_swapChain) {
            // Clear old framebuffers
            ctx.m_framebuffers.clear();

            // Resize the swap chain
            bool resized = ctx.m_swapChain->resize();
            if (!resized) {
                std::cerr << "Failed to resize swap chain" << std::endl;
                return;
            }

            // Recreate framebuffers for the resized swap chain
            createFramebuffers(ctx);
        }
    }

    static void render(TestContext& ctx) {
        static int counter = 0;
        if (counter++ % 60 == 0) {
            std::cout << "Rendering frame " << counter << " using " << ctx.m_apiName << " backend" << std::endl;
        }

        // Acquire the next swapchain image
        uint32_t imageIndex = 0;
        ctx.m_swapChain->acquireTexture(ctx.m_acquireSemaphore.get(), &imageIndex);

        // Begin command recording
        ctx.m_commandList->begin();

        // Get the current swap chain texture and transition to render target
        RenderTexture *swapChainTexture = ctx.m_swapChain->getTexture(imageIndex);
        ctx.m_commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));

        // Get the current swapchain framebuffer
        const RenderFramebuffer* framebuffer = ctx.m_framebuffers[imageIndex].get();
        ctx.m_commandList->setFramebuffer(framebuffer);

        // Set up viewport and scissor
        const uint32_t width = ctx.m_swapChain->getWidth();
        const uint32_t height = ctx.m_swapChain->getHeight();
        const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
        const RenderRect scissor(0, 0, width, height);

        ctx.m_commandList->setViewports(viewport);
        ctx.m_commandList->setScissors(scissor);

        // Clear with a dark blue color
        RenderColor clearColor(0.0f, 0.0f, 0.2f, 1.0f);
        ctx.m_commandList->clearColor(0, clearColor);

        // Bind the pipeline and vertex buffer
        ctx.m_commandList->setGraphicsPipelineLayout(ctx.m_pipelineLayout.get());
        ctx.m_commandList->setPipeline(ctx.m_pipeline.get());
        ctx.m_commandList->setVertexBuffers(0, &ctx.m_vertexBufferView, 1, &ctx.m_inputSlot);

        // Draw the triangle
        ctx.m_commandList->drawInstanced(3, 1, 0, 0);

        // Transition to present layout
        ctx.m_commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));

        // End command recording
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

        // Present the frame
        ctx.m_swapChain->present(imageIndex, &signalSemaphore, 1);
        ctx.m_commandQueue->waitForCommandFence(ctx.m_fence.get());
    }

    void RenderInterfaceTest(RenderInterface* renderInterface, SDL_Window* window, const std::string &apiName) {
        std::string windowTitle = "Plume Example (" + apiName + ")";
        SDL_SetWindowTitle(window, windowTitle.c_str());

        TestContext ctx;
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
                            int width = event.window.data1;
                            int height = event.window.data2;

                            resize(ctx, width, height);
                        }
                        break;
                }
            }

            render(ctx);
        }

        // Transition the active swap chain render target out of the present state to avoid live references to the resource
        uint32_t imageIndex = 0;
        if (!ctx.m_swapChain->isEmpty() && ctx.m_swapChain->acquireTexture(ctx.m_acquireSemaphore.get(), &imageIndex)) {
            RenderTexture *swapChainTexture = ctx.m_swapChain->getTexture(imageIndex);
            ctx.m_commandList->begin();
            ctx.m_commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
            ctx.m_commandList->end();

            const RenderCommandList *cmdList = ctx.m_commandList.get();
            RenderCommandSemaphore *waitSemaphore = ctx.m_acquireSemaphore.get();
            ctx.m_commandQueue->executeCommandLists(&cmdList, 1, &waitSemaphore, 1, nullptr, 0, ctx.m_fence.get());
            ctx.m_commandQueue->waitForCommandFence(ctx.m_fence.get());
        }

#if defined(__APPLE__)
        SDL_Metal_DestroyView(view);
#endif
    }
}

std::unique_ptr<plume::RenderInterface> CreateRenderInterface(SDL_Window* window, std::string &apiName) {
    const bool useVulkan = false;
#if defined(_WIN32)
    if (useVulkan) {
        apiName = "Vulkan";
#if PLUME_SDL_VULKAN_ENABLED
        return plume::CreateVulkanInterface(window);
#else
        return plume::CreateVulkanInterface();
#endif
    }
    else {
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
    }
    else {
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

    SDL_Window* window = SDL_CreateWindow("Plume Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
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

    plume::RenderInterfaceTest(renderInterface.get(), window, apiName);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}