// Vertex input structure
struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

// Vertex output structure
struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// Vertex shader main function
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
} 