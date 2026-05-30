# Triangle example

set(SOURCES main.cpp)

add_executable(plume_triangle ${SOURCES})

target_link_libraries(plume_triangle PRIVATE plume ${SDL2_LIBRARIES})

# Platform-specific libraries
if(APPLE)
    target_link_libraries(plume_triangle PRIVATE "-framework Metal -framework QuartzCore -framework CoreGraphics -framework Foundation")

    if(NOT IOS)
        target_link_libraries(plume_triangle PRIVATE "-framework IOKit")
    endif()

    set_target_properties(plume_triangle PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/../plume.entitlements
    )

    if(IOS)
        set_target_properties(plume_triangle PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.renderbag.plume_triangle"
            MACOSX_BUNDLE_BUNDLE_NAME "Plume Triangle"
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        )
    endif()
elseif(WIN32)
    target_link_libraries(plume_triangle PRIVATE d3d12 dxgi)

    if(EXISTS "${SDL2_BINDIR}/SDL2.dll")
        configure_file("${SDL2_BINDIR}/SDL2.dll" "${CMAKE_BINARY_DIR}/bin/SDL2.dll" COPYONLY)
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(X11 REQUIRED)
    target_include_directories(plume_triangle PUBLIC ${X11_INCLUDE_DIR} ${X11_Xrandr_INCLUDE_PATH})
    target_link_libraries(plume_triangle PRIVATE ${X11_LIBRARIES} ${X11_Xrandr_LIB})
endif()

target_include_directories(plume_triangle PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/examples
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/shaders
)

set_target_properties(plume_triangle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compile shaders (HLSL to SPIR-V/DXIL/Metal)
plume_compile_vertex_shader(plume_triangle "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert.hlsl" "triangleVert" "VSMain")
plume_compile_pixel_shader(plume_triangle "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag.hlsl" "triangleFrag" "PSMain")
