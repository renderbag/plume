#include <SDL.h>
#include <iostream>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow(
        "Plume Triangle Example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    std::cout << "Window created with SDL" << std::endl;
    
    // Main loop
    bool quit = false;
    SDL_Event event;
    
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
        }
        
        // In a real app, we would render here
        // For this simplified example, we just sleep a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Clean up
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
} 