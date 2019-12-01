struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

Texture2DArray Texture;
SamplerState Sampler;

float4 main(PS_INPUT input) : SV_TARGET
{
    return Texture.SampleLevel(Sampler, float3(input.texCoord, 0), 0);
    //return float4(1.0, 1.0, 1.0, 1.0);
}