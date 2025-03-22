#include "framework.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace plume {
namespace example {

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