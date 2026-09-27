# Shared helpers for building host tools from an iOS cross-compile

function(_plume_get_host_build_type OUT_VAR)
    if(CMAKE_BUILD_TYPE)
        set(${OUT_VAR} "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "Release" PARENT_SCOPE)
    endif()
endfunction()

function(_plume_host_tool_env_prefix OUT_VAR)
    set(${OUT_VAR}
        ${CMAKE_COMMAND} -E env
        --unset=SDKROOT
        --unset=ARCHS
        --unset=IPHONEOS_DEPLOYMENT_TARGET
        --unset=MACOSX_DEPLOYMENT_TARGET
        --unset=PLATFORM_NAME
        --unset=EFFECTIVE_PLATFORM_NAME
        --unset=CC
        --unset=CXX
        --unset=CFLAGS
        --unset=CXXFLAGS
        --unset=LDFLAGS
        PARENT_SCOPE
    )
endfunction()

function(plume_add_host_tool OUT_HOST_OUTPUT_BIN TARGET_NAME TOOL_SOURCE HOST_PROJECT_DIR HOST_BUILD_DIR HOST_INSTALL_DIR)
    _plume_get_host_build_type(HOST_BUILD_TYPE)
    _plume_host_tool_env_prefix(HOST_ENV_PREFIX)

    file(MAKE_DIRECTORY "${HOST_PROJECT_DIR}")

    if(CMAKE_HOST_WIN32)
        set(HOST_EXE_SUFFIX ".exe")
    else()
        set(HOST_EXE_SUFFIX "")
    endif()

    set(HOST_OUTPUT_PATH "${HOST_INSTALL_DIR}/bin/${TARGET_NAME}${HOST_EXE_SUFFIX}")

    add_custom_command(OUTPUT "${HOST_OUTPUT_PATH}"
        COMMAND ${HOST_ENV_PREFIX}
        ${CMAKE_COMMAND} -S "${HOST_PROJECT_DIR}" -B "${HOST_BUILD_DIR}" -G "Unix Makefiles"
        -DCMAKE_BUILD_TYPE=${HOST_BUILD_TYPE}
        -DCMAKE_OSX_SYSROOT=macosx
        -DCMAKE_OSX_ARCHITECTURES=${CMAKE_HOST_SYSTEM_PROCESSOR}
        COMMAND ${HOST_ENV_PREFIX}
        ${CMAKE_COMMAND} --build "${HOST_BUILD_DIR}" --config ${HOST_BUILD_TYPE} --target ${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} --install "${HOST_BUILD_DIR}" --config ${HOST_BUILD_TYPE} --prefix "${HOST_INSTALL_DIR}"
        DEPENDS "${TOOL_SOURCE}"
        COMMENT "Building host ${TARGET_NAME} tool"
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(${TARGET_NAME} DEPENDS "${HOST_OUTPUT_PATH}")

    set(${OUT_HOST_OUTPUT_BIN} "${HOST_OUTPUT_PATH}" PARENT_SCOPE)
endfunction()
