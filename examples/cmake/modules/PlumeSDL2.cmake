# PlumeSDL2.cmake
# Finds or fetches SDL2 for Plume examples

include(FetchContent)

set(PLUME_SDL2_VERSION "2.30.10" CACHE STRING "SDL2 version for auto-download on Windows")

function(plume_find_sdl2)
    if(NOT DEFINED PLUME_SDL2_DIR AND DEFINED ENV{PLUME_SDL2_DIR})
        set(PLUME_SDL2_DIR "$ENV{PLUME_SDL2_DIR}")
    endif()

    # If PLUME_SDL2_DIR is explicitly provided, use it directly
    # Expected layout: PLUME_SDL2_DIR/include/SDL.h, PLUME_SDL2_DIR/lib/x64/SDL2.lib
    if(DEFINED PLUME_SDL2_DIR AND EXISTS "${PLUME_SDL2_DIR}/include")
        message(STATUS "Plume - Using SDL2 from PLUME_SDL2_DIR: ${PLUME_SDL2_DIR}")
        _plume_sdl2_setup_from_dir("${PLUME_SDL2_DIR}")
        return()
    endif()

    # Try find_package (vcpkg, Homebrew, apt, etc.)
    find_package(SDL2 QUIET)

    if(SDL2_FOUND OR TARGET SDL2::SDL2)
        message(STATUS "Plume - Found SDL2 via find_package")

        if(TARGET SDL2::SDL2)
            if(TARGET SDL2::SDL2main)
                set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main PARENT_SCOPE)
            else()
                set(SDL2_LIBRARIES SDL2::SDL2 PARENT_SCOPE)
            endif()
        else()
            set(SDL2_LIBRARIES "${SDL2_LIBRARIES}" PARENT_SCOPE)
        endif()

        if(DEFINED SDL2_INCLUDE_DIRS)
            set(SDL2_INCLUDE_DIRS "${SDL2_INCLUDE_DIRS}" PARENT_SCOPE)
        endif()

        set(PLUME_SDL2_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # Auto-download SDL2 development libraries on Windows
    if(WIN32)
        message(STATUS "Plume - SDL2 not found, downloading v${PLUME_SDL2_VERSION}...")

        if(POLICY CMP0135)
            cmake_policy(SET CMP0135 NEW)
        endif()

        FetchContent_Declare(
            plume_sdl2
            URL "https://github.com/libsdl-org/SDL/releases/download/release-${PLUME_SDL2_VERSION}/SDL2-devel-${PLUME_SDL2_VERSION}-VC.zip"
        )
        FetchContent_MakeAvailable(plume_sdl2)

        set(_sdl2_src "${plume_sdl2_SOURCE_DIR}")

        if(NOT EXISTS "${_sdl2_src}/include" AND EXISTS "${_sdl2_src}/SDL2-${PLUME_SDL2_VERSION}/include")
            set(_sdl2_src "${_sdl2_src}/SDL2-${PLUME_SDL2_VERSION}")
        endif()

        _plume_sdl2_setup_from_dir("${_sdl2_src}")
        return()
    endif()

    message(FATAL_ERROR
        "SDL2 not found. Please install SDL2 or pass -DPLUME_SDL2_DIR=<path-to-SDL2>"
    )
endfunction()

# Set up SDL2 paths from a directory with include/ and lib/<arch>/ subdirectories
macro(_plume_sdl2_setup_from_dir _sdl2_root)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_plume_sdl2_arch "x64")
    else()
        set(_plume_sdl2_arch "x86")
    endif()

    set(SDL2_INCLUDE_DIRS "${_sdl2_root}/include" PARENT_SCOPE)
    set(SDL2_BINDIR "${_sdl2_root}/lib/${_plume_sdl2_arch}" PARENT_SCOPE)

    if(WIN32)
        set(SDL2_LIBRARIES "${SDL2_BINDIR}/SDL2.lib;${SDL2_BINDIR}/SDL2main.lib" PARENT_SCOPE)
    else()
        set(SDL2_LIBRARIES "${SDL2_BINDIR}/libSDL2.a;${SDL2_BINDIR}/libSDL2main.a" PARENT_SCOPE)
    endif()
    set(PLUME_SDL2_FOUND TRUE PARENT_SCOPE)
    unset(_plume_sdl2_arch)
endmacro()
