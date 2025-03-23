#include "framework.h"
#include <iostream>
#include <SDL.h>
#include <SDL_syswm.h>
#include <cmath>
#include <chrono>
#include <thread>

// Include the generated shader header files
#if defined(__APPLE__)
extern "C" const uint8_t clear_colorBlobMSL[];
extern "C" const uint32_t clear_colorBlobMSLSize;
#endif
extern "C" const uint8_t clear_color_vertBlobSPIRV[];
extern "C" const uint32_t clear_color_vertBlobSPIRVSize;
extern "C" const uint8_t clear_color_fragBlobSPIRV[];
extern "C" const uint32_t clear_color_fragBlobSPIRVSize;

// Include plume render interface header
#include "plume_render_interface.h"

class ClearColorExample : public plume::example::Example {
public:
    plume::example::FrameworkConfig getConfig() const override {
        plume::example::FrameworkConfig config;
        config.title = "Clear Color Example";
        config.width = 800;
        config.height = 600;
        config.resizable = true;
        return config;
    }
    
    void init(SDL_Window* window, plume::RenderInterface* renderInterface, plume::RenderWindow renderWindow) override {
        std::cout << "Initializing Clear Color Example" << std::endl;
        
        // Save window reference
        this->window = window;
        this->m_renderInterface = renderInterface;
        this->m_renderWindow = renderWindow;
        
        // Register shaders with the framework
        #if defined(__APPLE__)
        plume::example::registerMetalShader("clear_color", clear_colorBlobMSL, clear_colorBlobMSLSize);
        #endif
        plume::example::registerShader("clear_color", plume::example::ShaderType::Vertex, 
                                      clear_color_vertBlobSPIRV, clear_color_vertBlobSPIRVSize);
        plume::example::registerShader("clear_color", plume::example::ShaderType::Fragment, 
                                      clear_color_fragBlobSPIRV, clear_color_fragBlobSPIRVSize);
        
        // Initialize rendering resources
        initializeRenderResources();
        
        std::cout << "Render interface successfully initialized" << std::endl;
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle user input here
    }
    
    void resize(int width, int height) override {
        std::cout << "Resizing clear color example to " << width << "x" << height << std::endl;
        
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
            for (uint32_t i = 0; i < m_swapChain->getTextureCount(); i++) {
                const plume::RenderTexture* colorAttachment = m_swapChain->getTexture(i);
                if (!colorAttachment) {
                    std::cerr << "Failed to get swap chain texture after resize" << std::endl;
                    return;
                }
                
                plume::RenderFramebufferDesc fbDesc;
                fbDesc.colorAttachments = &colorAttachment;
                fbDesc.colorAttachmentsCount = 1;
                fbDesc.depthAttachment = nullptr;
                
                auto framebuffer = m_device->createFramebuffer(fbDesc);
                if (!framebuffer) {
                    std::cerr << "Failed to create framebuffer after resize" << std::endl;
                    return;
                }
                
                m_framebuffers.push_back(std::move(framebuffer));
            }
        }
    }
    
    void render() override {
        static int counter = 0;
        if (counter++ % 60 == 0) {
            std::cout << "Rendering frame " << counter << std::endl;
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
        
        // Clear with bright magenta color
        plume::RenderColor clearColor(1.0f, 0.0f, 1.0f, 1.0f); // Bright magenta
        m_commandList->clearColor(0, clearColor);
        
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
    
    ~ClearColorExample() override {
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
};

int main(int argc, char* argv[]) {
    auto example = std::make_unique<ClearColorExample>();
    plume::example::run(std::move(example));
    return 0;
} 