#include "framework.h"
#include <iostream>
#include <SDL.h>
#include <SDL_syswm.h>
#include <vector>

// Include the generated shader header files
#if defined(__APPLE__)
extern "C" const uint8_t triangleBlobMSL[];
extern "C" const uint32_t triangleBlobMSLSize;
#endif
extern "C" const uint8_t triangle_vertBlobSPIRV[];
extern "C" const uint32_t triangle_vertBlobSPIRVSize;
extern "C" const uint8_t triangle_fragBlobSPIRV[];
extern "C" const uint32_t triangle_fragBlobSPIRVSize;

// Include plume render interface header
#include "plume_render_interface.h"

class TriangleExample : public plume::example::Example {
public:
    plume::example::FrameworkConfig getConfig() const override {
        plume::example::FrameworkConfig config;
        config.title = "Triangle Example";
        config.width = 800;
        config.height = 600;
        config.resizable = true;
        return config;
    }
    
    void init(SDL_Window* window, plume::RenderInterface* renderInterface, plume::RenderWindow renderWindow) override {
        std::cout << "Initializing Triangle Example" << std::endl;
        
        // Save window reference
        this->window = window;
        this->m_renderInterface = renderInterface;
        this->m_renderWindow = renderWindow;
        
        // Get active backend type
        auto activeBackend = plume::example::getActiveBackendType();
        
        // Register shaders with the framework based on backend
        if (activeBackend == plume::example::RenderBackendType::Metal) {
            #if defined(__APPLE__)
            std::cout << "Registering Metal shaders" << std::endl;
            plume::example::registerMetalShader("triangle", triangleBlobMSL, triangleBlobMSLSize);
            #endif
        }
        
        // Always register SPIRV shaders for Vulkan backend or fallback
        std::cout << "Registering SPIRV shaders" << std::endl;
        plume::example::registerShader("triangle", plume::example::ShaderType::Vertex, 
                                     triangle_vertBlobSPIRV, triangle_vertBlobSPIRVSize);
        plume::example::registerShader("triangle", plume::example::ShaderType::Fragment, 
                                     triangle_fragBlobSPIRV, triangle_fragBlobSPIRVSize);
        
        // Initialize rendering resources
        initializeRenderResources();
        
        std::cout << "Triangle Example initialized" << std::endl;
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle user input here
    }
    
    void resize(int width, int height) override {
        std::cout << "Resizing triangle example to " << width << "x" << height << std::endl;
        
        // Simply resize the swapchain
        if (m_swapChain) {
            // Wait for GPU to finish all work
            if (m_commandQueue && m_fence) {
                m_commandQueue->waitForCommandFence(m_fence.get());
            }
            
            // Clear old framebuffers
            m_framebuffers.clear();
            
            // Resize the swap chain
            bool resized = m_swapChain->resize();
            if (!resized) {
                std::cerr << "Failed to resize swap chain" << std::endl;
                return;
            }
            
            // Recreate framebuffers for the resized swap chain
            createFramebuffers();
        }
    }
    
