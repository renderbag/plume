#include "framework.h"
#include <iostream>

// Include the generated shader header files
#if defined(__APPLE__)
extern "C" const uint8_t clear_colorBlobMSL[];
extern "C" const uint32_t clear_colorBlobMSLSize;
#endif
extern "C" const uint8_t clear_color_vertBlobSPIRV[];
extern "C" const uint32_t clear_color_vertBlobSPIRVSize;
extern "C" const uint8_t clear_color_fragBlobSPIRV[];
extern "C" const uint32_t clear_color_fragBlobSPIRVSize;

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
    
    void init(SDL_Window* window) override {
        std::cout << "Initializing Clear Color Example" << std::endl;
        
        // Register shaders with the framework
        #if defined(__APPLE__)
        plume::example::registerMetalShader("clear_color", clear_colorBlobMSL, clear_colorBlobMSLSize);
        #endif
        plume::example::registerShader("clear_color", plume::example::ShaderType::Vertex, 
                                      clear_color_vertBlobSPIRV, clear_color_vertBlobSPIRVSize);
        plume::example::registerShader("clear_color", plume::example::ShaderType::Fragment, 
                                      clear_color_fragBlobSPIRV, clear_color_fragBlobSPIRVSize);
        
        // Load shaders
        auto vertexShader = plume::example::loadShader("clear_color", plume::example::ShaderType::Vertex);
        auto fragmentShader = plume::example::loadShader("clear_color", plume::example::ShaderType::Fragment);
        
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            throw std::runtime_error("Failed to load shaders");
        }
        
        std::cout << "Vertex shader loaded: " << vertexShader.size << " bytes" << std::endl;
        std::cout << "Fragment shader loaded: " << fragmentShader.size << " bytes" << std::endl;
        
        // Initialize parameters and rendering resources
        // This would initialize the renderer with the loaded shaders and other resources
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle user input here
    }
    
    void resize(int width, int height) override {
        std::cout << "Resizing clear color example to " << width << "x" << height << std::endl;
        // Update viewport and projection matrices if needed
    }
    
    void render() override {
        // Update time and parameters
        // Render the clear color effect
    }
};

int main(int argc, char* argv[]) {
    auto example = std::make_unique<ClearColorExample>();
    plume::example::run(std::move(example));
    return 0;
} 