# PlumeFileToC.cmake
# Builds the file_to_c tool for embedding binary files as C arrays

function(_plume_get_host_build_type OUT_VAR)
    if(CMAKE_BUILD_TYPE)
        set(${OUT_VAR} "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "Release" PARENT_SCOPE)
    endif()
endfunction()

# Build the file_to_c tool for the host system
function(plume_build_file_to_c)
    if(TARGET plume_file_to_c)
        return()
    endif()

    # Find the source file relative to this module
    set(FILE_TO_C_SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tools/file_to_c.cpp")

    if(NOT EXISTS "${FILE_TO_C_SOURCE}")
        message(FATAL_ERROR "plume file_to_c.cpp not found at ${FILE_TO_C_SOURCE}")
    endif()

    if(CMAKE_CROSSCOMPILING)
        _plume_get_host_build_type(HOST_BUILD_TYPE)

        set(HOST_PROJECT_DIR "${CMAKE_BINARY_DIR}/host-tools/file_to_c-src")
        set(HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/host-tools/file_to_c-build")
        set(HOST_INSTALL_DIR "${CMAKE_BINARY_DIR}/host-tools/install")
        file(MAKE_DIRECTORY "${HOST_PROJECT_DIR}")

        if(CMAKE_HOST_WIN32)
            set(HOST_EXE_SUFFIX ".exe")
        else()
            set(HOST_EXE_SUFFIX "")
        endif()

        set(HOST_FILE_TO_C_BIN "${HOST_INSTALL_DIR}/bin/file_to_c${HOST_EXE_SUFFIX}")

        set(_host_file_to_c_cmake [=[
cmake_minimum_required(VERSION 3.16)
project(plume_host_file_to_c LANGUAGES CXX)

add_executable(file_to_c "@FILE_TO_C_SOURCE@")
set_target_properties(file_to_c PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
install(TARGETS file_to_c RUNTIME DESTINATION bin)
]=])
        string(REPLACE "@FILE_TO_C_SOURCE@" "${FILE_TO_C_SOURCE}" _host_file_to_c_cmake "${_host_file_to_c_cmake}")
        file(WRITE "${HOST_PROJECT_DIR}/CMakeLists.txt" "${_host_file_to_c_cmake}")

        add_custom_command(OUTPUT "${HOST_FILE_TO_C_BIN}"
            COMMAND ${CMAKE_COMMAND} -S "${HOST_PROJECT_DIR}" -B "${HOST_BUILD_DIR}" -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE=${HOST_BUILD_TYPE}
            COMMAND ${CMAKE_COMMAND} --build "${HOST_BUILD_DIR}" --config ${HOST_BUILD_TYPE} --target file_to_c
            COMMAND ${CMAKE_COMMAND} --install "${HOST_BUILD_DIR}" --config ${HOST_BUILD_TYPE} --prefix "${HOST_INSTALL_DIR}"
            DEPENDS "${FILE_TO_C_SOURCE}"
            COMMENT "Building host file_to_c tool"
            USES_TERMINAL
            VERBATIM
        )

        add_custom_target(plume_file_to_c DEPENDS "${HOST_FILE_TO_C_BIN}")
        set(PLUME_FILE_TO_C_EXECUTABLE "${HOST_FILE_TO_C_BIN}" CACHE INTERNAL "Path to host file_to_c executable")
    else()
        add_executable(plume_file_to_c ${FILE_TO_C_SOURCE})
        set_target_properties(plume_file_to_c PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plume_tools"
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
        set(PLUME_FILE_TO_C_EXECUTABLE "$<TARGET_FILE:plume_file_to_c>" CACHE INTERNAL "Path to file_to_c executable")
    endif()

    if(APPLE AND TARGET plume_file_to_c AND NOT CMAKE_CROSSCOMPILING)
        set_target_properties(plume_file_to_c PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        )
    endif()
endfunction()

function(plume_get_file_to_c_command OUT_VAR)
    if(NOT DEFINED PLUME_FILE_TO_C_EXECUTABLE)
        message(FATAL_ERROR "PLUME_FILE_TO_C_EXECUTABLE not set. Call plume_build_file_to_c() first.")
    endif()

    set(${OUT_VAR} "${PLUME_FILE_TO_C_EXECUTABLE}" PARENT_SCOPE)
endfunction()

# Convert a binary file to a C header
# Usage: plume_file_to_c_header(INPUT_FILE OUTPUT_C OUTPUT_H VARIABLE_NAME)
function(plume_file_to_c_header INPUT_FILE VARIABLE_NAME OUTPUT_C OUTPUT_H)
    plume_build_file_to_c()
    plume_get_file_to_c_command(FILE_TO_C_CMD)

    get_filename_component(OUTPUT_DIR "${OUTPUT_C}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    add_custom_command(
        OUTPUT "${OUTPUT_C}" "${OUTPUT_H}"
        COMMAND ${FILE_TO_C_CMD} "${INPUT_FILE}" "${VARIABLE_NAME}" "${OUTPUT_C}" "${OUTPUT_H}"
        DEPENDS "${INPUT_FILE}" plume_file_to_c
        COMMENT "Generating C header for ${VARIABLE_NAME}"
        VERBATIM
    )
endfunction()
