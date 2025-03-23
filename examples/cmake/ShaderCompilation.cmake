# Shader compilation functions for Plume
# Using DXC for HLSL to SPIR-V compilation and Metal for macOS

# Detect platform
if(APPLE)
    set(PLUME_PLATFORM "macos")
elseif(WIN32)
    set(PLUME_PLATFORM "windows")
else()
    set(PLUME_PLATFORM "linux")
endif()

# Set up DXC paths based on platform
if(WIN32)
    set(DXC "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxc.exe")
    
    # Dependencies that must be next to the DLL if on Windows
    if(EXISTS "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxcompiler.dll")
        configure_file("${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxcompiler.dll" "${CMAKE_BINARY_DIR}/bin/dxcompiler.dll" COPYONLY)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxil.dll")
        configure_file("${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxil.dll" "${CMAKE_BINARY_DIR}/bin/dxil.dll" COPYONLY)
    endif()
elseif(APPLE)
    # On macOS, find the right DXC binary based on architecture
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(DXC_EXECUTABLE "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxc-macos")
        set(DXC_LIB_DIR "${CMAKE_SOURCE_DIR}/contrib/dxc/lib/x64")
    else()
        set(DXC_EXECUTABLE "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/arm64/dxc-macos")
        set(DXC_LIB_DIR "${CMAKE_SOURCE_DIR}/contrib/dxc/lib/arm64")
    endif()
    
    # Make sure the executable is accessible and has execute permissions
    if(EXISTS "${DXC_EXECUTABLE}")
        # Set executable permission if needed
        execute_process(COMMAND chmod +x "${DXC_EXECUTABLE}")
        set(DXC "${DXC_EXECUTABLE}")
        message(STATUS "Found DXC at ${DXC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "DXC not found at ${DXC_EXECUTABLE} - required for shader compilation")
    endif()
else()
    # Linux
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(DXC_EXECUTABLE "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/x64/dxc-linux")
    else()
        set(DXC_EXECUTABLE "${CMAKE_SOURCE_DIR}/contrib/dxc/bin/arm64/dxc-linux")
    endif()
    
    # Make sure the executable is accessible and has execute permissions
    if(EXISTS "${DXC_EXECUTABLE}")
        # Set executable permission if needed
        execute_process(COMMAND chmod +x "${DXC_EXECUTABLE}")
        set(DXC "${DXC_EXECUTABLE}")
        message(STATUS "Found DXC at ${DXC_EXECUTABLE}")
    else()
        message(FATAL_ERROR "DXC not found at ${DXC_EXECUTABLE} - required for shader compilation")
    endif()
endif()

# Function to compile HLSL to SPIR-V using DXC
function(build_shader_spirv_impl TARGET_NAME SHADER_SOURCE SHADER_TYPE OUTPUT_NAME)
    # Create unique output names
    set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spv")
    set(SPIRV_C_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.c")
    set(SPIRV_H_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${OUTPUT_NAME}.spirv.h")
    
    # Create output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    
    # Determine the shader options based on type
    if(SHADER_TYPE STREQUAL "vertex")
        set(SHADER_PROFILE "vs_6_0")
        set(DXC_EXTRA_ARGS "-fvk-invert-y") # Only include invert-y for vertex shaders
        set(BLOB_NAME "${OUTPUT_NAME}BlobSPIRV")
    elseif(SHADER_TYPE STREQUAL "fragment")
        set(SHADER_PROFILE "ps_6_0")
        set(DXC_EXTRA_ARGS "")
        set(BLOB_NAME "${OUTPUT_NAME}BlobSPIRV") 
    elseif(SHADER_TYPE STREQUAL "compute")
        set(SHADER_PROFILE "cs_6_0")
        set(DXC_EXTRA_ARGS "")
        set(BLOB_NAME "${OUTPUT_NAME}BlobSPIRV")
    else()
        message(FATAL_ERROR "Unknown shader type: ${SHADER_TYPE}")
    endif()
    
    # Compile to SPIR-V using DXC
    if(APPLE)
        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E env "DYLD_LIBRARY_PATH=${DXC_LIB_DIR}" 
                    ${DXC} -E main -T ${SHADER_PROFILE} -spirv -fspv-target-env=vulkan1.0 -fvk-use-dx-layout ${DXC_EXTRA_ARGS}
                    -Fo ${SPIRV_OUTPUT} ${SHADER_SOURCE}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_TYPE} shader ${SHADER_SOURCE} to SPIR-V using DXC"
        )
    else()
        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${DXC} -E main -T ${SHADER_PROFILE} -spirv -fspv-target-env=vulkan1.0 -fvk-use-dx-layout ${DXC_EXTRA_ARGS}
                    -Fo ${SPIRV_OUTPUT} ${SHADER_SOURCE}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling ${SHADER_TYPE} shader ${SHADER_SOURCE} to SPIR-V using DXC"
        )
    endif()
    
    # Generate C header
    add_custom_command(
        OUTPUT "${SPIRV_C_OUTPUT}" "${SPIRV_H_OUTPUT}"
        COMMAND file_to_c ${SPIRV_OUTPUT} "${BLOB_NAME}" "${SPIRV_C_OUTPUT}" "${SPIRV_H_OUTPUT}"
        DEPENDS ${SPIRV_OUTPUT} file_to_c
        COMMENT "Generating C header for SPIR-V shader ${OUTPUT_NAME}"
    )
    
    # Add the generated source file to the target
    target_sources(${TARGET_NAME} PRIVATE "${SPIRV_C_OUTPUT}")
    
    # Make sure the target can find the generated header
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_BINARY_DIR}/shaders")
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
