#include "structs.h"
#include "random.h"

////////////////////////////////////////////

cbuffer Material : register(b1)
{
    MaterialProperty materialProp[5]; // hopefully there won't be more than 128 materials (dx12 has unbound descriptors)
};

////////////////////////////////////////////
globallycoherent RWTexture2DArray<float4> output : register(u0);
RWStructuredBuffer<PathState> pathState : register(u1); // TODO registers
RWStructuredBuffer<Queue> queue : register(u2);
RWByteAddressBuffer queueCounters : register(u3);

StructuredBuffer<BVHNode> tree : register(t0);
StructuredBuffer<Triangle> indices : register(t1);
Buffer<float3> vertices : register(t2);
StructuredBuffer<Light> lights : register(t3);
StructuredBuffer<TriangleParameters> triParams : register(t4);

Texture2DArray diffuse : register(t5);
Texture2DArray metallicRoughness : register(t6);
Texture2DArray normals : register(t7);
SamplerState samplerState : register(s0);

////////////////////////////////////////////

static bool pathEliminated;

void endPath(in float3 radiance, in uint index)
{	
	// write to the newpath queue
	uint ballot = NvBallot(pathEliminated);
	uint count = countbits(ballot);
	uint offset = 0;
	uint qindex = NvWaveMultiPrefixExclusiveAdd(1, ballot);

	if (NvGetLaneId() == 0)
		queueCounters.InterlockedAdd(OFFSET_NEWPATH, count, offset);
	
	broadcast(offset);
	
	// only threads in warp with path being eliminated
	if (pathEliminated)
	{
		radiance = saturate(radiance);

		uint2 coord = pathState[index].screenCoord;

		float3 pixel = output[uint3(coord, 0)].rgb;
		uint sampleCount = asuint(output[uint3(coord, 0)].a);

		// kahan summation
		float3 sum = pixel * sampleCount++;
		float3 c = output[uint3(coord, 1)];
		
		float3 y = radiance - c;
		float3 t = sum + y;
	
		// write out
		//output[uint3(coord, 0)] = float4(t / sampleCount, asfloat(sampleCount)); // probably race condition, but nothing I can do with it atm
		output[uint3(coord, 0)] = float4(radiance, asfloat(sampleCount)); // probably race condition, but nothing I can do with it atm
		output[uint3(coord, 1)] = float4((t - sum) - y, 0);
		AllMemoryBarrier();
		queue[offset + qindex].newPath = index;
	}
}

uint setMaterialHitProperties(in uint index)
{
    uint3 indices = pathState[index].tri.vtix;
    uint materialID = pathState[index].tri.materialID;
    float3 baryCoord = pathState[index].baryCoord;

    float2 t0 = triParams[indices.x].texCoord;
    float2 t1 = triParams[indices.y].texCoord;
    float2 t2 = triParams[indices.z].texCoord;

    float3 n0 = triParams[indices.x].normal;
    float3 n1 = triParams[indices.y].normal;
    float3 n2 = triParams[indices.z].normal;

	float2 texCoord = t0 * baryCoord.x + t1 * baryCoord.y + t2 * baryCoord.z;
    float3 normal = n0 * baryCoord.x + n1 * baryCoord.y + n2 * baryCoord.z;
	
    MaterialProperty material = materialProp[materialID];
    //state.material.roughness = saturate(state.material.roughness + cam.sampleCounter * 0.0001);
	
    if (material.diffuseIndex >= 0)
        material.baseColor = diffuse.SampleLevel(samplerState, float3(texCoord, material.diffuseIndex), 0);

    if (material.metallicRoughnesIndex >= 0)
    {
        float2 data = metallicRoughness.SampleLevel(samplerState, float3(texCoord, material.metallicRoughnesIndex), 0);
        material.metallic = data.x;
        material.roughness = data.y;
    }

    if (material.normalIndex >= 0)
    {
        float3 data = normals.SampleLevel(samplerState, float3(texCoord, material.normalIndex), 0);
        data = data * 2.0 - 1.0; // TODO maybe normalize

		// flip the normal, if the ray is coming from behind
        float3 rayDirection = pathState[index].ray.direction;
        float3 ortNormal = dot(normal, rayDirection) <= 0.0 ? normal : normal * -1.0;

		// orthonormal basis
        float3 up = abs(ortNormal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, ortNormal));
        float3 bitangent = cross(ortNormal, tangent);
		
        normal = tangent * data.x + bitangent * data.y + ortNormal * data.z; // TODO normalize?
    }

    material.roughness = max(0.014, material.roughness); // gotta clip roughness - floating point precission

    pathState[index].material.baseColor = material.baseColor;
    pathState[index].material.metallic = material.metallic;
    pathState[index].material.roughness = material.roughness;
	//pathState[index].normal = normal;
	pathState[index].normal = normal;

    return material.materialType;
}

