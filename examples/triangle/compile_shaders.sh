#!/bin/bash

# Make sure we're in the right directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create output directory if it doesn't exist
mkdir -p shaders

# Check if glslangValidator is available
if ! command -v glslangValidator &> /dev/null; then
    echo "Error: glslangValidator not found. Please install the Vulkan SDK."
    exit 1
fi

# Compile vertex shader
echo "Compiling vertex shader..."
glslangValidator -V shaders/triangle.vert -o shaders/triangle.vert.spv

# Compile fragment shader
echo "Compiling fragment shader..."
glslangValidator -V shaders/triangle.frag -o shaders/triangle.frag.spv

echo "Shader compilation complete." 