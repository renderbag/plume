// Pixel shader input (matching output from vertex shader)
struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// Pixel shader main function
float4 main(PSInput input) : SV_TARGET {
    return input.color;
} 