set(EXAMPLE_NAME triangle)

# Source files
set(SOURCES
    main.cpp
)

# Add shader source files
if(APPLE)
    list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/triangle.metal.c)
endif()
list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/triangle_vert.spirv.c)
list(APPEND SOURCES ${CMAKE_BINARY_DIR}/shaders/triangle_frag.spirv.c)

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
    compile_metal_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.metal" "triangle")
endif()

# Compile GLSL shaders for Vulkan
compile_vertex_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert" "triangle_vert")
compile_fragment_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag" "triangle_frag") 