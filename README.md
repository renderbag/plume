# Plume Render Hardware Interface (RHI)

A lightweight cross-platform rendering abstraction layer supporting Vulkan, Direct3D 12, and Metal.

## Overview

Plume RHI provides a unified API for modern graphics APIs, allowing you to write rendering code once and run it on multiple platforms with minimal changes. It supports:

- Windows: Direct3D 12 and Vulkan
- macOS/iOS: Metal and MoltenVK (Vulkan)
- Linux: Vulkan

## Features

- Modern rendering pipeline with explicit synchronization
- Unified resource handling (buffers, textures, samplers)
- Shader compilation and reflection
- Efficient descriptor set management
- Compute shader support
- Low-level access to native API objects when needed

## Building

### Prerequisites

- CMake 3.20 or newer
- C++17 compatible compiler
- Platform-specific SDK:
  - Windows: Windows SDK, DirectX SDK, Vulkan SDK
  - macOS: Xcode with Metal support, Vulkan SDK (optional)
  - Linux: Vulkan SDK

### Build Steps

```bash
# Clone the repository
git clone https://github.com/your-username/plume.git
cd plume

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
cmake --build .
```

### Build Options

- `PLUME_STATIC`: Build as a static library (default: OFF)
- `PLUME_BUILD_EXAMPLES`: Build example applications (default: ON)

## Examples

The repository includes several examples demonstrating how to use the Plume RHI:

- `triangle_example`: A simple colored triangle, the "Hello World" of graphics programming
- More examples coming soon!

## License

This project is licensed under the MIT License - see the LICENSE file for details.
