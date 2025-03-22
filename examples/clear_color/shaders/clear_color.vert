#version 450

// Vertex input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Vertex output
layout(location = 0) out vec2 outTexCoord;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;
} 