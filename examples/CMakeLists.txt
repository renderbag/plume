# Enable SDL Vulkan integration
set_option(SDL_VULKAN_ENABLED ON)

# Find SDL2 (required for examples)
find_package(SDL2 REQUIRED)

# Framework sources
set(FRAMEWORK_SOURCES
    framework.cpp
    framework.h
)

# Add tools directory first (needed for shader compilation)
add_subdirectory(tools)

# Include custom ShaderCompilation module
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ShaderCompilation)

# Create framework library
add_library(plume_example_framework STATIC ${FRAMEWORK_SOURCES})
target_link_libraries(plume_example_framework plume)
target_include_directories(plume_example_framework PUBLIC 
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Add examples source directory to include paths
target_include_directories(plume_example_framework PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create shaders directory for the examples
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Set up a custom command to copy the shader files to the bin directory
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders ${CMAKE_BINARY_DIR}/bin/shaders
    COMMENT "Copying shader files to bin directory"
)

# Add examples in order
add_subdirectory(triangle)

# Make examples depend on the shader copying
add_dependencies(triangle copy_shaders) 
