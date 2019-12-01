#include "structs.h"
#include "random.h"

////////////////////////////////////////////

cbuffer Material : register(b1)
{
    MaterialProperty materialProp[5]; // hopefully there won't be more than 128 materials (dx12 has unbound descriptors)
};

////////////////////////////////////////////
RWTexture2DArray<float4> output : register(u0);
RWStructuredBuffer<PathState> pathState : register(u1); // TODO registers
RWStructuredBuffer<Queue> queue : register(u2);
globallycoherent RWByteAddressBuffer queueCounters : register(u3);

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

void endPath(in float3 radiance, in uint index)
{
    radiance = saturate(radiance);

    uint2 coord = pathState[index].screenCoord;

    float3 pixel = output[uint3(coord, 0)].rgb;
    uint pathCount = asuint(output[uint3(coord, 0)].a);

	// kahan summation
    float3 sum = pixel * pathCount++;
    float3 c = output[uint3(coord, 1)];
		
    float3 y = radiance - c;
    float3 t = sum + y;
	
    // write out
    output[uint3(coord, 0)] = float4(t / (pathCount), asfloat(pathCount)); // probably race condition, but nothing I can do with it atm
	output[uint3(coord, 1)] = float4((t - sum) - y, 0);
	
	//write to the newpath qeueu
    uint activeThreads = NvActiveThreads();
    uint count = countbits(activeThreads);
    uint offset = 0;
    uint qindex = NvWaveMultiPrefixExclusiveAdd(1, activeThreads);

	if (NvGetLaneId() == firstbitlow(activeThreads))
		queueCounters.InterlockedAdd(OFFSET_NEWPATH, count, offset);

	broadcast(offset);
	
	//output[uint3(min(1279, (offset + qindex) % 1280), min(719, (offset + qindex) / 1280), 0)] = float4(1, 0.5, 0, 1);

	queue[offset + qindex].newPath = index;
}

uint setMaterialHitProperties(in uint index)
{
    uint3 indices = pathState[index].tri.vtix;
    uint materialID = pathState[index].tri.materialID;
    uint3 baryCoord = pathState[index].baryCoord;

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

    return material.materialType;
}

void createShadowRay(in uint index)
{
	// set shadow ray
    Light light;
    uint lightIndex = uint(rand() * 2); // TODO change: 2 = num of lights

    if (lightIndex == 0)
    {
        light.position = LIGHT;
        light.emission = EMISSION;
        light.radius = LIGHTRADIUS;
    }
    else
    {
        light.position = LIGHT2;
        light.emission = EMISSION2;
        light.radius = LIGHTRADIUS;
    }

    float3 normal = pathState[index].normal;
    float3 surfacePos = pathState[index].surfacePoint + normal * EPSILON;
    float3 lightDir = light.position - surfacePos;
    float distance = length(lightDir);

    lightDir = normalize(lightDir);

	// write state
    pathState[index].lightIndex = lightIndex;
    pathState[index].shadowRay = Ray::create(surfacePos, lightDir);
    pathState[index].lightDistance = distance - EPSILON;
    pathState[index].inShadow = false;
}


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID) // TODO REWRITE TO NON TERMINATING THREADS
{
    uint stride = threadCountX * numGroups;
	seed = giseed.xy;

    for (uint i = 0; i < 16; i++) // TODO prob less branching (remove all the continues)
    {
        uint index = tid + 256 * gid.x + i * stride;

        float3 throughput = pathState[index].throughtput;
        float3 radiance = pathState[index].radiance;
		
		// accunmulate from previous path
        if (!pathState[index].inShadow) 
            radiance += pathState[index].directLight * throughput;

		// eliminate path with zero throughput
        if (all(throughput < 1e-8)) // TODO tweak const
        {
            endPath(radiance, index);
            continue;
        }

		// eliminate path out of scene
        if (pathState[index].hitDistance == FLT_MAX)
        {
            radiance += float3(1, 1, 1) * throughput * 0.3;
            endPath(radiance, index);
            continue;
        }

		// update throughput
        throughput *= pathState[index].lightThroughput;

		// enqueue materials
        uint materialType = setMaterialHitProperties(index);
        uint ue4Ballot = NvBallot(materialType == 0);
        uint glassBallot = NvBallot(materialType == 1);
        uint ue4Index = NvWaveMultiPrefixExclusiveAdd(1, ue4Ballot);
        uint glassIndex = NvWaveMultiPrefixExclusiveAdd(1, glassBallot);
        
        uint ue4Count = countbits(ue4Ballot);
        uint glassCount = countbits(glassBallot);
        uint ue4Offset = 0;
        uint glassOffset = 0;

        if (NvGetLaneId() == firstbitlow(NvActiveThreads())) // Todo use of shared memory, and flushing at once
        {
            queueCounters.InterlockedAdd(OFFSET_MATUE4, ue4Count, ue4Offset);
			queueCounters.InterlockedAdd(OFFSET_MATGLASS, glassCount, glassOffset);
		}

        broadcast(ue4Offset);
        broadcast(glassOffset);

        if (materialType == 0)
			queue[ue4Offset + ue4Index].materialUE4 = index;
        else
			queue[glassOffset + glassIndex].materialGlass = index;

		// pick light and craete shadow ray
        createShadowRay(index);

		// russian roulette
        if (pathState[index].pathLength > 100)
        {
            float p = max(throughput.x, max(throughput.y, throughput.z));
            if (rand() > p * 0.004)
            {
                endPath(radiance, index);
                continue;
            }

            throughput *= 1 / p;
        }

        pathState[index].radiance = radiance;
        pathState[index].throughtput = throughput;
		pathState[index].pathLength++;
	}

	// reset extension and shadow ray queues
	if (tid + gid.x == 0)
		queueCounters.Store2(OFFSET_EXTRAY, uint2(0, 0));
}