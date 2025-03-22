set(EXAMPLE_NAME triangle)

# Source files
set(SOURCES
    main.cpp
)

# Create executable
add_executable(${EXAMPLE_NAME} ${SOURCES})

# Link with SDL2 library
target_link_libraries(${EXAMPLE_NAME} ${SDL2_LIBRARIES})

# Include directories
target_include_directories(${EXAMPLE_NAME} PRIVATE 
    ${SDL2_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(${EXAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 