void createShadowRay(in uint index)
{
	uint lightIndex = uint(rand() * 2); // TODO change: 2 = num of lights
	
	// set shadow ray
    float3 normal = pathState[index].normal;
    float3 surfacePos = pathState[index].surfacePoint + normal * EPSILON;
    float3 lightDir = lights[lightIndex].position - surfacePos;
    float distance = length(lightDir);

    lightDir = normalize(lightDir);

	// write state
	pathState[index].lightIndex = lightIndex;
    pathState[index].shadowRay = Ray::create(surfacePos, lightDir);
    pathState[index].lightDistance = distance - EPSILON;
}


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;	
	
	//for (uint i = 0; i < 16; i++)
	//{
	//	uint index = tid + 256 * gid.x + i * stride;
	//	seed = float2(frac(index * INVPI), frac(index * PI));
	//	index += gid.x * 7;
	//	//for (uint x = 0; x < 210; x++)
	//	//	createShadowRay(x * 143);
		
	//	uint width = 1280;
	//	uint2 coord = uint2(index % 1280, index / 1280);
	//	output[uint3(coord.x, coord.y % 720, 0)] = float4(coord.x / 1280.0, coord.y / (720.0 * 4), 0, asfloat(0));
	//	AllMemoryBarrier();
	//}
	
	//return;

    for (uint i = 0; i < 16; i++)
	{
		uint index = tid + 256 * gid.x + i * stride;
		//uint index = giseed.x + stride * i;
		
		//if (index >= 1280 * 720)
		//	break;
		
		// camera moved - resets accumulation buffer and generate new paths
		if (cam.sampleCounter == 0)
		{
			uint width = 1280;
			uint2 coord = uint2(index % width, index / width);
			
			if (index < 1280 * 720)
			{
				if (index == 0)
					queueCounters.Store(OFFSET_NEWPATH, PATHCOUNT);
				
				float3 color = output[uint3(coord, 0)];
				output[uint3(coord, 0)] = float4(color, asfloat(0));
				output[uint3(coord, 1)] = float4(0, 0, 0, 0);
			}
			
			queue[index].newPath = index;
		}
		else
		{
			seed = float2(frac(index * INVPI), frac(index * PI));
			pathEliminated = false;

			float3 throughput = pathState[index].throughtput;
			float3 radiance = pathState[index].radiance;
		
			// accunmulate from previous path
			if (!pathState[index].inShadow)
				radiance += pathState[index].directLight * throughput;
		
			// update throughput
			throughput *= pathState[index].lightThroughput;

			// eliminate path with zero throughput
			if (all(throughput <= 1e-8)) // TODO tweak const
				pathEliminated = true;

			// eliminate path out of scene
			if (pathState[index].hitDistance == FLT_MAX)
			{
				radiance = float3(1, 1, 1) * throughput * 0.3;
				pathEliminated = true;
			}
			
			// russian roulette
			if (pathState[index].pathLength > 20) // todo tweak
			{
				float p = max(throughput.x, max(throughput.y, throughput.z));
				if (rand() > p * 0.004)
					pathEliminated = true;

				throughput *= 1 / p;
			}
			
			
			if (pathState[index].pathLength > 0)
				pathEliminated = true;

			// find paths for elimination
			if (any(pathEliminated))
				endPath(radiance, index);

			// enqueue materials
			int materialType = pathEliminated ? -1 : setMaterialHitProperties(index);
			uint ue4Ballot = NvBallot(materialType == 0);
			uint glassBallot = NvBallot(materialType == 1);
			uint ue4Index = NvWaveMultiPrefixExclusiveAdd(1, ue4Ballot);
			uint glassIndex = NvWaveMultiPrefixExclusiveAdd(1, glassBallot);
        
			uint ue4Count = countbits(ue4Ballot);
			uint glassCount = countbits(glassBallot);
			uint ue4Offset = 0;
			uint glassOffset = 0;

			if (NvGetLaneId() == 0) // Todo use of shared memory, and flushing at once
			{
				queueCounters.InterlockedAdd(OFFSET_MATUE4, ue4Count, ue4Offset);
				queueCounters.InterlockedAdd(OFFSET_MATGLASS, glassCount, glassOffset);
			}

			broadcast(ue4Offset);
			broadcast(glassOffset);

			if (materialType == 0)
				queue[ue4Offset + ue4Index].materialUE4 = index;
			else if (materialType == 1)
				queue[glassOffset + glassIndex].materialGlass = index;
		
			// update path only if it's alive
			if (!pathEliminated)
			{
				// pick light and craete shadow ray
				createShadowRay(index);

				pathState[index].radiance = radiance;
				pathState[index].throughtput = throughput;
				pathState[index].pathLength++;
				pathState[index].inShadow = true;
			}
		}
	}

	//// reset extension and shadow ray queues
	//if (tid + gid.x == 0)
	//	queueCounters.Store4(OFFSET_EXTRAY, uint4(0, 0, 0, 0));
}