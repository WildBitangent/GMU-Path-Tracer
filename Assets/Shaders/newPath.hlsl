#include "structs.h"
#include "bsdf.h"
#include "random.h"

////////////////////////////////////////////

RWStructuredBuffer<PathState> pathState : register(u1);
RWStructuredBuffer<Queue> queue : register(u2);
RWByteAddressBuffer queueCounters : register(u3);

////////////////////////////////////////////


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;
	uint queueElementCount = queueCounters.Load(OFFSET_NEWPATH);
	
	seed = giseed.xy;
	
    for (int i = 0; i < 16; i++)
    {
        uint queueIndex = tid + 256 * gid.x + i * stride;
        if (queueIndex >= queueElementCount)
            break;
		
		int index = queue[queueIndex].newPath;
        uint2 coord;
        uint width = (1 / cam.pixelSize.x);
        uint height = (1 / cam.pixelSize.y);

        coord = (index < width * height) ? uint2(index % width, index * cam.pixelSize.x) : uint2(rand() * width, rand() * height); // TODO try uniform
		float2 jitter = float2(rand(), rand()) * 2 - 1;
        float2 uv = (coord + jitter) * cam.pixelSize;

        pathState[index].ray = Ray::create(cam.pos, normalize(cam.ulc + uv.x * cam.horizontal - uv.y * cam.vertical));
        pathState[index].screenCoord = coord;
        pathState[index].radiance = float3(0, 0, 0);
        pathState[index].throughtput = float3(1, 1, 1);
		pathState[index].lightThroughput = float3(1, 1, 1);
        pathState[index].inShadow = true;
        pathState[index].pathLength = 0;

		// expecting that new path is running always as first
		queue[queueIndex].extensionRay = index;
	}
	
	// update extension queue count
	if (gid.x + tid == 0)
		queueCounters.Store(OFFSET_EXTRAY, queueElementCount);
}