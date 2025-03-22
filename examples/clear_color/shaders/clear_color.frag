#version 450

// Parameters buffer
layout(set = 0, binding = 0) uniform ClearColorParams {
    vec4 color;
} params;

// Fragment input
layout(location = 0) in vec2 inTexCoord;

// Fragment output
layout(location = 0) out vec4 outColor;

void main() {
    // Apply a simple gradient based on texture coordinates
    vec4 color = params.color;
    color.rgb *= vec3(inTexCoord.x, inTexCoord.y, 1.0);
    outColor = color;
} 