    void render() override {
        static int counter = 0;
        if (counter++ % 60 == 0) {
            // Display which backend is in use
            auto backend = plume::example::getActiveBackendType();
            std::string backendName;
            
            switch (backend) {
                case plume::example::RenderBackendType::Metal:
                    backendName = "Metal";
                    break;
                case plume::example::RenderBackendType::Vulkan:
                    backendName = "Vulkan";
                    break;
                case plume::example::RenderBackendType::Auto:
                    backendName = "Auto";
                    break;
            }
            
            std::cout << "Rendering frame " << counter << " using " << backendName << " backend" << std::endl;
        }
        
        // Acquire the next swapchain image
        uint32_t imageIndex = 0;
        m_swapChain->acquireTexture(m_acquireSemaphore.get(), &imageIndex);
        
        // Begin command recording
        m_commandList->begin();
        
        // Get the current swapchain framebuffer
        const plume::RenderFramebuffer* framebuffer = m_framebuffers[imageIndex].get();
        m_commandList->setFramebuffer(framebuffer);
        
        // Set up viewport and scissor
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        plume::RenderViewport viewport(0, 0, width, height);
        plume::RenderRect scissor(0, 0, width, height);
        m_commandList->setViewports(viewport);
        m_commandList->setScissors(scissor);
        
        // Get the current swap chain texture and transition to render target
        plume::RenderTexture* swapChainTexture = m_swapChain->getTexture(imageIndex);
        m_commandList->barriers(plume::RenderBarrierStage::GRAPHICS, 
                              plume::RenderTextureBarrier(swapChainTexture, plume::RenderTextureLayout::COLOR_WRITE));
        
        // Clear with a dark blue color
        plume::RenderColor clearColor(0.0f, 0.0f, 0.2f, 1.0f);
        m_commandList->clearColor(0, clearColor);

        // Bind the pipeline and vertex buffer
        m_commandList->setPipeline(m_pipeline.get());
        m_commandList->setVertexBuffers(0, &m_vertexBufferView, 1, &m_inputSlot);
        
        // Draw the triangle
        m_commandList->drawInstanced(3, 1, 0, 0);

        // Transition to present layout
        m_commandList->barriers(plume::RenderBarrierStage::NONE, 
                              plume::RenderTextureBarrier(swapChainTexture, plume::RenderTextureLayout::PRESENT));
        
        // End command recording
        m_commandList->end();
        
        // Submit and present
        const plume::RenderCommandList* cmdList = m_commandList.get();
        plume::RenderCommandSemaphore* waitSemaphore = m_acquireSemaphore.get();
        plume::RenderCommandSemaphore* signalSemaphore = m_releaseSemaphore.get();
        
        m_commandQueue->executeCommandLists(&cmdList, 1, 
                                           &waitSemaphore, 1, 
                                           &signalSemaphore, 1, 
                                           m_fence.get());
        
        // Present the frame
        m_swapChain->present(imageIndex, &signalSemaphore, 1);
    }
    
    ~TriangleExample() override {
        // Make sure GPU is done before cleaning up resources
        if (m_device) {
            // Wait for the GPU to finish all work
            if (m_commandQueue && m_fence) {
                m_commandQueue->waitForCommandFence(m_fence.get());
            }
        }
    }

private:
    // Rendering resources
    SDL_Window* window = nullptr;
    plume::RenderInterface* m_renderInterface = nullptr;
    plume::RenderWindow m_renderWindow = {};
    std::unique_ptr<plume::RenderDevice> m_device;
    std::unique_ptr<plume::RenderCommandQueue> m_commandQueue;
    std::unique_ptr<plume::RenderCommandList> m_commandList;
    std::unique_ptr<plume::RenderCommandFence> m_fence;
    std::unique_ptr<plume::RenderSwapChain> m_swapChain;
    std::unique_ptr<plume::RenderCommandSemaphore> m_acquireSemaphore;
    std::unique_ptr<plume::RenderCommandSemaphore> m_releaseSemaphore;
    std::vector<std::unique_ptr<plume::RenderFramebuffer>> m_framebuffers;
    
    // Pipeline and buffer resources
    std::unique_ptr<plume::RenderPipeline> m_pipeline;
    std::unique_ptr<plume::RenderPipelineLayout> m_pipelineLayout;
    std::unique_ptr<plume::RenderBuffer> m_vertexBuffer;
    plume::RenderVertexBufferView m_vertexBufferView;
    plume::RenderInputSlot m_inputSlot;

