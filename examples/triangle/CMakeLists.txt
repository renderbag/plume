set(EXAMPLE_NAME triangle)

# Source files
set(SOURCES
    main.cpp
)

# Create executable
add_executable(${EXAMPLE_NAME} ${SOURCES})

# Link with framework and platform-specific libraries
target_link_libraries(${EXAMPLE_NAME} plume_example_framework ${SDL2_LIBRARIES})

# Platform-specific libraries
if(APPLE)
    target_link_libraries(${EXAMPLE_NAME} "-framework Metal -framework QuartzCore -framework CoreGraphics -framework Foundation -framework IOKit")
elseif(WIN32)
    target_link_libraries(${EXAMPLE_NAME} d3d12 dxgi)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(X11 REQUIRED)
    target_include_directories(${EXAMPLE_NAME} PUBLIC ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
    target_link_libraries(${EXAMPLE_NAME} ${X11_LIBRARIES} ${X11_Xrandr_LIB})
endif()

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