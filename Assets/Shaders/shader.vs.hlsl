//cbuffer Camera
//{
//    float4x4 view;
//    float4x4 proj;
//    float4x4 viewProj;
//};

//cbuffer Model
//{
//    float4x4 model;
//};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

//VS_OUTPUT main(float4 inPos : POSITION, float4 inTexCoord : TEXCOORD)
VS_OUTPUT main(uint vI : SV_VERTEXID)
{
    VS_OUTPUT output;
	
	float2 texcoord = float2(vI & 1, vI >> 1);

	output.pos = float4((texcoord.x - 0.5f) * 2, -(texcoord.y - 0.5f) * 2, 0, 1);
	output.texCoord = texcoord;

	return output;
 //   
 //   float4x4 MVP = mul(model, viewProj);
 //   output.pos = mul(inPos, MVP);
 //   //output.texCoord = inTexCoord;

	//return output;
}