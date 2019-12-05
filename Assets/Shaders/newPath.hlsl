#include "structs.h"
#include "bsdf.h"
#include "random.h"

////////////////////////////////////////////

RWByteAddressBuffer pathState : register(u1);
RWStructuredBuffer<Queue> queue : register(u2);
RWByteAddressBuffer queueCounters : register(u3);

////////////////////////////////////////////


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;
	uint queueElementCount = queueCounters.Load(OFFSET_QC_NEWPATH);
	uint lastPath = queueCounters.Load(OFFSET_QC_LASTPATHCNT);
	//seed = giseed.xy;
	
    for (int i = 0; i < 16; i++)
    {
		uint queueIndex = tid + 256 * gid.x + i * stride;		
		if (queueIndex >= queueElementCount)
            break;
		
		seed = float2(frac(queueIndex * INVPI), frac(queueIndex * PI));
		
		uint index = queue[queueIndex].newPath;
        uint2 coord;
		//uint width = (1.0 / cam.pixelSize.x);
		//uint height = (1.0 / cam.pixelSize.y);
		uint width = 1280;
		uint height = 720;

        //coord = (index < width * height) ? uint2(index % width, index * cam.pixelSize.x) : uint2(rand() * width, rand() * height); // TODO try uniform
		uint newIndex = (lastPath + queueIndex) % (width * height);
		coord = uint2(newIndex % width, newIndex / width);
		//coord = uint2(newIndex % width, (newIndex / width) % height);
		
		float2 jitter = float2(rand(), rand()) * 2 - 1;
		float2 uv = (coord + jitter) * cam.pixelSize;
		//float2 uv = (coord) * cam.pixelSize;

		Ray extRay = Ray::create(cam.pos, normalize(cam.ulc + uv.x * cam.horizontal - uv.y * cam.vertical));
		
		_set_pstate_rayOrigin(extRay.origin);
		_set_pstate_rayDirection(extRay.direction);
		_set_pstate_screenCoord(coord);
		_set_pstate_radiance(float3(0, 0, 0));
		_set_pstate_throughput(float3(1, 1, 1));
		_set_pstate_lightThroughput(float3(1, 1, 1));
		_set_pstate_pathLength(0);
		_set_pstate_inShadow(true);

		// expecting that new path is running always as first
		queue[queueIndex].extensionRay = index;
	}
	
	// update extension queue count, set offsets for UE4 and GLASS, and finally reset shadowCounts
	if (gid.x + tid == 0) // TODO this ain't really scalable for more materials
	{
		uint ue4Offset = queueElementCount;
		uint glassOffset = ue4Offset + queueCounters.Load(OFFSET_QC_MATUE4);
		queueCounters.Store3(OFFSET_QC_EXTRAY_UE4_OFFSET, uint3(ue4Offset, glassOffset, 0));
	}
}