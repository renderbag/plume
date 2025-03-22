#include "framework.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <fstream>

namespace plume {
namespace example {

// Map of shader names to data
using ShaderMap = std::map<std::string, ShaderData>;

// Initialize empty shader maps
static ShaderMap s_metalShaders;
static ShaderMap s_spirvVertexShaders;
static ShaderMap s_spirvFragmentShaders;

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
    // On macOS, prefer Metal shaders
    auto metalIter = s_metalShaders.find(shaderName);
    if (metalIter != s_metalShaders.end()) {
        std::cout << "Found Metal shader for: " << shaderName << std::endl;
        return metalIter->second;
    }
#endif
    
    // Fall back to SPIRV shaders
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
    
    // Initialize the example
    try {
        example->init(window);
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize example: " << e.what() << std::endl;
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
    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace example
} // namespace plume 