#include "structs.h"
#include "random.h"

////////////////////////////////////////////

cbuffer Material : register(b1)
{
    MaterialProperty materialProp[128]; // hopefully there won't be more than 128 materials (dx12 has unbound descriptors)
};

////////////////////////////////////////////

RWTexture2DArray<float4> output : register(u0); // TODO probably globally coherent
RWByteAddressBuffer pathState : register(u1);
RWByteAddressBuffer queue : register(u2);
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
		queueCounters.InterlockedAdd(OFFSET_QC_NEWPATH, count, offset);
	
	broadcast(offset);
	
	// only threads in warp with path being eliminated
	if (pathEliminated)
	{
		radiance = saturate(radiance);
		
		// gamma correction
		radiance /= radiance + float3(1, 1, 1);
		radiance = pow(radiance, float3(1, 1, 1) / 2.2);

		uint2 coord = _pstate_screenCoord;

		float3 pixel = output[uint3(coord, 0)].rgb;
		uint sampleCount = asuint(output[uint3(coord, 0)].a);

		// kahan summation
		float3 sum = pixel * sampleCount++;
		float3 c = output[uint3(coord, 1)];
		
		float3 y = radiance - c;
		float3 t = sum + y;
	
		// write out
		output[uint3(coord, 0)] = float4(t / sampleCount, asfloat(sampleCount)); // probably race condition
		output[uint3(coord, 1)] = float4((t - sum) - y, 0);
		
		_set_queue_newPath(offset + qindex, index);
	}
}

uint setMaterialHitProperties(in uint index)
{
	uint4 tri = _pstate_triangle;
	float3 indices = tri.xyz;
	float3 baryCoord = _pstate_baryCoord;

    float2 t0 = triParams[indices.x].texCoord;
    float2 t1 = triParams[indices.y].texCoord;
    float2 t2 = triParams[indices.z].texCoord;

    float3 n0 = triParams[indices.x].normal;
    float3 n1 = triParams[indices.y].normal;
    float3 n2 = triParams[indices.z].normal;

	float2 texCoord = t0 * baryCoord.x + t1 * baryCoord.y + t2 * baryCoord.z;
    float3 normal = n0 * baryCoord.x + n1 * baryCoord.y + n2 * baryCoord.z;
	
    MaterialProperty material = materialProp[tri.w];
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
		float3 rayDirection = _pstate_rayDirection;
        float3 ortNormal = dot(normal, rayDirection) <= 0.0 ? normal : normal * -1.0;

		// orthonormal basis
        float3 up = abs(ortNormal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, ortNormal));
        float3 bitangent = cross(ortNormal, tangent);
		
        normal = tangent * data.x + bitangent * data.y + ortNormal * data.z; // TODO normalize?
    }

    material.roughness = max(0.014, material.roughness); // gotta clip roughness - floating point precission
	
	_set_pstate_matColor(material.baseColor.xyz);
	_set_pstate_matMetallicRoughness(float2(material.metallic, material.roughness));
	_set_pstate_normal(normal);
	
    return material.materialType;
}

void createShadowRay(in uint index)
{
	uint lightIndex = uint(rand() * cam.lightCount);
	
	// set shadow ray
	float3 normal = _pstate_normal;
	float3 surfacePos = _pstate_surfacePoint + normal * EPSILON_OFFSET;
    float3 lightDir = lights[lightIndex].position - surfacePos;
    float distance = length(lightDir);

    lightDir = normalize(lightDir);
	
	Ray shadowRay = Ray::create(surfacePos, lightDir);

	// write state
	_set_pstate_lightIndex(lightIndex);
	_set_pstate_shadowrayOrigin(shadowRay.origin);
	_set_pstate_shadowrayDirection(shadowRay.direction);
	_set_pstate_lightDistance(distance - EPSILON_OFFSET);
}

