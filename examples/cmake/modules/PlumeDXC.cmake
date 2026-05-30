# PlumeDXC.cmake
# Fetches and configures DXC (DirectX Shader Compiler) for HLSL compilation

include(FetchContent)

function(_plume_get_dxc_host_arch OUT_VAR)
    if(CMAKE_CROSSCOMPILING)
        set(_plume_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    else()
        set(_plume_arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    string(TOLOWER "${_plume_arch}" _plume_arch_lower)

    if(_plume_arch_lower MATCHES "^(x86_64|amd64)$")
        set(${OUT_VAR} "x64" PARENT_SCOPE)
    elseif(_plume_arch_lower MATCHES "^(arm64|aarch64)$")
        set(${OUT_VAR} "arm64" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unsupported host architecture for DXC: ${_plume_arch}")
    endif()
endfunction()

# Set up common DXC options (called regardless of fetch)
function(_plume_setup_dxc_options)
    if(DEFINED PLUME_DXC_COMMON_OPTS)
        return()
    endif()

    set(PLUME_DXC_COMMON_OPTS "-I${CMAKE_SOURCE_DIR}" CACHE INTERNAL "DXC common options")
    set(PLUME_DXC_DXIL_OPTS "-Wno-ignored-attributes" CACHE INTERNAL "DXC DXIL options")
    set(PLUME_DXC_SPV_OPTS "-spirv;-fspv-target-env=vulkan1.0;-fvk-use-dx-layout" CACHE INTERNAL "DXC SPIR-V options")
    set(PLUME_DXC_RT_OPTS "-D;RT_SHADER;-T;lib_6_3;-fspv-target-env=vulkan1.1spirv1.4;-fspv-extension=SPV_KHR_ray_tracing;-fspv-extension=SPV_EXT_descriptor_indexing" CACHE INTERNAL "DXC ray tracing options")
endfunction()

# Fetch DXC binaries
function(plume_fetch_dxc)
    if(DEFINED PLUME_DXC_EXECUTABLE)
        _plume_setup_dxc_options()
        return()
    endif()

    FetchContent_Declare(
        plume_dxc
        GIT_REPOSITORY https://github.com/renderbag/dxc-bin.git
        GIT_TAG 781065589d5dba23598b746b3d2e457e985b1442
    )
    FetchContent_MakeAvailable(plume_dxc)

    _plume_get_dxc_host_arch(PLUME_DXC_HOST_ARCH)

    # Set up DXC paths based on platform
    if(WIN32)
        set(PLUME_DXC_EXECUTABLE "${plume_dxc_SOURCE_DIR}/bin/x64/dxc.exe" CACHE INTERNAL "DXC executable")
        set(PLUME_DXC_LIB_DIR "" CACHE INTERNAL "DXC library directory")

        # Copy DLLs that must be next to the executable
        if(EXISTS "${plume_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll")
            configure_file("${plume_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll" "${CMAKE_BINARY_DIR}/bin/dxcompiler.dll" COPYONLY)
        endif()

        if(EXISTS "${plume_dxc_SOURCE_DIR}/bin/x64/dxil.dll")
            configure_file("${plume_dxc_SOURCE_DIR}/bin/x64/dxil.dll" "${CMAKE_BINARY_DIR}/bin/dxil.dll" COPYONLY)
        endif()
    elseif(APPLE)
        set(PLUME_DXC_EXECUTABLE "${plume_dxc_SOURCE_DIR}/bin/${PLUME_DXC_HOST_ARCH}/dxc-macos" CACHE INTERNAL "DXC executable")
        set(PLUME_DXC_LIB_DIR "${plume_dxc_SOURCE_DIR}/lib/${PLUME_DXC_HOST_ARCH}" CACHE INTERNAL "DXC library directory")

        # Ensure executable permission
        if(EXISTS "${PLUME_DXC_EXECUTABLE}")
            execute_process(COMMAND chmod +x "${PLUME_DXC_EXECUTABLE}")
        else()
            message(FATAL_ERROR "DXC not found at ${PLUME_DXC_EXECUTABLE}")
        endif()
    else()
        # Linux
        set(PLUME_DXC_EXECUTABLE "${plume_dxc_SOURCE_DIR}/bin/${PLUME_DXC_HOST_ARCH}/dxc-linux" CACHE INTERNAL "DXC executable")
        set(PLUME_DXC_LIB_DIR "${plume_dxc_SOURCE_DIR}/lib/${PLUME_DXC_HOST_ARCH}" CACHE INTERNAL "DXC library directory")

        # Ensure executable permission
        if(EXISTS "${PLUME_DXC_EXECUTABLE}")
            execute_process(COMMAND chmod +x "${PLUME_DXC_EXECUTABLE}")
        else()
            message(FATAL_ERROR "DXC not found at ${PLUME_DXC_EXECUTABLE}")
        endif()
    endif()

    _plume_setup_dxc_options()
endfunction()

# Get the DXC command with proper environment setup
function(plume_get_dxc_command OUT_VAR)
    if(NOT DEFINED PLUME_DXC_EXECUTABLE)
        message(FATAL_ERROR "PLUME_DXC_EXECUTABLE not set. Call plume_shaders_init() or set PLUME_DXC_EXECUTABLE manually.")
    endif()

    # Ensure options are set up
    _plume_setup_dxc_options()

    if(WIN32)
        set(${OUT_VAR} "${PLUME_DXC_EXECUTABLE}" PARENT_SCOPE)
    elseif(APPLE)
        if(PLUME_DXC_LIB_DIR)
            set(${OUT_VAR} "${CMAKE_COMMAND}" -E env "DYLD_LIBRARY_PATH=${PLUME_DXC_LIB_DIR}" "${PLUME_DXC_EXECUTABLE}" PARENT_SCOPE)
        else()
            set(${OUT_VAR} "${PLUME_DXC_EXECUTABLE}" PARENT_SCOPE)
        endif()
    else()
        if(PLUME_DXC_LIB_DIR)
            set(${OUT_VAR} "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${PLUME_DXC_LIB_DIR}" "${PLUME_DXC_EXECUTABLE}" PARENT_SCOPE)
        else()
            set(${OUT_VAR} "${PLUME_DXC_EXECUTABLE}" PARENT_SCOPE)
        endif()
    endif()
endfunction()
