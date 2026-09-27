# PlumeShaders.cmake
# Shader compilation helpers for Plume examples
#
# Usage (examples):
#   include(path/to/plume/examples/cmake/PlumeShaders.cmake)
#   plume_shaders_init()
#
#   plume_compile_vertex_shader(my_target shaders/main.vert.hlsl mainVert VSMain)
#   plume_compile_pixel_shader(my_target shaders/main.frag.hlsl mainFrag PSMain)
#   plume_compile_compute_shader(my_target shaders/compute.hlsl computeShader CSMain)

include("${CMAKE_CURRENT_LIST_DIR}/modules/PlumeFileToC.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/PlumeDXC.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/PlumeSpirvCross.cmake")

function(_plume_embed TARGET_NAME INPUT_FILE VAR_NAME OUTPUT_C OUTPUT_H)
    plume_build_file_to_c()
    plume_get_file_to_c_command(FILE_TO_C_CMD)

    get_filename_component(OUT_DIR "${OUTPUT_C}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUT_DIR}")

    add_custom_command(
        OUTPUT "${OUTPUT_C}" "${OUTPUT_H}"
        COMMAND ${FILE_TO_C_CMD} "${INPUT_FILE}" "${VAR_NAME}" "${OUTPUT_C}" "${OUTPUT_H}"
        DEPENDS "${INPUT_FILE}" plume_file_to_c
        COMMENT "Embedding ${VAR_NAME} from ${INPUT_FILE}"
        VERBATIM
    )

    target_sources(${TARGET_NAME} PRIVATE "${OUTPUT_C}")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}")
endfunction()

function(plume_shaders_init)
    if(NOT DEFINED PLUME_DXC_EXECUTABLE)
        plume_fetch_dxc()
    endif()

    if(APPLE AND NOT TARGET plume_spirv_cross_msl)
        plume_fetch_spirv_cross()
    endif()

    plume_build_file_to_c()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
endfunction()

