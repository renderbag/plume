if(PLUME_SDL2_VULKAN_ENABLED OR PLUME_SDL3_VULKAN_ENABLED)
    message(FATAL_ERROR "Plume - The tests create the Vulkan interface without a window, which requires SDL Vulkan integration to be disabled")
endif()

include(${CMAKE_SOURCE_DIR}/examples/cmake/PlumeShaders.cmake)
plume_shaders_init()

add_executable(plume_tests main.cpp)

target_link_libraries(plume_tests PRIVATE plume)

plume_compile_vertex_shader(plume_tests "${CMAKE_CURRENT_SOURCE_DIR}/shaders/color.vert.hlsl" "testColorVert" "VSMain")
plume_compile_pixel_shader(plume_tests "${CMAKE_CURRENT_SOURCE_DIR}/shaders/color.frag.hlsl" "testColorFrag" "PSMain")
plume_compile_vertex_shader(plume_tests "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fullscreen.vert.hlsl" "testFullscreenVert" "VSMain")
plume_compile_pixel_shader(plume_tests "${CMAKE_CURRENT_SOURCE_DIR}/shaders/texture.frag.hlsl" "testTextureFrag" "PSMain")

if(APPLE)
    target_link_libraries(plume_tests PRIVATE "-framework Metal -framework QuartzCore -framework CoreGraphics -framework Foundation -framework IOKit")
elseif(WIN32)
    target_link_libraries(plume_tests PRIVATE d3d12 dxgi)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(X11 REQUIRED)
    target_include_directories(plume_tests PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(plume_tests PRIVATE ${X11_LIBRARIES})
endif()

target_include_directories(plume_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

set_target_properties(plume_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
