cmake_minimum_required(VERSION 3.12)
project(plume LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    enable_language(OBJC OBJCXX)
    # Always enable Metal on macOS
    add_compile_definitions(PLUME_ENABLE_METAL)
endif()

# Enable SDL Vulkan support
option(SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF)
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND SDL_VULKAN_ENABLED)
    add_compile_definitions("SDL_VULKAN_ENABLED")
endif()

# Find SDL2 on all platforms
find_package(SDL2 REQUIRED)

# Print status messages
message(STATUS "Building with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message(STATUS "Building with backends: Vulkan=ON Metal=${APPLE} D3D12=${WIN32}")
message(STATUS "SDL Vulkan integration: ${SDL_VULKAN_ENABLED}")

# Basic source files that are always included
set(PLUME_SOURCES
    plume_render_interface.h
    plume_render_interface_types.h
    plume_render_interface_builders.h
)

# Platform-specific files
if(APPLE)
    message(STATUS "Adding Metal backend files")
    list(APPEND PLUME_SOURCES
        plume_metal.cpp
        plume_metal.h
        plume_apple.mm
        plume_apple.h
    )
endif()

if (WIN32)
    list(APPEND PLUME_SOURCES
        plume_d3d12.cpp
        plume_d3d12.h
    )
endif()

# Always include Vulkan backend files
list(APPEND PLUME_SOURCES
    plume_vulkan.cpp
    plume_vulkan.h
)

# Add a dummy source file if no backend is enabled to avoid CMake errors
if(NOT PLUME_SOURCES)
    file(WRITE ${CMAKE_BINARY_DIR}/dummy.cpp "// Empty file to ensure library has at least one source file\n")
    list(APPEND PLUME_SOURCES ${CMAKE_BINARY_DIR}/dummy.cpp)
endif()

# Create target
add_library(plume ${PLUME_SOURCES})

# Include directories
target_include_directories(plume PUBLIC
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/volk
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/VulkanMemoryAllocator/include
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Vulkan-Headers/include
)

# Platform-specific includes
if(APPLE)
    target_include_directories(plume PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/metal-cpp
    )
endif()

# Platform-specific libraries
if(APPLE)
    target_link_libraries(plume "-framework Metal -framework QuartzCore -framework CoreGraphics -framework Foundation -framework IOKit")
endif()

target_link_libraries(plume ${SDL2_LIBRARIES})

# Add examples
add_subdirectory(examples) 