function(_plume_compile_hlsl_impl TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME OUTPUT_FORMAT ENTRY_POINT)
    cmake_parse_arguments(PARSE_ARGV 6 ARG "" "SHADER_MODEL;OUTPUT_DIR" "INCLUDE_DIRS;EXTRA_ARGS")

    plume_get_dxc_command(DXC_CMD)

    if(OUTPUT_FORMAT STREQUAL "spirv")
        set(OUTPUT_EXT "spirv")
        set(BLOB_SUFFIX "SPIRV")
        set(FORMAT_FLAGS ${PLUME_DXC_SPV_OPTS})
    elseif(OUTPUT_FORMAT STREQUAL "dxil")
        set(OUTPUT_EXT "dxil")
        set(BLOB_SUFFIX "DXIL")
        set(FORMAT_FLAGS ${PLUME_DXC_DXIL_OPTS})
    else()
        message(FATAL_ERROR "Unknown output format: ${OUTPUT_FORMAT}")
    endif()

    set(SHADER_MODEL "6_0")

    if(ARG_SHADER_MODEL)
        set(SHADER_MODEL "${ARG_SHADER_MODEL}")
    endif()

    if(ARG_OUTPUT_DIR)
        set(OUT_DIR "${ARG_OUTPUT_DIR}")
    else()
        set(OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    endif()

    file(MAKE_DIRECTORY "${OUT_DIR}")

    set(PROFILE "")
    set(DXC_TYPE_ARGS "")

    if(SHADER_TYPE STREQUAL "vertex")
        set(PROFILE "vs_${SHADER_MODEL}")
        set(DXC_TYPE_ARGS "-fvk-invert-y")
    elseif(SHADER_TYPE STREQUAL "pixel" OR SHADER_TYPE STREQUAL "fragment")
        set(PROFILE "ps_${SHADER_MODEL}")
    elseif(SHADER_TYPE STREQUAL "compute")
        set(PROFILE "cs_${SHADER_MODEL}")
    elseif(SHADER_TYPE STREQUAL "geometry")
        set(PROFILE "gs_${SHADER_MODEL}")
    elseif(SHADER_TYPE STREQUAL "library")
        set(PROFILE "lib_${SHADER_MODEL}")
    else()
        message(FATAL_ERROR "Unknown shader type: ${SHADER_TYPE}")
    endif()

    set(INCLUDE_FLAGS "")

    foreach(DIR ${ARG_INCLUDE_DIRS})
        list(APPEND INCLUDE_FLAGS "-I${DIR}")
    endforeach()

    if(ENTRY_POINT STREQUAL "")
        set(ENTRY_ARGS "")
    else()
        set(ENTRY_ARGS "-E" "${ENTRY_POINT}")
    endif()

    set(OUTPUT_FILE "${OUT_DIR}/${OUTPUT_NAME}.hlsl.${OUTPUT_EXT}")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND ${DXC_CMD}
            ${PLUME_DXC_COMMON_OPTS}
            ${INCLUDE_FLAGS}
            ${DXC_TYPE_ARGS}
            ${ARG_EXTRA_ARGS}
            ${ENTRY_ARGS}
            -T ${PROFILE}
            ${FORMAT_FLAGS}
            -Fo "${OUTPUT_FILE}"
            "${SHADER_SOURCE}"
        DEPENDS "${SHADER_SOURCE}"
        COMMENT "DXC: ${SHADER_SOURCE} -> ${OUTPUT_EXT}"
        VERBATIM
    )

    set(C_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.${OUTPUT_EXT}.c")
    set(H_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.${OUTPUT_EXT}.h")
    _plume_embed(${TARGET_NAME} "${OUTPUT_FILE}" "${OUTPUT_NAME}Blob${BLOB_SUFFIX}" "${C_OUTPUT}" "${H_OUTPUT}")
endfunction()

function(_plume_compile_spirv_to_metal_impl TARGET_NAME SPIRV_FILE OUTPUT_NAME)
    cmake_parse_arguments(PARSE_ARGV 3 ARG "" "OUTPUT_DIR" "")
    plume_get_spirv_cross_msl_command(SPIRV_CROSS_MSL_CMD)

    if(ARG_OUTPUT_DIR)
        set(OUT_DIR "${ARG_OUTPUT_DIR}")
    else()
        set(OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    endif()

    file(MAKE_DIRECTORY "${OUT_DIR}")

    set(METAL_SOURCE "${OUT_DIR}/${OUTPUT_NAME}.hlsl.metal")
    set(IR_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.ir")
    set(METALLIB_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.metallib")
    set(C_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.metal.c")
    set(H_OUTPUT "${OUT_DIR}/${OUTPUT_NAME}.hlsl.metal.h")

    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(METAL_VERSION_FLAG "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(METAL_VERSION_FLAG "")
    endif()

    add_custom_command(
        OUTPUT "${METAL_SOURCE}"
        COMMAND ${SPIRV_CROSS_MSL_CMD} "${SPIRV_FILE}" "${METAL_SOURCE}"
        DEPENDS "${SPIRV_FILE}" plume_spirv_cross_msl
        COMMENT "SPIRV-Cross: ${SPIRV_FILE} -> Metal source"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${IR_OUTPUT}"
        COMMAND xcrun -sdk macosx metal ${METAL_VERSION_FLAG} -o "${IR_OUTPUT}" -c "${METAL_SOURCE}"
        DEPENDS "${METAL_SOURCE}"
        COMMENT "Compiling Metal shader ${OUTPUT_NAME} to IR"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${METALLIB_OUTPUT}"
        COMMAND xcrun -sdk macosx metallib "${IR_OUTPUT}" -o "${METALLIB_OUTPUT}"
        DEPENDS "${IR_OUTPUT}"
        COMMENT "Linking ${OUTPUT_NAME} to metallib"
        VERBATIM
    )

    _plume_embed(${TARGET_NAME} "${METALLIB_OUTPUT}" "${OUTPUT_NAME}BlobMSL"
        "${C_OUTPUT}" "${H_OUTPUT}")
endfunction()

function(_plume_compile_metal_impl TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    plume_get_file_to_c_command(FILE_TO_C_CMD)

    set(IR_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.ir")
    set(METALLIB_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metallib")
    set(C_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.c")
    set(H_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.h")

    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(METAL_VERSION_FLAG "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        set(METAL_VERSION_FLAG "")
    endif()

    add_custom_command(
        OUTPUT "${IR_OUTPUT}"
        COMMAND xcrun -sdk macosx metal ${METAL_VERSION_FLAG} -o "${IR_OUTPUT}" -c "${SHADER_SOURCE}"
        DEPENDS "${SHADER_SOURCE}"
        COMMENT "Compiling Metal shader ${OUTPUT_NAME} to IR"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${METALLIB_OUTPUT}"
        COMMAND xcrun -sdk macosx metallib "${IR_OUTPUT}" -o "${METALLIB_OUTPUT}"
        DEPENDS "${IR_OUTPUT}"
        COMMENT "Linking ${OUTPUT_NAME} to metallib"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${C_OUTPUT}" "${H_OUTPUT}"
        COMMAND ${FILE_TO_C_CMD} "${METALLIB_OUTPUT}" "${OUTPUT_NAME}BlobMSL" "${C_OUTPUT}" "${H_OUTPUT}"
        DEPENDS "${METALLIB_OUTPUT}" plume_file_to_c
        COMMENT "Generating C header for Metal shader ${OUTPUT_NAME}"
        VERBATIM
    )

    target_sources(${TARGET_NAME} PRIVATE "${C_OUTPUT}")
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}")
endfunction()

# Public API
function(plume_compile_shader TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME ENTRY_POINT)
    cmake_parse_arguments(ARG "SPEC_CONSTANTS" "SHADER_MODEL;OUTPUT_DIR" "INCLUDE_DIRS;EXTRA_ARGS" ${ARGN})

    get_filename_component(SHADER_EXT "${SHADER_SOURCE}" EXT)

    if(SHADER_EXT MATCHES "\\.metal$")
        if(APPLE)
            _plume_compile_metal_impl(${TARGET_NAME} "${SHADER_SOURCE}" ${OUTPUT_NAME})
        endif()
    elseif(SHADER_EXT MATCHES "\\.hlsl$")
        set(IMPL_ARGS "")

        if(ARG_SHADER_MODEL)
            list(APPEND IMPL_ARGS SHADER_MODEL "${ARG_SHADER_MODEL}")
        endif()

        if(ARG_INCLUDE_DIRS)
            list(APPEND IMPL_ARGS INCLUDE_DIRS ${ARG_INCLUDE_DIRS})
        endif()

        if(ARG_EXTRA_ARGS)
            list(APPEND IMPL_ARGS EXTRA_ARGS ${ARG_EXTRA_ARGS})
        endif()

        if(ARG_OUTPUT_DIR)
            list(APPEND IMPL_ARGS OUTPUT_DIR "${ARG_OUTPUT_DIR}")
            set(OUT_DIR "${ARG_OUTPUT_DIR}")
        else()
            set(OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
        endif()

        _plume_compile_hlsl_impl(${TARGET_NAME} "${SHADER_SOURCE}" ${SHADER_TYPE} ${OUTPUT_NAME} "spirv" ${ENTRY_POINT} ${IMPL_ARGS})

        if(WIN32 AND NOT ARG_SPEC_CONSTANTS)
            _plume_compile_hlsl_impl(${TARGET_NAME} "${SHADER_SOURCE}" ${SHADER_TYPE} ${OUTPUT_NAME} "dxil" ${ENTRY_POINT} ${IMPL_ARGS})
        endif()

        if(APPLE AND TARGET plume_spirv_cross_msl)
            set(SPIRV_FILE "${OUT_DIR}/${OUTPUT_NAME}.hlsl.spirv")
            _plume_compile_spirv_to_metal_impl(${TARGET_NAME} "${SPIRV_FILE}" ${OUTPUT_NAME} OUTPUT_DIR "${OUT_DIR}")
        endif()
    else()
        message(WARNING "Unsupported shader extension '${SHADER_EXT}' for ${SHADER_SOURCE}. Use .hlsl or .metal")
    endif()
endfunction()

function(plume_compile_vertex_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME ENTRY_POINT)
    plume_compile_shader(${TARGET_NAME} "${SHADER_SOURCE}" "vertex" ${OUTPUT_NAME} ${ENTRY_POINT} ${ARGN})
endfunction()

function(plume_compile_pixel_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME ENTRY_POINT)
    plume_compile_shader(${TARGET_NAME} "${SHADER_SOURCE}" "pixel" ${OUTPUT_NAME} ${ENTRY_POINT} ${ARGN})
endfunction()

function(plume_compile_compute_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME ENTRY_POINT)
    plume_compile_shader(${TARGET_NAME} "${SHADER_SOURCE}" "compute" ${OUTPUT_NAME} ${ENTRY_POINT} ${ARGN})
endfunction()

function(plume_compile_geometry_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME ENTRY_POINT)
    plume_compile_shader(${TARGET_NAME} "${SHADER_SOURCE}" "geometry" ${OUTPUT_NAME} ${ENTRY_POINT} ${ARGN})
endfunction()
