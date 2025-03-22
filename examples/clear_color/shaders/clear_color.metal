#include <metal_stdlib>
using namespace metal;

// Vertex input
struct VertexInput {
    float3 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

// Vertex to fragment data
struct VertexOutput {
    float4 position [[position]];
    float2 texCoord;
};

// Shader parameters
struct ClearColorParams {
    float4 color;
};

// Vertex shader
vertex VertexOutput vertexMain(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

// Fragment shader
fragment float4 fragmentMain(VertexOutput in [[stage_in]],
                         constant ClearColorParams& params [[buffer(0)]]) {
    // Apply a simple gradient based on texture coordinates
    float4 color = params.color;
    color.rgb *= float3(in.texCoord.x, in.texCoord.y, 1.0);
    return color;
} 