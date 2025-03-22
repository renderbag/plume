#pragma once

#include <SDL.h>
#include <memory>
#include <string>
#include <functional>

namespace plume {
namespace example {

// Framework configuration that can be specialized by each example
struct FrameworkConfig {
    const char* title = "Plume Example";
    int width = 800;
    int height = 600;
    bool vsync = true;
    bool resizable = true;
};

// Interface that each example must implement
class Example {
public:
    virtual ~Example() = default;

    // Called when the example is initialized
    virtual void init(SDL_Window* window) = 0;

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

} // namespace example
} // namespace plume 