void clearTexture(in uint tid, in uint gid, in uint stride)
{
	for (uint i = 0; true; i++)
	{
		uint index = tid + threadCountX * gid + i * stride; // todo switch to dispatchID
		
		uint width = 1 / cam.pixelSize.x;
		uint height = 1 / cam.pixelSize.y;
		uint2 coord = uint2(index % width, index / width);
			
		if (index < width * height)
		{
			if (index == 0)
				queueCounters.Store(OFFSET_QC_NEWPATH, PATHCOUNT); // TODO make getters too?
				
			float3 color = output[uint3(coord, 0)];
			output[uint3(coord, 0)] = float4(color, asfloat(0));
			output[uint3(coord, 1)] = float4(0, 0, 0, 0);
		}
		else if (index >= PATHCOUNT) // texture is cleared, terminate loop
			break;
			
		if (index < PATHCOUNT)
			_set_queue_newPath(index, index);
	}
}

bool sampleLights(inout float3 radiance, in float3 throughput, in uint index) // todo used only for debugging purposes
{
	if (cam.sampleLights)
	{
		int lightIndex = -1;
		float closestHit = _pstate_hitDistance;
		
		for (uint i = 0; i < cam.lightCount; i++)
		{
			float3 position = lights[i].position - _pstate_rayOrigin;
			float radius2 = 0.01; // todo
			
			float tca = dot(position, _pstate_rayDirection);
			float d2 = dot(position, position) - tca * tca;
			
			if (d2 > radius2)
				continue;
			
			float thc = sqrt(radius2 - d2);
			float t0 = tca - thc;
			float t1 = tca + thc;
 
			if (t0 < 0)
				t0 = t1; // if t0 is negative, let's use t1 instead
 
			if (t0 > 0.0f && t0 < closestHit)
			{
				closestHit = t0;
				lightIndex = i;
			}
		}
		
		if (lightIndex > -1)
		{
			float3 emission = lights[lightIndex].emission;
			radiance += throughput * (emission / max(emission.x, max(emission.y, emission.z)));
			return true;
		}
	}
	
	return false;
}


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;	
	
	// camera moved - resets accumulation buffer and generate new paths
	if (cam.sampleCounter == 0)
	{
		clearTexture(tid, gid.x, stride);
	}
	else
	{
		for (uint i = 0; i < 16; i++)
		{
			uint index = tid + 256 * gid.x + i * stride;

			seed = float2(frac(index * INVPI), frac(index * PI));
			pathEliminated = false;

			float3 throughput = _pstate_throughput;
			float3 radiance = _pstate_radiance;
		
			// accunmulate from previous path
			if (!_pstate_inShadow)
				radiance += _pstate_directlight * throughput;
			
			if (sampleLights(radiance, throughput, index))
				pathEliminated = true;
		
			// update throughput
			throughput *= _pstate_lightThroughput;

			// eliminate path with zero throughput
			if (all(throughput <= 0)) // TODO tweak const
				pathEliminated = true;

			// eliminate path out of scene
			if (_pstate_hitDistance == FLT_MAX)
			{
				radiance += throughput * cam.envColor;
				pathEliminated = true;
			}
			
			// russian roulette
			if (_pstate_pathLength > 200) // todo tweak
			{
				float p = max(throughput.x, max(throughput.y, throughput.z));
				if (rand() > p * 0.004)
					pathEliminated = true;

				throughput *= 1 / p;
			}

			// find paths for elimination
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
				queueCounters.InterlockedAdd(OFFSET_QC_MATUE4, ue4Count, ue4Offset);
				queueCounters.InterlockedAdd(OFFSET_QC_MATGLASS, glassCount, glassOffset);
			}

			broadcast(ue4Offset);
			broadcast(glassOffset);

			if (materialType == 0)
			_set_queue_matUE4(ue4Offset + ue4Index, index);
			else if (materialType == 1)
			_set_queue_matGlass(glassOffset + glassIndex, index);
		
			// update path only if it's alive
			if (!pathEliminated)
			{
				// pick light and craete shadow ray
				createShadowRay(index);
				
				uint pathLength = _pstate_pathLength + 1;
				
				_set_pstate_radiance(radiance);
				_set_pstate_throughput(throughput);
				_set_pstate_pathLength(pathLength);
				_set_pstate_inShadow(true);
			}
		}
	}
}