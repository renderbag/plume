# Plume

Plume is a modern, cross-platform rendering hardware interface (RHI) library that provides a unified API for multiple rendering backends:

- Vulkan
- Metal (macOS/iOS)
- Direct3D 12 (Windows)

## Features

- Unified rendering API for multiple backends
- Modern shader model (SM6.0+)
- Support for compute shaders
- Resource barriers for explicit synchronization
- Pipeline state objects
- Descriptor sets/layouts
- And more!

## Building

### Prerequisites

- CMake 3.20+
- C++17 compatible compiler
- SDL2 development libraries
- Platform-specific SDK:
  - Vulkan SDK (all platforms)
  - Metal SDK (macOS/iOS)
  - DirectX 12 SDK (Windows)

### Build Commands

```bash
# Create a build directory
mkdir build
cd build

# Configure with CMake
# For macOS, use pkgx to ensure SDL2 is available
pkgx +ninja +libsdl.org@2 cmake ..

# Build
cmake --build .
```

## Examples

### Triangle

A basic example demonstrating how to set up and render a simple colored triangle.

To run:

```bash
./bin/triangle
```

## License

See the [LICENSE](LICENSE) file for details.
