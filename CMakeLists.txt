cmake_minimum_required(VERSION 3.20)
project(plume LANGUAGES C CXX OBJC OBJCXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# macOS specific settings
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
endif()

# Options
option(PLUME_BUILD_EXAMPLES "Build examples for Plume" ON)
option(PLUME_STATIC "Build Plume as a static library" OFF)
option(PLUME_ENABLE_VULKAN "Enable Vulkan backend" ON)

# Build settings for static library
if(PLUME_STATIC)
    set(PLUME_LIBRARY_TYPE STATIC)
else()
    set(PLUME_LIBRARY_TYPE SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Platform-specific preprocessor definitions
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Set up the compiler options
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Use /W4 for clang-cl, as -Wall maps to MSVC's /Wall which maps to clang's -Weverything
    if(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra)
    endif()
    add_compile_options(-Wno-nullability-completeness -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)
endif()

# Function to generate Metal Shader Library on macOS
function(compile_metal_shader TARGET_NAME SHADER_FILE OUTPUT_NAME)
    if(APPLE)
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.metallib
            COMMAND xcrun -sdk macosx metal -c ${SHADER_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.ir
            COMMAND xcrun -sdk macosx metallib ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.ir -o ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.metallib
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling Metal shader ${SHADER_FILE}"
        )
        target_sources(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_NAME}.metallib)
    endif()
endfunction()

# Check for contrib folder
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contrib")
    message(FATAL_ERROR "contrib directory not found. Make sure it exists at the root of the project.")
endif()

# Find required dependencies
find_package(SDL2 REQUIRED)

# Define source files from existing files in the repository
set(PLUME_SOURCES "")

# Add Vulkan support
if(PLUME_ENABLE_VULKAN)
    add_compile_definitions(PLUME_ENABLE_VULKAN)
    list(APPEND PLUME_SOURCES plume_vulkan.cpp)
    
    # On Windows we bundle Vulkan headers, on other platforms we need the SDK
    if(NOT WIN32)
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND)
            message(STATUS "Vulkan SDK found")
            include_directories(${Vulkan_INCLUDE_DIRS})
        else()
            # On macOS, Vulkan is optional as we can use Metal
            if(APPLE)
                message(WARNING "Vulkan SDK not found, but continuing with Metal support only")
                add_compile_definitions(PLUME_DISABLE_VULKAN)
                list(REMOVE_ITEM PLUME_SOURCES plume_vulkan.cpp)
            else()
                message(FATAL_ERROR "Vulkan SDK is required on Linux platforms")
            endif()
        endif()
    endif()
endif()

# Add platform-specific files
if(APPLE)
    add_compile_definitions(PLUME_ENABLE_METAL)
    list(APPEND PLUME_SOURCES
        plume_metal.cpp
        plume_apple.mm
    )
endif()

if(WIN32)
    add_compile_definitions(PLUME_ENABLE_D3D12)
    list(APPEND PLUME_SOURCES
        plume_d3d12.cpp
    )
endif()

# Include directories
include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific includes
if(APPLE)
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/metal-cpp
    )
endif()

# Create the Plume library
add_library(plume ${PLUME_LIBRARY_TYPE} ${PLUME_SOURCES})

# Set output name
set_target_properties(plume PROPERTIES 
    OUTPUT_NAME "plume"
    PREFIX ""
)

# Platform-specific compile options
if(APPLE)
    target_compile_options(plume PRIVATE -fobjc-arc)
endif()

# Link with platform-specific libraries
target_link_libraries(plume ${SDL2_LIBRARIES})

if(APPLE)
    target_link_libraries(plume "-framework Metal -framework QuartzCore -framework CoreGraphics -framework Foundation -framework IOKit")
endif()

if(WIN32)
    target_link_libraries(plume
        D3D12
        DXGI
    )
endif()

if(PLUME_ENABLE_VULKAN AND Vulkan_FOUND AND NOT WIN32)
    target_link_libraries(plume ${Vulkan_LIBRARIES})
endif()

# Build examples if enabled
if(PLUME_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install targets
install(TARGETS plume
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
) 