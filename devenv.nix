{
  pkgs,
  lib,
  inputs,
  ...
}:
{
  # https://devenv.sh/basics/

  # https://devenv.sh/packages/
  packages =
    (with pkgs; [
      # Common development tools
      cmake
      ninja
      pkg-config
      git

      # Graphics development dependencies
      vulkan-headers
      vulkan-loader
      vulkan-tools
      spirv-tools
      spirv-headers
      glslang

      # Shader compilers
      shaderc
      directx-shader-compiler

      # Vulkan validation
      vulkan-validation-layers

      # Math library for examples
      glm
    ])
    # macOS specific packages
    ++ lib.optionals pkgs.stdenv.isDarwin [
      pkgs.moltenvk
    ]
    # SDL2 from pinned nixpkgs
    ++ [ inputs.nixpkgs-sdl2.legacyPackages.${pkgs.system}.SDL2 ]
    ++ lib.optionals pkgs.stdenv.isLinux (with pkgs; [
      # Linux specific packages
      clang
      clang-tools
      lldb
      libGL
      libxkbcommon
      wayland
      wayland-protocols
      mesa
      xorg.libX11
      xorg.libXrandr
      xorg.libXi
    ])
    # Note: On macOS, we use the system SDK via Xcode, so we don't need
    # to include Darwin frameworks from nixpkgs here.
    ;

  # Environment variables
  env = {
    # CMake build type for development
    CMAKE_BUILD_TYPE = "Debug";

    # Help CMake find SDL2 and GLM (using pinned SDL2)
    SDL2_ROOT = "${inputs.nixpkgs-sdl2.legacyPackages.${pkgs.system}.SDL2}";
    CMAKE_PREFIX_PATH = "${inputs.nixpkgs-sdl2.legacyPackages.${pkgs.system}.SDL2}:${pkgs.glm}";
  } // lib.optionalAttrs pkgs.stdenv.isLinux {
    # Vulkan environment for Linux
    VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
    VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation";
  } // lib.optionalAttrs pkgs.stdenv.isDarwin {
    # Vulkan environment for macOS (MoltenVK from nixpkgs)
    VK_ICD_FILENAMES = "${pkgs.moltenvk}/share/vulkan/icd.d/MoltenVK_icd.json";
    VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
  };

  # https://devenv.sh/scripts/
  scripts = {
    configure.exec = ''
      echo "Configuring build with CMake..."
      cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPLUME_BUILD_EXAMPLES=ON
    '';

    build.exec = ''
      echo "Building project..."
      cmake --build build
    '';

    clean.exec = ''
      echo "Cleaning build directory..."
      rm -rf build
    '';

    run-triangle.exec = ''
      echo "Running triangle example..."
      ${if pkgs.stdenv.isDarwin then ''
        export VK_ICD_FILENAMES="${pkgs.moltenvk}/share/vulkan/icd.d/MoltenVK_icd.json"
        export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
        export DYLD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:''${DYLD_LIBRARY_PATH:-}"
      '' else ""}
      ./build/bin/plume_triangle
    '';

    run-cube.exec = ''
      echo "Running cube example..."
      ${if pkgs.stdenv.isDarwin then ''
        export VK_ICD_FILENAMES="${pkgs.moltenvk}/share/vulkan/icd.d/MoltenVK_icd.json"
        export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
        export DYLD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:''${DYLD_LIBRARY_PATH:-}"
      '' else ""}
      ./build/bin/plume_cube
    '';
  };

  # https://devenv.sh/languages/
  # On macOS we use Xcode's clang via enterShell PATH override
  languages = lib.optionalAttrs pkgs.stdenv.isLinux {
    c.enable = true;
    cplusplus.enable = true;
  };

  # On macOS, use Xcode toolchain instead of Nix's
  enterShell = lib.optionalString pkgs.stdenv.isDarwin ''
    export DEVELOPER_DIR="$(/usr/bin/readlink /var/db/xcode_select_link)"
    export SDKROOT="$DEVELOPER_DIR/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"

    # Use system clang instead of Nix's clang-wrapper
    export PATH="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin:$DEVELOPER_DIR/usr/bin:$PATH"

    # Use system xcrun instead of Nix's xcbuild version (avoids Metal compiler warnings)
    export PATH="${pkgs.runCommand "xcrun-wrapper" {} "mkdir -p $out/bin && ln -s /usr/bin/xcrun $out/bin/xcrun"}/bin:$PATH"
  '';
}
