set(CMAKE_CXX_STANDARD 17)

# Enable SDL Vulkan integration (Linux only)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLUME_SDL2_VULKAN_ENABLED ON CACHE BOOL "Enable SDL Vulkan integration" FORCE)
endif()

# Find or fetch SDL2 (required for examples)
include(${CMAKE_SOURCE_DIR}/examples/cmake/modules/PlumeSDL2.cmake)
plume_find_sdl2()

# Initialize shader compilation
include(${CMAKE_SOURCE_DIR}/examples/cmake/PlumeShaders.cmake)
plume_shaders_init()

# Add example subdirectories
add_subdirectory(triangle)
add_subdirectory(cube)
