#pragma once

#include <SDL.h>
#if SDL_VULKAN_ENABLED
#include <SDL_vulkan.h>
#endif
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>
#include "plume_render_interface.h"

namespace plume {
namespace example {

// Render backend type
enum class RenderBackendType {
    Auto,       // Choose the best backend for the platform (Metal on macOS, Vulkan elsewhere)
    Metal,      // Force Metal backend (macOS only)
    Vulkan      // Force Vulkan backend
};

// Framework configuration that can be specialized by each example
struct FrameworkConfig {
    const char* title = "Plume Example";
    int width = 800;
    int height = 600;
    bool vsync = true;
    bool resizable = true;
    RenderBackendType backendType = RenderBackendType::Auto; // Default to automatic selection
};

// Shader data structure
struct ShaderData {
    const uint8_t* data = nullptr;
    uint32_t size = 0;
    
    bool isValid() const { return data != nullptr && size > 0; }
};

// Shader type
enum class ShaderType {
    Vertex,
    Fragment,
    Compute
};

// Interface that each example must implement
class Example {
public:
    virtual ~Example() = default;

    // Called when the example is initialized
    virtual void init(SDL_Window* window, plume::RenderInterface* renderInterface, plume::RenderWindow renderWindow) = 0;

    // Called when the example should be rendered
    virtual void render() = 0;

    // Called when the window is resized
    virtual void resize(int width, int height) = 0;

    // Called to handle events
    virtual void handleEvent(const SDL_Event& event) = 0;

    // Get the configuration for this example
    virtual FrameworkConfig getConfig() const = 0;
};

// Run an example with the framework
void run(std::unique_ptr<Example> example);

// Load a shader from compiled binary
ShaderData loadShader(const char* name, ShaderType type);

// Register a shader with the framework
void registerShader(const char* name, ShaderType type, const uint8_t* data, uint32_t size);

// Register a Metal shader with the framework (macOS only)
void registerMetalShader(const char* name, const uint8_t* data, uint32_t size);

// Get the current active backend type
RenderBackendType getActiveBackendType();

} // namespace example
} // namespace plume 