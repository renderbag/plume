# Shader compilation functions for Plume
# Using DXC for HLSL to SPIR-V compilation and Metal for macOS

# Fetch DXC for shader compilation
include(FetchContent)
FetchContent_Declare(
    dxc
    GIT_REPOSITORY https://github.com/renderbag/dxc-bin.git
    GIT_TAG 781065589d5dba23598b746b3d2e457e985b1442
)
FetchContent_MakeAvailable(dxc)

# Set up DXC paths based on platform
if(WIN32)
    set(DXC "${dxc_SOURCE_DIR}/bin/x64/dxc.exe")
    
    # Dependencies that must be next to the DLL if on Windows
    if(EXISTS "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
        configure_file("${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll" "${CMAKE_BINARY_DIR}/bin/dxcompiler.dll" COPYONLY)
    endif()
    if(EXISTS "${dxc_SOURCE_DIR}/bin/x64/dxil.dll")
        configure_file("${dxc_SOURCE_DIR}/bin/x64/dxil.dll" "${CMAKE_BINARY_DIR}/bin/dxil.dll" COPYONLY)
    endif()
elseif(APPLE)
    # On macOS, find the right DXC binary based on architecture
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(DXC_EXECUTABLE "${dxc_SOURCE_DIR}/bin/x64/dxc-macos")
        set(DXC_LIB_DIR "${dxc_SOURCE_DIR}/lib/x64")
    else()
        set(DXC_EXECUTABLE "${dxc_SOURCE_DIR}/bin/arm64/dxc-macos")
        set(DXC_LIB_DIR "${dxc_SOURCE_DIR}/lib/arm64")
    endif()
    
    # Make sure the executable is accessible and has execute permissions
    if(EXISTS "${DXC_EXECUTABLE}")
        # Set executable permission if needed
        execute_process(COMMAND chmod +x "${DXC_EXECUTABLE}")
        
        # Set DXC command with DYLD_LIBRARY_PATH
        set(DXC "DYLD_LIBRARY_PATH=${DXC_LIB_DIR}" "${DXC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "DXC not found at ${DXC_EXECUTABLE} - required for shader compilation")
    endif()
else()
    # Linux
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(DXC_EXECUTABLE "${dxc_SOURCE_DIR}/bin/x64/dxc-linux")
        set(DXC_LIB_DIR "${dxc_SOURCE_DIR}/lib/x64")
    else()
        set(DXC_EXECUTABLE "${dxc_SOURCE_DIR}/bin/arm64/dxc-linux")
        set(DXC_LIB_DIR "${dxc_SOURCE_DIR}/lib/arm64")
    endif()
    
    # Make sure the executable is accessible and has execute permissions
    if(EXISTS "${DXC_EXECUTABLE}")
        # Set executable permission if needed
        execute_process(COMMAND chmod +x "${DXC_EXECUTABLE}")
        set(DXC "LD_LIBRARY_PATH=${DXC_LIB_DIR}" "${DXC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "DXC not found at ${DXC_EXECUTABLE} - required for shader compilation")
    endif()
endif()

# Function to compile HLSL using DXC with common parameters
function(build_shader_dxc_impl TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME OUTPUT_FORMAT)
    # Create unique output names based on format
    if(OUTPUT_FORMAT STREQUAL "spirv")
        set(OUTPUT_EXT "spv")
        set(BLOB_SUFFIX "SPIRV")
        set(FORMAT_FLAGS "-spirv")
    elseif(OUTPUT_FORMAT STREQUAL "dxil")
        set(OUTPUT_EXT "dxil")
        set(BLOB_SUFFIX "DXIL")
        set(FORMAT_FLAGS "-Wno-ignored-attributes")
    else()
        message(FATAL_ERROR "Unknown output format: ${OUTPUT_FORMAT}")
    endif()

    set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.${OUTPUT_EXT}")
    set(C_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.${OUTPUT_FORMAT}.c")
    set(H_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.${OUTPUT_FORMAT}.h")
    
    # Create output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    
    # Determine the shader options based on type
    if(SHADER_TYPE STREQUAL "vertex")
        set(SHADER_PROFILE "vs_6_0")
        set(DXC_EXTRA_ARGS "-fvk-invert-y") # Only include invert-y for vertex shaders
    elseif(SHADER_TYPE STREQUAL "fragment")
        set(SHADER_PROFILE "ps_6_0")
        set(DXC_EXTRA_ARGS "")
    elseif(SHADER_TYPE STREQUAL "compute")
        set(SHADER_PROFILE "cs_6_0")
        set(DXC_EXTRA_ARGS "")
    else()
        message(FATAL_ERROR "Unknown shader type: ${SHADER_TYPE}")
    endif()

    set(BLOB_NAME "${OUTPUT_NAME}Blob${BLOB_SUFFIX}")
    
    # Compile using DXC
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${DXC} -E main -T ${SHADER_PROFILE} ${FORMAT_FLAGS} -fspv-target-env=vulkan1.0 -fvk-use-dx-layout ${DXC_EXTRA_ARGS}
                -Fo ${SHADER_OUTPUT} ${SHADER_SOURCE}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling ${SHADER_TYPE} shader ${SHADER_SOURCE} to ${OUTPUT_FORMAT} using DXC"
    )
    
    # Generate C header
    add_custom_command(
        OUTPUT "${C_OUTPUT}" "${H_OUTPUT}"
        COMMAND file_to_c ${SHADER_OUTPUT} "${BLOB_NAME}" "${C_OUTPUT}" "${H_OUTPUT}"
        DEPENDS ${SHADER_OUTPUT} file_to_c
        COMMENT "Generating C header for ${OUTPUT_FORMAT} shader ${OUTPUT_NAME}"
    )
    
    # Add the generated source file to the target
    target_sources(${TARGET_NAME} PRIVATE "${C_OUTPUT}")
    
    # Make sure the target can find the generated header
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
endfunction()

# Function to compile HLSL to SPIR-V using DXC
function(build_shader_spirv_impl TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME)
    build_shader_dxc_impl(${TARGET_NAME} ${SHADER_SOURCE} ${SHADER_TYPE} ${OUTPUT_NAME} "spirv")
endfunction()

# Function to compile HLSL to DXIL using DXC
function(build_shader_dxil_impl TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME)
    build_shader_dxc_impl(${TARGET_NAME} ${SHADER_SOURCE} ${SHADER_TYPE} ${OUTPUT_NAME} "dxil")
endfunction()

# Function to compile Metal shaders
function(build_shader_metal_impl TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    # Create unique output names
    set(METALLIB_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metallib")
    set(IR_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.ir")
    set(METAL_C_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.c")
    set(METAL_H_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.metal.h")
    
    # Create output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    
    # Compile Metal to IR
    add_custom_command(
        OUTPUT ${IR_OUTPUT}
        COMMAND xcrun -sdk macosx metal -o ${IR_OUTPUT} -c ${SHADER_SOURCE}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling Metal shader to IR"
    )
    
    # Compile IR to metallib
    add_custom_command(
        OUTPUT ${METALLIB_OUTPUT}
        COMMAND xcrun -sdk macosx metallib ${IR_OUTPUT} -o ${METALLIB_OUTPUT}
        DEPENDS ${IR_OUTPUT}
        COMMENT "Compiling Metal IR to metallib"
    )
    
    # Generate C header
    add_custom_command(
        OUTPUT "${METAL_C_OUTPUT}" "${METAL_H_OUTPUT}"
        COMMAND file_to_c ${METALLIB_OUTPUT} "${OUTPUT_NAME}BlobMSL" "${METAL_C_OUTPUT}" "${METAL_H_OUTPUT}"
        DEPENDS ${METALLIB_OUTPUT} file_to_c
        COMMENT "Generating C header for Metal shader ${OUTPUT_NAME}"
    )
    
    # Add the generated source file to the target
    target_sources(${TARGET_NAME} PRIVATE "${METAL_C_OUTPUT}")
    
    # Make sure the target can find the generated header
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
endfunction()

# Compile a shader based on its type
function(compile_shader TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME)
    # Get the file extension to determine the shader language
    get_filename_component(SHADER_EXT ${SHADER_SOURCE} EXT)
    
    # Compile based on extension
    if(SHADER_EXT STREQUAL ".metal")
        # Compile Metal shader
        if(APPLE)
            build_shader_metal_impl(${TARGET_NAME} ${SHADER_SOURCE} ${OUTPUT_NAME})
        endif()
    elseif(SHADER_SOURCE MATCHES ".*\\.hlsl$")
        # Compile HLSL shader to SPIR-V using DXC
        build_shader_spirv_impl(${TARGET_NAME} ${SHADER_SOURCE} ${SHADER_TYPE} ${OUTPUT_NAME})
    else()
        message(WARNING "Unsupported shader extension ${SHADER_EXT} for ${SHADER_SOURCE} - only .hlsl and .metal files are supported")
    endif()
endfunction()

# Shorthand functions for common shader types
function(compile_vertex_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "vertex" ${OUTPUT_NAME})
endfunction()

function(compile_fragment_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "fragment" ${OUTPUT_NAME})
endfunction()

function(compile_compute_shader TARGET_NAME SHADER_SOURCE OUTPUT_NAME)
    compile_shader(${TARGET_NAME} ${SHADER_SOURCE} "compute" ${OUTPUT_NAME})
endfunction() 
