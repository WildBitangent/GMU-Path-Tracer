#include "structs.h"

////////////////////////////////////////////

struct State
{
    Ray ray;
    float3 hitPoint;
    float3 baryCoord;
    Triangle tri;
};

////////////////////////////////////////////

cbuffer Cam : register(b0)
{
	Camera cam;
};

////////////////////////////////////////////

RWByteAddressBuffer pathState : register(u1);
RWByteAddressBuffer queue : register(u2);
RWByteAddressBuffer queueCounters : register(u3);

StructuredBuffer<BVHNode> tree : register(t0);
StructuredBuffer<Triangle> indices : register(t1);
Buffer<float3> vertices : register(t2);
StructuredBuffer<Light> lights : register(t3);

////////////////////////////////////////////

static State state;

////////////////////////////////////////////


bool rayTriangleIntersection(float3 v0, float3 v1, float3 v2, inout float distance)
{
    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;
	
    float3 pvec = cross(state.ray.direction, e2);
    float det = dot(e1, pvec);

    if (det > -EPSILON && det < EPSILON)
        return false;
		
    float invDet = 1.0 / det;
    float3 tvec = state.ray.origin - v0;
    float u = dot(tvec, pvec) * invDet;
	
    if (u < 0.0f || u > 1.0f)
        return false;
		
    float3 qvec = cross(tvec, e1);
    float v = dot(state.ray.direction, qvec) * invDet;
	
    if (v < 0.0 || u + v > 1.0)
        return false;
	
    float t = dot(e2, qvec) * invDet;
	
    if (t >= 0/*t > EPSILON && t < 1 / EPSILON*/) // ray intersection
    {
        float3 pp = state.ray.origin + state.ray.direction * t;

        if (t < distance)
        {
            distance = t;
            state.hitPoint = pp;
            state.baryCoord = float3(1 - u - v, u, v);
            return true;
        }
    }
    return false;
}

float rayAABBIntersection(float3 minbox, float3 maxbox, Ray r)
{
    float3 invdir = 1.0 / r.direction;

    float3 f = (maxbox - r.origin) * invdir;
    float3 n = (minbox - r.origin) * invdir;

    float3 tmax = max(f, n);
    float3 tmin = min(f, n);

    float t1 = min(tmax.x, min(tmax.y, tmax.z));
    float t0 = max(tmin.x, max(tmin.y, tmin.z));

	//return t0 <= t1 && t1 >= 0.0;
    return (t1 >= t0) ? (t0 > 0.f ? t0 : t1) : -1.0;
}

float rayBVHIntersection()
{	
    int stack[STACKSIZE];
    uint ptr = 0;
    stack[ptr++] = -1;
    float distance = FLT_MAX;

	// early test
    BVHNode node = tree[0];
    if (rayAABBIntersection(node.min, node.max, state.ray) > 0.0)
    {
        for (int idx = 0; idx > -1;)
        {
            node = tree[idx];

            if (node.isLeaf)
            {
                for (int i = node.leftIndex; i < node.rightIndex; i++)
                {
                    uint3 ii = indices[i].vtix;
                    float3 v0 = vertices[ii.x];
                    float3 v1 = vertices[ii.y];
                    float3 v2 = vertices[ii.z];

                    if (rayTriangleIntersection(v0, v1, v2, distance))
                        state.tri = indices[i];
                }
            }
            else
            {
                BVHNode left = tree[node.leftIndex];
                BVHNode right = tree[node.rightIndex];

                float leftHit = rayAABBIntersection(left.min, left.max, state.ray);
                float rightHit = rayAABBIntersection(right.min, right.max, state.ray);

                if (leftHit > 0.0 && rightHit > 0.0)
                {
                    int deferred;

                    if (leftHit > rightHit)
                    {
                        idx = node.rightIndex;
                        deferred = node.leftIndex;
                    }
                    else
                    {
                        idx = node.leftIndex;
                        deferred = node.rightIndex;
                    }

                    stack[ptr++] = deferred;
                    continue;
                }
                else if (leftHit > 0.0)
                {
                    idx = node.leftIndex;
                    continue;
                }
                else if (rightHit > 0.0)
                {
                    idx = node.rightIndex;
                    continue;
                }
            }
            idx = stack[--ptr];
        }
    }

    return distance;
}

void rayLightIntersection(inout uint lightIndex, inout float distance)
{
	for (uint i = 0; i < cam.lightCount; i++)
	{
		float3 position = lights[i].position - state.ray.origin;
		float radius2 = lights[i].radius * lights[i].radius;
			
		float tca = dot(position, state.ray.direction);
		float d2 = dot(position, position) - tca * tca;
			
		if (d2 > radius2)
			continue;
			
		float thc = sqrt(radius2 - d2);
		float t0 = tca - thc;
		float t1 = tca + thc;
 
		if (t0 < 0)
			t0 = t1; // if t0 is negative, let's use t1 instead
 
		if (t0 > 0.0f && t0 < distance)
		{
			distance = t0;
			lightIndex = i + 1;
		}
	}
}


[numthreads(NUM_THREADS, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex) // TODO does it need queue - isn't it just going through all the paths all the times
{
    uint stride = NUM_THREADS * NUM_GROUPS;
	//uint queueElementCount = queueCounters.Load(OFFSET_EXTRAY);

    for (int i = 0; i < ITERATIONS; i++)
    {
		// Always going through all paths
		uint queueIndex = tid + NUM_THREADS * gid.x + i * stride;
        //if (queueIndex >= queueElementCount)
        //    break;
		
		uint index = _queue_extRay;
		//uint index = queueIndex;

		state.ray.origin = _pstate_rayOrigin;
		state.ray.direction = _pstate_rayDirection;
		
        float distance = rayBVHIntersection(); // TODO maybe traverse more than one path
		
		if (distance < FLT_MAX)
        {
			_set_pstate_surfacePoint(state.hitPoint);
			_set_pstate_baryCoord(state.baryCoord);
			
			uint4 tri = uint4(state.tri.vtix, state.tri.materialID);
			_set_pstate_triangle(tri);
		}
		
		
		uint lightIndex = 0;
		rayLightIntersection(lightIndex, distance);
		
		_set_pstate_isEmitter(lightIndex);
		_set_pstate_hitDistance(distance);
	}
}