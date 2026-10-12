[[vk::binding(0, 0)]] Texture2D<float4> colorTexture : register(t0);
[[vk::binding(1, 0)]] SamplerState colorSampler : register(s1);

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return colorTexture.SampleLevel(colorSampler, input.uv, 0.0);
}
