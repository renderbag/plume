cmake_minimum_required(VERSION 3.12)
project(plume LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ShaderCompilation module is now in examples/cmake

if(APPLE)
    enable_language(OBJC OBJCXX)
    # Disable Vulkan on macOS
    set(PLUME_ENABLE_VULKAN OFF)
    # Enable Metal on macOS
    set(PLUME_ENABLE_METAL ON CACHE BOOL "Enable Metal backend" FORCE)
endif()

# file_to_c tool is now in examples/tools

# Backend options
if(NOT DEFINED PLUME_ENABLE_VULKAN)
    option(PLUME_ENABLE_VULKAN "Enable Vulkan backend" OFF)
endif()
if(NOT DEFINED PLUME_ENABLE_METAL)
    option(PLUME_ENABLE_METAL "Enable Metal backend" OFF)
endif()
option(PLUME_ENABLE_D3D12 "Enable D3D12 backend" OFF)

# Use pkgx for SDL2 on macOS
if(APPLE)
    # Define SDL2 variables manually to avoid find_package
    set(SDL2_INCLUDE_DIRS "$ENV{HOME}/.pkgx/libsdl.org/v2.30.10/include/SDL2" "$ENV{HOME}/.pkgx/libsdl.org/v2.30.10/include")
    set(SDL2_LIBRARIES "-L$ENV{HOME}/.pkgx/libsdl.org/v2.30.10/lib -lSDL2")
    
    # Add define to use SDL2 prefix
    add_compile_definitions(SDL_MAIN_HANDLED)
else()
    # Find SDL2 on other platforms
    find_package(SDL2 REQUIRED)
endif()

# Print status messages
message(STATUS "Building with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
message(STATUS "Building with backends: Vulkan=${PLUME_ENABLE_VULKAN} Metal=${PLUME_ENABLE_METAL} D3D12=${PLUME_ENABLE_D3D12}")

# Basic source files that are always included
set(PLUME_SOURCES
    plume_render_interface.h
    plume_render_interface_types.h
    plume_render_interface_builders.h
)

# Platform-specific files
if(APPLE AND PLUME_ENABLE_METAL)
    message(STATUS "Adding Metal backend files")
    add_compile_definitions(PLUME_ENABLE_METAL)
    list(APPEND PLUME_SOURCES
        plume_metal.cpp
        plume_metal.h
        plume_apple.mm
        plume_apple.h
    )
endif()

if(WIN32 AND PLUME_ENABLE_D3D12)
    add_compile_definitions(PLUME_ENABLE_D3D12)
    list(APPEND PLUME_SOURCES
        plume_d3d12.cpp
        plume_d3d12.h
    )
endif()

if(PLUME_ENABLE_VULKAN)
    add_compile_definitions(PLUME_ENABLE_VULKAN)
    list(APPEND PLUME_SOURCES
        plume_vulkan.cpp
        plume_vulkan.h
    )
else()
    add_compile_definitions(PLUME_DISABLE_VULKAN)
endif()

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