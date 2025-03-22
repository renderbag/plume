#include "../framework.h"
#include <iostream>
#include <cmath>

class ClearColorExample : public plume::example::Example {
private:
    SDL_Window* m_window;
    int m_width;
    int m_height;
    float m_time = 0.0f;

public:
    void init(SDL_Window* window) override {
        m_window = window;
        
        // Get the configuration to set initial size
        auto config = getConfig();
        m_width = config.width;
        m_height = config.height;
        
        std::cout << "Clear Color example initialized\n";
    }
    
    void render() override {
        // In a real implementation, this would clear the screen with a changing color
        m_time += 0.016f; // Approximate frame time
        
        float r = (std::sin(m_time) + 1.0f) * 0.5f;
        float g = (std::sin(m_time + 2.0f) + 1.0f) * 0.5f;
        float b = (std::sin(m_time + 4.0f) + 1.0f) * 0.5f;
        
        std::cout << "Clear color: " << r << ", " << g << ", " << b << "                \r" << std::flush;
    }
    
    void resize(int width, int height) override {
        m_width = width;
        m_height = height;
        std::cout << "Resized to " << width << "x" << height << std::endl;
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle key events for this example
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_SPACE) {
                // Reset the animation time
                m_time = 0.0f;
                std::cout << "Animation reset" << std::endl;
            }
        }
    }
    
    plume::example::FrameworkConfig getConfig() const override {
        plume::example::FrameworkConfig config;
        config.title = "Plume Clear Color Example";
        config.width = 800;
        config.height = 600;
        config.vsync = true;
        config.resizable = true;
        return config;
    }
};

int main(int argc, char* argv[]) {
    auto clearColorExample = std::make_unique<ClearColorExample>();
    plume::example::run(std::move(clearColorExample));
    return 0;
} 