cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

project(plume)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

# D3D11-only build - remove all other backends
set(PLUME_SOURCES
    plume_d3d11.cpp
    plume_d3d11.h
)

# Create target
add_library(plume STATIC ${PLUME_SOURCES})

# Include directories
target_include_directories(plume PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Windows specific
if(WIN32)
    target_include_directories(plume PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(plume PRIVATE
        d3d11.lib
        dxgi.lib
        dxguid.lib
    )
endif()

# Print status
message(STATUS "Plume - Building with D3D11 backend only")