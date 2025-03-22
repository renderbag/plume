#include <metal_stdlib>
using namespace metal;

// Vertex input
struct VertexInput {
    float3 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

// Vertex to fragment data
struct VertexOutput {
    float4 position [[position]];
    float4 color;
};

// Vertex shader
vertex VertexOutput vertexMain(VertexInput in [[stage_in]]) {
    VertexOutput out;
    out.position = float4(in.position, 1.0);
    out.color = in.color;
    return out;
}

// Fragment shader
fragment float4 fragmentMain(VertexOutput in [[stage_in]]) {
    return in.color;
} 