#include "framework.h"
#include <iostream>

// Include the generated shader header files
#if defined(__APPLE__)
extern "C" const uint8_t triangleBlobMSL[];
extern "C" const uint32_t triangleBlobMSLSize;
#endif
extern "C" const uint8_t triangle_vertBlobSPIRV[];
extern "C" const uint32_t triangle_vertBlobSPIRVSize;
extern "C" const uint8_t triangle_fragBlobSPIRV[];
extern "C" const uint32_t triangle_fragBlobSPIRVSize;

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
    
    void init(SDL_Window* window) override {
        std::cout << "Initializing Triangle Example" << std::endl;
        
        // Register shaders with the framework
        #if defined(__APPLE__)
        plume::example::registerMetalShader("triangle", triangleBlobMSL, triangleBlobMSLSize);
        #endif
        plume::example::registerShader("triangle", plume::example::ShaderType::Vertex, 
                                      triangle_vertBlobSPIRV, triangle_vertBlobSPIRVSize);
        plume::example::registerShader("triangle", plume::example::ShaderType::Fragment, 
                                      triangle_fragBlobSPIRV, triangle_fragBlobSPIRVSize);
        
        // Load shaders
        auto vertexShader = plume::example::loadShader("triangle", plume::example::ShaderType::Vertex);
        auto fragmentShader = plume::example::loadShader("triangle", plume::example::ShaderType::Fragment);
        
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            throw std::runtime_error("Failed to load shaders");
        }
        
        std::cout << "Vertex shader loaded: " << vertexShader.size << " bytes" << std::endl;
        std::cout << "Fragment shader loaded: " << fragmentShader.size << " bytes" << std::endl;
        
        // Initialize rendering resources
        // This would initialize the renderer with the loaded shaders and other resources
    }
    
    void handleEvent(const SDL_Event& event) override {
        // Handle user input here
    }
    
    void resize(int width, int height) override {
        std::cout << "Resizing triangle example to " << width << "x" << height << std::endl;
        // Update viewport and projection matrices if needed
    }
    
    void render() override {
        // Render the triangle
    }
};

int main(int argc, char* argv[]) {
    auto example = std::make_unique<TriangleExample>();
    plume::example::run(std::move(example));
    return 0;
} 