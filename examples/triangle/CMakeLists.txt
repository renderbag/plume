set(EXAMPLE_NAME triangle)

# Source files
set(SOURCES
    main.cpp
)

# Create executable
add_executable(${EXAMPLE_NAME} ${SOURCES})

# Link with framework and SDL2 libraries
target_link_libraries(${EXAMPLE_NAME} plume_example_framework ${SDL2_LIBRARIES})

# Include directories
target_include_directories(${EXAMPLE_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/examples
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/shaders  # Include shaders directory
)

# Set output directory
set_target_properties(${EXAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create the shaders directory
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")

# Compile Metal shader if on Apple platform - this outputs triangleBlobMSL as the main.cpp expects
if(APPLE)
    compile_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.metal" "vertex" "triangle")
endif()

# Compile HLSL shaders for Vulkan - these output triangle_vertBlobSPIRV and triangle_fragBlobSPIRV
compile_vertex_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.hlsl" "triangle_vert")
compile_fragment_shader(${EXAMPLE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.hlsl" "triangle_frag") 