    void initializeRenderResources() {
        if (!m_renderInterface) {
            throw std::runtime_error("No render interface provided");
        }
        
        // Create device
        m_device = m_renderInterface->createDevice();
        if (!m_device) {
            throw std::runtime_error("Failed to create render device");
        }
        
        // Create command queue for graphics
        m_commandQueue = m_device->createCommandQueue(plume::RenderCommandListType::DIRECT);
        if (!m_commandQueue) {
            throw std::runtime_error("Failed to create command queue");
        }
        
        // Create a command fence
        m_fence = m_device->createCommandFence();
        
        // Create a swap chain for the window using the render window from init
        m_swapChain = m_commandQueue->createSwapChain(m_renderWindow, 2, plume::RenderFormat::B8G8R8A8_UNORM, 2);
        if (!m_swapChain) {
            throw std::runtime_error("Failed to create swap chain");
        }
        
        // Create command list
        m_commandList = m_commandQueue->createCommandList();
        if (!m_commandList) {
            throw std::runtime_error("Failed to create command list");
        }
        
        // Create acquire semaphore for swap chain synchronization
        m_acquireSemaphore = m_device->createCommandSemaphore();
        
        // Create release semaphore for swap chain synchronization
        m_releaseSemaphore = m_device->createCommandSemaphore();
        
        // Create framebuffers for each swap chain image
        createFramebuffers();
        
        // Create the graphics pipeline
        createPipeline();
        
        // Create the vertex buffer with triangle data
        createVertexBuffer();
    }
    
    void createFramebuffers() {
        // Create framebuffers for each swap chain image
        m_framebuffers.clear();
        for (uint32_t i = 0; i < m_swapChain->getTextureCount(); i++) {
            const plume::RenderTexture* colorAttachment = m_swapChain->getTexture(i);
            if (!colorAttachment) {
                throw std::runtime_error("Failed to get swap chain texture");
            }
            
            plume::RenderFramebufferDesc fbDesc;
            fbDesc.colorAttachments = &colorAttachment;
            fbDesc.colorAttachmentsCount = 1;
            fbDesc.depthAttachment = nullptr;
            
            auto framebuffer = m_device->createFramebuffer(fbDesc);
            if (!framebuffer) {
                throw std::runtime_error("Failed to create framebuffer");
            }
            
            m_framebuffers.push_back(std::move(framebuffer));
        }
    }
    
    void createPipeline() {
        // Create a pipeline layout (without any descriptor sets or push constants)
        plume::RenderPipelineLayoutDesc layoutDesc;
        layoutDesc.descriptorSetDescs = nullptr;
        layoutDesc.descriptorSetDescsCount = 0;
        layoutDesc.pushConstantRanges = nullptr;
        layoutDesc.pushConstantRangesCount = 0;
        
        m_pipelineLayout = m_device->createPipelineLayout(layoutDesc);
        if (!m_pipelineLayout) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
        
        // Load vertex and fragment shaders
        plume::example::ShaderData vertexShaderData = plume::example::loadShader("triangle", plume::example::ShaderType::Vertex);
        plume::example::ShaderData fragmentShaderData = plume::example::loadShader("triangle", plume::example::ShaderType::Fragment);
        
        if (!vertexShaderData.isValid() || !fragmentShaderData.isValid()) {
            throw std::runtime_error("Failed to load shaders");
        }
        
        // Get the shader format from the render interface
        plume::RenderShaderFormat shaderFormat = m_renderInterface->getCapabilities().shaderFormat;
        
        // Create shader objects
        std::unique_ptr<plume::RenderShader> vertexShader;
        std::unique_ptr<plume::RenderShader> fragmentShader;
        
        // Different entry point names depending on shader format
        if (shaderFormat == plume::RenderShaderFormat::METAL) {
            vertexShader = m_device->createShader(vertexShaderData.data, vertexShaderData.size, "vertexMain", shaderFormat);
            fragmentShader = m_device->createShader(fragmentShaderData.data, fragmentShaderData.size, "fragmentMain", shaderFormat);
        } else {
            vertexShader = m_device->createShader(vertexShaderData.data, vertexShaderData.size, "main", shaderFormat);
            fragmentShader = m_device->createShader(fragmentShaderData.data, fragmentShaderData.size, "main", shaderFormat);
        }
        
        if (!vertexShader || !fragmentShader) {
            throw std::runtime_error("Failed to create shaders");
        }
        
        // Define vertex input layout
        // The vertex format has position (vec3) and color (vec4)
        m_inputSlot = plume::RenderInputSlot(0, sizeof(float) * 7); // 3 floats for position + 4 floats for color
        
        std::vector<plume::RenderInputElement> inputElements = {
            plume::RenderInputElement("POSITION", 0, 0, plume::RenderFormat::R32G32B32_FLOAT, 0, 0),
            plume::RenderInputElement("COLOR", 0, 1, plume::RenderFormat::R32G32B32A32_FLOAT, 0, sizeof(float) * 3)
        };
        
        // Create graphics pipeline
        plume::RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.inputSlots = &m_inputSlot;
        pipelineDesc.inputSlotsCount = 1;
        pipelineDesc.inputElements = inputElements.data();
        pipelineDesc.inputElementsCount = static_cast<uint32_t>(inputElements.size());
        pipelineDesc.pipelineLayout = m_pipelineLayout.get();
        pipelineDesc.vertexShader = vertexShader.get();
        pipelineDesc.pixelShader = fragmentShader.get();
        pipelineDesc.renderTargetFormat[0] = plume::RenderFormat::B8G8R8A8_UNORM;
        pipelineDesc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.primitiveTopology = plume::RenderPrimitiveTopology::TRIANGLE_LIST;
        
        m_pipeline = m_device->createGraphicsPipeline(pipelineDesc);
        if (!m_pipeline) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
    }
    
