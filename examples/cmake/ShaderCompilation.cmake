# Shader compilation functions for Plume

# Find pkgx if available
find_program(PKGX_EXECUTABLE NAMES pkgx)

# Detect platform
if(APPLE)
    set(PLUME_PLATFORM "macos")
elseif(WIN32)
    set(PLUME_PLATFORM "windows")
else()
    set(PLUME_PLATFORM "linux")
endif()

# Metal shader compilation for macOS
function(compile_metal_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    set(METALLIB_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metallib")
    set(IR_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.ir")
    
    # Create output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    
    # Compile .metal to .ir
    add_custom_command(
        OUTPUT ${IR_OUTPUT}
        COMMAND xcrun -sdk macosx metal -o ${IR_OUTPUT} -c ${SHADER_SOURCE}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling Metal shader ${SHADER_SOURCE} to IR"
    )
    
    # Compile .ir to .metallib
    add_custom_command(
        OUTPUT ${METALLIB_OUTPUT}
        COMMAND xcrun -sdk macosx metallib ${IR_OUTPUT} -o ${METALLIB_OUTPUT}
        DEPENDS ${IR_OUTPUT}
        COMMENT "Compiling Metal IR to metallib"
    )
    
    # Generate C header
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.c" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.h"
        COMMAND file_to_c ${METALLIB_OUTPUT} "${OUTPUT_NAME}BlobMSL" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.c" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.h"
        DEPENDS ${METALLIB_OUTPUT} file_to_c
        COMMENT "Generating C header for Metal shader ${OUTPUT_NAME}"
    )
    
    # Add the generated source file to the target
    target_sources(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.c")
    
    # Make sure the target can find the generated header
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
endfunction()

# SPIR-V compilation for Vulkan
function(compile_spirv_shader TARGET_NAME SHADER_SOURCE SHADER_STAGE OUTPUT_NAME)
    # Skip SPIR-V compilation on macOS
    if(APPLE)
        message(STATUS "Skipping SPIR-V compilation on macOS for ${SHADER_SOURCE}")
        # Create a dummy source file to avoid compilation issues
        file(WRITE "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c" 
            "// Dummy file for ${OUTPUT_NAME} on macOS\n"
            "#include <stdint.h>\n"
            "const uint8_t ${OUTPUT_NAME}BlobSPIRV[] = {0};\n"
            "const uint32_t ${OUTPUT_NAME}BlobSPIRVSize = 0;\n"
        )
        file(WRITE "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.h"
            "// Dummy header for ${OUTPUT_NAME} on macOS\n"
            "#ifndef ${OUTPUT_NAME}_SPIRV_H\n"
            "#define ${OUTPUT_NAME}_SPIRV_H\n"
            "#include <stdint.h>\n"
            "extern const uint8_t ${OUTPUT_NAME}BlobSPIRV[];\n"
            "extern const uint32_t ${OUTPUT_NAME}BlobSPIRVSize;\n"
            "#endif\n"
        )
        target_sources(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c")
        target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
        return()
    endif()
    
    set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spv")
    
    # Create output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    
    # Determine the shader stage
    if(SHADER_STAGE STREQUAL "vertex")
        set(GLSLC_STAGE "-fshader-stage=vert")
    elseif(SHADER_STAGE STREQUAL "fragment")
        set(GLSLC_STAGE "-fshader-stage=frag")
    elseif(SHADER_STAGE STREQUAL "compute")
        set(GLSLC_STAGE "-fshader-stage=comp")
    else()
        message(FATAL_ERROR "Unknown shader stage: ${SHADER_STAGE}")
    endif()
    
    # Compile GLSL to SPIR-V
    if(PKGX_EXECUTABLE)
        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${PKGX_EXECUTABLE} +glslang.org glslangValidator ${GLSLC_STAGE} -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling GLSL shader ${SHADER_SOURCE} to SPIR-V"
        )
    else()
        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND glslangValidator ${GLSLC_STAGE} -V ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling GLSL shader ${SHADER_SOURCE} to SPIR-V"
        )
    endif()
    
    # Generate C header
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.h"
        COMMAND file_to_c ${SPIRV_OUTPUT} "${OUTPUT_NAME}BlobSPIRV" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c" "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.h"
        DEPENDS ${SPIRV_OUTPUT} file_to_c
        COMMENT "Generating C header for SPIR-V shader ${OUTPUT_NAME}"
    )
    
    # Add the generated source file to the target
    target_sources(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c")
    
    # Make sure the target can find the generated header
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
endfunction()

# Wrapper function to compile a shader for all supported platforms
function(compile_shader TARGET_NAME SHADER_SOURCE SHADER_STAGE OUTPUT_NAME)
    # Get the file extension
    get_filename_component(SHADER_EXT ${SHADER_SOURCE} EXT)
    
    # Metal-specific compilation for macOS
    if(APPLE)
        if(SHADER_EXT STREQUAL ".metal")
            compile_metal_shader(${TARGET_NAME} ${SHADER_SOURCE} ${OUTPUT_NAME})
        else()
            message(WARNING "Shader ${SHADER_SOURCE} is not a .metal file, skipping Metal compilation")
        endif()
    endif()
    
    # SPIR-V compilation for all platforms
    if(SHADER_EXT STREQUAL ".glsl" OR SHADER_EXT STREQUAL ".vert" OR SHADER_EXT STREQUAL ".frag" OR SHADER_EXT STREQUAL ".comp")
        compile_spirv_shader(${TARGET_NAME} ${SHADER_SOURCE} ${SHADER_STAGE} ${OUTPUT_NAME})
    else()
        message(WARNING "Shader ${SHADER_SOURCE} has unknown extension ${SHADER_EXT}, skipping SPIR-V compilation")
    endif()
endfunction()

# Shorthand functions for common shader types
function(compile_vertex_shader TARGET_NAME SHADER_SOURCE)
    if(${ARGC} GREATER 2)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "vertex" ${ARGV2})
    else()
        get_filename_component(OUTPUT_NAME ${SHADER_SOURCE} NAME_WE)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "vertex" ${OUTPUT_NAME})
    endif()
endfunction()

function(compile_fragment_shader TARGET_NAME SHADER_SOURCE)
    if(${ARGC} GREATER 2)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "fragment" ${ARGV2})
    else()
        get_filename_component(OUTPUT_NAME ${SHADER_SOURCE} NAME_WE)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "fragment" ${OUTPUT_NAME})
    endif()
endfunction()

function(compile_compute_shader TARGET_NAME SHADER_SOURCE)
    if(${ARGC} GREATER 2)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "compute" ${ARGV2})
    else()
        get_filename_component(OUTPUT_NAME ${SHADER_SOURCE} NAME_WE)
        compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "compute" ${OUTPUT_NAME})
    endif()
endfunction() 