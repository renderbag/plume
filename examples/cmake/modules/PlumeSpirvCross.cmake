# PlumeSpirvCross.cmake
# Fetches and builds SPIRV-Cross from source, then builds our spirv_cross_msl tool

include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/PlumeHostTool.cmake)

# Build the spirv_cross_msl tool, fetching and compiling SPIRV-Cross if not provided
function(plume_fetch_spirv_cross)
    if(TARGET plume_spirv_cross_msl)
        return()
    endif()

    set(SPIRV_CROSS_MSL_SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/spirv_cross_msl.cpp")

    if(NOT EXISTS "${SPIRV_CROSS_MSL_SOURCE}")
        message(FATAL_ERROR "plume spirv_cross_msl.cpp not found at ${SPIRV_CROSS_MSL_SOURCE}")
    endif()

    if(IOS)
        message(STATUS "Plume - Building plume_spirv_cross_msl as HOST tool")

        set(HOST_PROJECT_DIR "${CMAKE_BINARY_DIR}/host-tools/spirv_cross_msl-src")
        set(HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/host-tools/spirv_cross_msl-build")
        set(HOST_INSTALL_DIR "${CMAKE_BINARY_DIR}/host-tools/install")

        set(_host_spirv_cross_cmake [=[
cmake_minimum_required(VERSION 3.16)
project(plume_host_spirv_cross_msl LANGUAGES CXX)

include(FetchContent)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG vulkan-sdk-1.4.335.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spirv_cross)

add_executable(plume_spirv_cross_msl "@SPIRV_CROSS_MSL_SOURCE@")
target_include_directories(plume_spirv_cross_msl PRIVATE ${spirv_cross_SOURCE_DIR})
target_link_libraries(plume_spirv_cross_msl PRIVATE
    spirv-cross-msl
    spirv-cross-glsl
    spirv-cross-core
)
set_target_properties(plume_spirv_cross_msl PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
install(TARGETS plume_spirv_cross_msl RUNTIME DESTINATION bin)
]=])
        string(REPLACE "@SPIRV_CROSS_MSL_SOURCE@" "${SPIRV_CROSS_MSL_SOURCE}" _host_spirv_cross_cmake "${_host_spirv_cross_cmake}")
        file(WRITE "${HOST_PROJECT_DIR}/CMakeLists.txt" "${_host_spirv_cross_cmake}")
        plume_add_host_tool(HOST_SPIRV_CROSS_MSL_BIN plume_spirv_cross_msl "${SPIRV_CROSS_MSL_SOURCE}" "${HOST_PROJECT_DIR}" "${HOST_BUILD_DIR}" "${HOST_INSTALL_DIR}")

        set(PLUME_SPIRV_CROSS_MSL_EXECUTABLE "${HOST_SPIRV_CROSS_MSL_BIN}" CACHE INTERNAL "Path to host plume_spirv_cross_msl executable")
    else()
        # Use provided paths or fetch and build from source
        if(DEFINED PLUME_SPIRV_CROSS_LIB_DIR AND DEFINED PLUME_SPIRV_CROSS_INCLUDE_DIR)
            # User provided prebuilt libraries
            set(SPIRV_CROSS_LIB_DIR "${PLUME_SPIRV_CROSS_LIB_DIR}")
            set(SPIRV_CROSS_INCLUDE_DIR "${PLUME_SPIRV_CROSS_INCLUDE_DIR}")
            set(SPIRV_CROSS_USE_PREBUILT TRUE)
        else()
            # Fetch and build from source
            set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
            set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

            FetchContent_Declare(
                spirv_cross
                GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
                GIT_TAG vulkan-sdk-1.4.335.0
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(spirv_cross)

            set(SPIRV_CROSS_INCLUDE_DIR "${spirv_cross_SOURCE_DIR}")
            set(SPIRV_CROSS_USE_PREBUILT FALSE)
        endif()

        add_executable(plume_spirv_cross_msl ${SPIRV_CROSS_MSL_SOURCE})
        target_include_directories(plume_spirv_cross_msl PRIVATE ${SPIRV_CROSS_INCLUDE_DIR})
        set_target_properties(plume_spirv_cross_msl PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

        if(SPIRV_CROSS_USE_PREBUILT)
            # Link against pre-built static libraries
            # Order matters: msl depends on glsl depends on core
            if(WIN32)
                target_link_libraries(plume_spirv_cross_msl PRIVATE
                    "${SPIRV_CROSS_LIB_DIR}/spirv-cross-msl.lib"
                    "${SPIRV_CROSS_LIB_DIR}/spirv-cross-glsl.lib"
                    "${SPIRV_CROSS_LIB_DIR}/spirv-cross-core.lib"
                )
            else()
                target_link_libraries(plume_spirv_cross_msl PRIVATE
                    "${SPIRV_CROSS_LIB_DIR}/libspirv-cross-msl.a"
                    "${SPIRV_CROSS_LIB_DIR}/libspirv-cross-glsl.a"
                    "${SPIRV_CROSS_LIB_DIR}/libspirv-cross-core.a"
                )
            endif()
        else()
            # Link against freshly built targets
            target_link_libraries(plume_spirv_cross_msl PRIVATE
                spirv-cross-msl
                spirv-cross-glsl
                spirv-cross-core
            )
        endif()

        set_target_properties(plume_spirv_cross_msl PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plume_tools"
        )
        set(PLUME_SPIRV_CROSS_MSL_EXECUTABLE "$<TARGET_FILE:plume_spirv_cross_msl>" CACHE INTERNAL "Path to plume_spirv_cross_msl executable")

        if(APPLE)
            set_target_properties(plume_spirv_cross_msl PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
            )
        endif()
    endif()
endfunction()

function(plume_get_spirv_cross_msl_command OUT_VAR)
    if(NOT DEFINED PLUME_SPIRV_CROSS_MSL_EXECUTABLE)
        message(FATAL_ERROR "PLUME_SPIRV_CROSS_MSL_EXECUTABLE not set. Call plume_fetch_spirv_cross() first.")
    endif()

    set(${OUT_VAR} "${PLUME_SPIRV_CROSS_MSL_EXECUTABLE}" PARENT_SCOPE)
endfunction()
