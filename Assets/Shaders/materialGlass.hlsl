#include "structs.h"
#include "bsdf.h"
#include "random.h"

////////////////////////////////////////////

struct State
{
    Ray ray;
    float3 baseColor;
    float3 normal;
};

////////////////////////////////////////////

RWStructuredBuffer<PathState> pathState : register(u1);
RWStructuredBuffer<Queue> queue : register(u2);
globallycoherent RWByteAddressBuffer queueCounters : register(u3);

////////////////////////////////////////////


float3 glassSample(in State state)
{
    float3 normal = dot(state.normal, state.ray.direction) <= 0.0 ? state.normal : state.normal * -1.0;

	// refraction
    float n1 = 1.0;
    float n2 = 1.458; // Glass refraction

    float r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
	
    float theta = dot(-state.ray.direction, normal);
    
    float probability = schlickFresnel(r0, theta);

    float refractFactor = dot(state.normal, normal) > 0.0 ? (n1 / n2) : (n2 / n1); // decide where do we go, inside glass or from
    float3 transDirection = normalize(refract(state.ray.direction, normal, refractFactor));
    float cos2t = 1.0 - refractFactor * refractFactor * (1.0 - theta * theta);

    if (cos2t < 0.0 || rand() < probability)
        return normalize(reflect(state.ray.direction, normal));
        
    return transDirection;
}

[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;
	uint queueElementCount = queueCounters.Load(OFFSET_MATGLASS);
	uint extQueueOffset = queueCounters.Load(OFFSET_EXTRAY_GLASS_OFFSET);

	//seed = giseed.xy;
	
    for (int i = 0; i < 16; i++)
    {
        uint queueIndex = tid + 256 * gid.x + i * stride;
        if (queueIndex >= queueElementCount)
            break;
		
		seed = float2(frac(queueIndex * INVPI), frac(queueIndex * PI));
		uint index = queue[queueIndex].materialGlass;

        State state;
        Sample sample;

		// fill the state
		state.ray = pathState[index].ray;
		state.normal = pathState[index].normal;
		state.baseColor = pathState[index].material.baseColor;
		
        sample.bsdfDir = glassSample(state);
        
		pathState[index].lightThroughput = state.baseColor;

		// create extended ray
		float3 surfacePoint = pathState[index].surfacePoint;
        state.ray = Ray::create(surfacePoint + sample.bsdfDir * EPSILON, sample.bsdfDir);

        pathState[index].ray = state.ray;
		queue[extQueueOffset + queueIndex].extensionRay = index;
	}
	
	// update extension ray queue count
	//if (tid + gid.x == 0)
	//	queueCounters.Store(OFFSET_EXTRAY, extQueueOffset + queueElementCount);
}