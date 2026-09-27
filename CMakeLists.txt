cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    # 10.15 because of our use of supportsFamily()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

project(plume)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

string(COMPARE EQUAL ${CMAKE_SYSTEM_NAME} "Linux" IS_LINUX)

# Project options
include(CMakeDependentOption)
cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF IS_LINUX OFF)
cmake_dependent_option(PLUME_D3D12_AGILITY_SDK_ENABLED "Enable D3D12 Agility SDK" OFF WIN32 OFF)
cmake_dependent_option(PLUME_APPLE_RETINA_ENABLED "Enable Apple Retina display support" ON APPLE ON)
option(PLUME_BUILD_EXAMPLES "Build example applications" OFF)
cmake_dependent_option(PLUME_XCODE_TEAM_ID "Xcode team ID for code signing (e.g., ABCDE12345)" "" APPLE OFF)

# Windows-specific definitions
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Enable SDL Vulkan support
if(PLUME_SDL_VULKAN_ENABLED)
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 REQUIRED)
    else()
        set(SDL2_INCLUDE_DIRS "$<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>")
    endif()

    message(STATUS "Plume - Building with SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS}")
endif()

# Enable D3D12 Agility SDK support
if(PLUME_D3D12_AGILITY_SDK_ENABLED)
    find_package(directx-headers CONFIG REQUIRED)
    find_package(directx12-agility CONFIG REQUIRED)
endif()

# Print status messages
message(STATUS "Plume - Building with backends: Vulkan=1 Metal=${APPLE} D3D12=${WIN32}")
message(STATUS "Plume - SDL Vulkan integration: ${PLUME_SDL_VULKAN_ENABLED}")
message(STATUS "Plume - D3D12 Agility SDK: ${PLUME_D3D12_AGILITY_SDK_ENABLED}")
message(STATUS "Plume - Building examples: ${PLUME_BUILD_EXAMPLES}")

if(APPLE AND PLUME_BUILD_EXAMPLES)
    message(STATUS "Plume - Xcode examples configured with automatic code signing")

    if(PLUME_XCODE_TEAM_ID)
        message(STATUS "Plume - Xcode Team ID: ${PLUME_XCODE_TEAM_ID}")
    else()
        message(STATUS "Plume - To set your Xcode Team ID, use: -DPLUME_XCODE_TEAM_ID=<your-team-id>")
    endif()
endif()

# Basic source files that are always included
set(PLUME_SOURCES
    plume_vulkan.cpp
    plume_vulkan.h
)

# Platform-specific files
if(APPLE)
    list(APPEND PLUME_SOURCES
        plume_metal.cpp
        plume_metal.h
        plume_apple.mm
        plume_apple.h
    )
elseif(WIN32)
    list(APPEND PLUME_SOURCES
        plume_d3d12.cpp
        plume_d3d12.h
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp
    )
endif()

# Create target
add_library(plume STATIC ${PLUME_SOURCES})

# Include directories
target_include_directories(plume PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/volk
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/Vulkan-Headers/include
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/VulkanMemoryAllocator/include
)

if(WIN32)
    target_include_directories(plume PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/D3D12MemoryAllocator/include
    )
endif()

if(PLUME_SDL_VULKAN_ENABLED)
    target_compile_definitions(plume PUBLIC PLUME_SDL_VULKAN_ENABLED)
    target_include_directories(plume PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if(PLUME_D3D12_AGILITY_SDK_ENABLED)
    target_compile_definitions(plume PUBLIC PLUME_D3D12_AGILITY_SDK_ENABLED)
    target_compile_definitions(plume PRIVATE D3D12MA_USING_DIRECTX_HEADERS)
    target_link_libraries(plume PRIVATE Microsoft::DirectX-Headers Microsoft::DirectX-Guids Microsoft::DirectX12-Agility)
endif()

# Platform-specific includes and shader compilation
if(APPLE)
    if(PLUME_APPLE_RETINA_ENABLED)
        target_compile_definitions(plume PRIVATE PLUME_APPLE_RETINA_ENABLED)
    endif()

    target_include_directories(plume PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/metal-cpp
    )
endif()

# Add examples if requested
if(PLUME_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
