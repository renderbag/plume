#include "../framework.h"
#include <iostream>

class TriangleExample : public plume::example::Example {
private:
    SDL_Window* m_window;
    int m_width;
    int m_height;

public:
    void init(SDL_Window* window) override {
        m_window = window;
        
        // Get the configuration to set initial size
        auto config = getConfig();
        m_width = config.width;
        m_height = config.height;
        
        std::cout << "Triangle example initialized\n";
    }
    
    void render() override {
        // For now, just print that we're rendering
        std::cout << "Rendering triangle frame...\r" << std::flush;
        
        // In a full implementation, this would draw a triangle using the render interface
    }
    
    void resize(int width, int height) override {
        m_width = width;
        m_height = height;
        std::cout << "Resized to " << width << "x" << height << std::endl;
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle any additional events specific to this example
    }
    
    plume::example::FrameworkConfig getConfig() const override {
        plume::example::FrameworkConfig config;
        config.title = "Plume Triangle Example";
        config.width = 800;
        config.height = 600;
        config.vsync = true;
        config.resizable = true;
        return config;
    }
};

int main(int argc, char* argv[]) {
    auto triangleExample = std::make_unique<TriangleExample>();
    plume::example::run(std::move(triangleExample));
    return 0;
} 