    void createVertexBuffer() {
        // Define triangle vertices: position (x, y, z) and color (r, g, b, a)
        const float vertices[] = {
             0.0f,  0.5f, 0.0f,    1.0f, 0.0f, 0.0f, 1.0f, // Top vertex (red)
            -0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f, 1.0f, // Bottom left vertex (green)
             0.5f, -0.5f, 0.0f,    0.0f, 0.0f, 1.0f, 1.0f  // Bottom right vertex (blue)
        };
        
        // Create vertex buffer
        m_vertexBuffer = m_device->createBuffer(plume::RenderBufferDesc::VertexBuffer(
            sizeof(vertices), plume::RenderHeapType::UPLOAD));
        
        if (!m_vertexBuffer) {
            throw std::runtime_error("Failed to create vertex buffer");
        }
        
        // Map buffer and copy vertex data
        void* bufferData = m_vertexBuffer->map();
        std::memcpy(bufferData, vertices, sizeof(vertices));
        m_vertexBuffer->unmap();
        
        // Create vertex buffer view
        m_vertexBufferView = plume::RenderVertexBufferView(m_vertexBuffer.get(), sizeof(vertices));
    }
};

int main(int argc, char* argv[]) {
    auto example = std::make_unique<TriangleExample>();
    
    // Parse command line arguments for backend selection
    if (argc > 1) {
        std::string arg(argv[1]);
        if (arg == "--metal") {
            std::cout << "Backend requested: Metal" << std::endl;
            auto config = example->getConfig();
            config.backendType = plume::example::RenderBackendType::Metal;
            
            // Create a new example with the updated config
            class ConfigurableTriangleExample : public TriangleExample {
            public:
                ConfigurableTriangleExample(const plume::example::FrameworkConfig& config) : m_config(config) {}
                
                plume::example::FrameworkConfig getConfig() const override {
                    return m_config;
                }
                
            private:
                plume::example::FrameworkConfig m_config;
            };
            
            example = std::make_unique<ConfigurableTriangleExample>(config);
        } else if (arg == "--vulkan") {
            std::cout << "Backend requested: Vulkan" << std::endl;
            auto config = example->getConfig();
            config.backendType = plume::example::RenderBackendType::Vulkan;
            
            // Create a new example with the updated config
            class ConfigurableTriangleExample : public TriangleExample {
            public:
                ConfigurableTriangleExample(const plume::example::FrameworkConfig& config) : m_config(config) {}
                
                plume::example::FrameworkConfig getConfig() const override {
                    return m_config;
                }
                
            private:
                plume::example::FrameworkConfig m_config;
            };
            
            example = std::make_unique<ConfigurableTriangleExample>(config);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --metal     Force Metal backend (macOS only)" << std::endl;
            std::cout << "  --vulkan    Force Vulkan backend" << std::endl;
            std::cout << "  --help      Show this help message" << std::endl;
            return 0;
        }
    }
    
    plume::example::run(std::move(example));
    return 0;
} 