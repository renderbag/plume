set(EXAMPLE_NAME clear_color)

# Source files
set(SOURCES
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../framework.cpp
)

# Create executable
add_executable(${EXAMPLE_NAME} ${SOURCES})

# Link with SDL2 library
target_link_libraries(${EXAMPLE_NAME} ${SDL2_LIBRARIES})

# Include directories
target_include_directories(${EXAMPLE_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Set output directory
set_target_properties(${EXAMPLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 