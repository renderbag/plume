set(EXAMPLE_NAME clear_color)

# Source files
set(SOURCES
    main.cpp
)

# Add shader source files when on Apple platforms
if(APPLE)
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/clear_color.metal.c)
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/clear_color_vert.spirv.c)
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/clear_color_frag.spirv.c)
endif()

# Create executable
add_executable(${EXAMPLE_NAME} ${SOURCES})

# Link with framework and SDL2 libraries
target_link_libraries(${EXAMPLE_NAME} plume_example_framework)

# Include directories
target_include_directories(${EXAMPLE_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/examples
    ${SDL2_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(${EXAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compile shaders
if(APPLE)
    # Compile Metal shader
    compile_metal_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/clear_color.metal" "clear_color")
endif()

# Compile GLSL shaders for Vulkan
compile_vertex_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/clear_color.vert" "clear_color_vert")
compile_fragment_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/clear_color.frag" "clear_color_frag") 