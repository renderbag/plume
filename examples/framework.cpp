#include "framework.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <fstream>

#include <SDL_vulkan.h>
#ifdef __APPLE__
#include <SDL_metal.h>
#include <SDL_syswm.h>
#endif

// Function prototype for creating the Metal interface on Apple platforms
namespace plume {
    extern std::unique_ptr<RenderInterface> CreateMetalInterface();
    #if SDL_VULKAN_ENABLED
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow sdlWindow);
    #else
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
    #endif
}

namespace plume {
namespace example {

// Map of shader names to data
using ShaderMap = std::map<std::string, ShaderData>;

// Initialize empty shader maps
static ShaderMap s_metalShaders;
static ShaderMap s_spirvVertexShaders;
static ShaderMap s_spirvFragmentShaders;

// Track the active backend type
static RenderBackendType s_activeBackendType = RenderBackendType::Auto;

// Get the currently active backend type
RenderBackendType getActiveBackendType() {
    return s_activeBackendType;
}

// Register a shader with the framework
void registerShader(const char* name, ShaderType type, const uint8_t* data, uint32_t size) {
    if (data == nullptr || size == 0) {
        std::cerr << "Attempted to register invalid shader: " << name << std::endl;
        return;
    }
    
    std::string shaderName(name);
    
    switch (type) {
    case ShaderType::Vertex:
        s_spirvVertexShaders[shaderName] = {data, size};
        break;
    case ShaderType::Fragment:
        s_spirvFragmentShaders[shaderName] = {data, size};
        break;
    case ShaderType::Compute:
        // Add support for compute shaders later
        break;
    }
}

// Register a Metal shader with the framework (macOS only)
void registerMetalShader(const char* name, const uint8_t* data, uint32_t size) {
    if (data == nullptr || size == 0) {
        std::cerr << "Attempted to register invalid Metal shader: " << name << std::endl;
        return;
    }
    
    s_metalShaders[std::string(name)] = {data, size};
}

// Load a shader based on platform and type
ShaderData loadShader(const char* name, ShaderType type) {
    std::string shaderName(name);
    
#if defined(__APPLE__)
    // On macOS, check if Metal backend is active
    if (s_activeBackendType == RenderBackendType::Auto || s_activeBackendType == RenderBackendType::Metal) {
        // Use Metal shaders
        auto metalIter = s_metalShaders.find(shaderName);
        if (metalIter != s_metalShaders.end()) {
            std::cout << "Found Metal shader for: " << shaderName << std::endl;
            return metalIter->second;
        }
    }
#endif
    
    // Use SPIRV shaders for Vulkan backend or if Metal shader not found
    if (type == ShaderType::Vertex) {
        auto iter = s_spirvVertexShaders.find(shaderName);
        if (iter != s_spirvVertexShaders.end()) {
            return iter->second;
        }
    } else if (type == ShaderType::Fragment) {
        auto iter = s_spirvFragmentShaders.find(shaderName);
        if (iter != s_spirvFragmentShaders.end()) {
            return iter->second;
        }
    }
    
    // Return an invalid shader if not found
    std::cerr << "Shader not found: " << name << std::endl;
    return {};
}

void run(std::unique_ptr<Example> example) {
    // Get configuration from the example
    auto config = example->getConfig();
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Create SDL window
    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (config.resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    // Set up window flags based on backend preference
    bool useMetalBackend = false;
    bool useVulkanBackend = false;
    
    // Determine which backend to use based on config and platform support
    switch (config.backendType) {
        case RenderBackendType::Auto:
            // Use platform default (Metal on macOS, Vulkan elsewhere)
            #ifdef __APPLE__
            useMetalBackend = true;
            #else
            useVulkanBackend = true;
            #endif
            break;
            
        case RenderBackendType::Metal:
            #ifdef __APPLE__
            useMetalBackend = true;
            #else
            std::cerr << "Metal backend requested but not supported on this platform" << std::endl;
            useVulkanBackend = true; // Fall back to Vulkan
            #endif
            break;
            
        case RenderBackendType::Vulkan:
            useVulkanBackend = true;
            break;
    }

    // Update window flags based on selected backend
    #ifdef __APPLE__
    if (useMetalBackend) {
        windowFlags |= SDL_WINDOW_METAL;
    }
    #endif

    #if SDL_VULKAN_ENABLED
    if (useVulkanBackend) {
        windowFlags |= SDL_WINDOW_VULKAN;
    }
    #endif
    
    SDL_Window* window = SDL_CreateWindow(
        config.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.width, config.height,
        windowFlags
    );
    
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }
    
    // Create the render interface and platform-specific window
    std::unique_ptr<plume::RenderInterface> renderInterface;
    plume::RenderWindow renderWindow = {};

    // Create renderWindow based on platform
    #ifdef __APPLE__
        SDL_MetalView metalView = SDL_Metal_CreateView(window);
        if (!metalView) {
            std::cerr << "Failed to create Metal view: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
        
        // Get the Cocoa window and CAMetalLayer
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
            std::cerr << "Failed to get window info: " << SDL_GetError() << std::endl;
            SDL_Metal_DestroyView(metalView);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
        
        // Set up the render window for Metal
        renderWindow.window = wmInfo.info.cocoa.window;
        renderWindow.view = SDL_Metal_GetLayer(metalView);
    #elif defined(__linux__) || defined(__ANDROID__)  
        #ifdef SDL_VULKAN_ENABLED
        renderWindow = window;
        renderInterface = plume::CreateVulkanInterface(renderWindow);
        #else
        renderWindow.window = wmInfo.info.x11.display;
        renderWindow.view = wmInfo.info.x11.window;
        #endif
    #endif
    
    // Create backend based on selection
    if (useMetalBackend) {
        #ifdef __APPLE__
        s_activeBackendType = RenderBackendType::Metal;
        std::cout << "Using Metal backend" << std::endl;
        
        // Create the Metal interface
        renderInterface = plume::CreateMetalInterface();
        if (!renderInterface) {
            std::cerr << "Failed to create Metal interface" << std::endl;
            SDL_Metal_DestroyView(metalView);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
        #endif
    } else if (useVulkanBackend) {
        s_activeBackendType = RenderBackendType::Vulkan;
        std::cout << "Using Vulkan backend" << std::endl;        

        renderInterface = plume::CreateVulkanInterface();
        if (!renderInterface) {
            std::cerr << "Failed to create Vulkan interface" << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }
    } else {
        std::cerr << "No suitable rendering backend available" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Initialize the example
    try {
        example->init(window, renderInterface.get(), renderWindow);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize example: " << e.what() << std::endl;
        #if defined(__APPLE__) && (useMetalBackend)
        SDL_Metal_DestroyView(metalView);
        #endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Main loop
    bool quit = false;
    SDL_Event event;
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int width = event.window.data1;
                    int height = event.window.data2;
                    example->resize(width, height);
                }
            }
            
            // Let the example handle the event
            example->handleEvent(event);
        }
        
        // Render the example
        example->render();
        
        // Simple frame rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Clean up
    #if defined(__APPLE__) && (useMetalBackend)
    SDL_Metal_DestroyView(metalView);
    #endif
    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace example
} // namespace plume 