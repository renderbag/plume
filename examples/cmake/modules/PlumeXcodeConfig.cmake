# Configure Xcode-specific properties for code signing and team ID
# Usage: plume_apply_xcode_config(target_name)

function(plume_apply_xcode_config TARGET_NAME)
    if(NOT APPLE)
        return()
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    )

    if(DEFINED PLUME_XCODE_TEAM_ID AND PLUME_XCODE_TEAM_ID)
        set_target_properties(${TARGET_NAME} PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PLUME_XCODE_TEAM_ID}"
        )
        message(STATUS "Plume - Applied Team ID '${PLUME_XCODE_TEAM_ID}' to ${TARGET_NAME}")
    endif()
endfunction()
