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

# Find SDL2 only if Vulkan via SDL is enabled
if(SDL_VULKAN_ENABLED)
    find_package(SDL2 REQUIRED)
    message(STATUS "Building with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
endif()

# Print status messages
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

# Create target
add_library(plume STATIC ${PLUME_SOURCES})

# Include directories
target_include_directories(plume PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/volk
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/VulkanMemoryAllocator/include
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Vulkan-Headers/include
)

if(SDL_VULKAN_ENABLED)
    target_include_directories(plume PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

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

# Add examples
add_subdirectory(examples) 
