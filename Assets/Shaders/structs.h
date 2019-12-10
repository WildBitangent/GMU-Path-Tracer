#define threadCountX 256
#define numGroups 512 // TODO is there better way?
#define FLT_MAX 3.402823466e+38
#define EPSILON 1e-8
#define EPSILON_OFFSET 1e-3
#define STACKSIZE 16 // stack with depth of 16 is enough even for scene with 1.5M triangles TODO mb change in future / use of shared memory and subgroups
#define PI 3.1415926535897932384626433832795
#define INVPI 0.31830988618379067153776752674503
#define PATHCOUNT 2097152 // 2^21 2M paths TODO define at compilation time

///////////////////////////////////////////////////
// define offsets for types
///////////////////////////////////////////////////
#define F4SO 16 * PATHCOUNT
#define F2SO 8 * PATHCOUNT
#define F1SO 4 * PATHCOUNT

///////////////////////////////////////////////////
// path state offsets
///////////////////////////////////////////////////
#define OFFSET_P_RAY_ORIGIN				0
#define OFFSET_P_RAY_DIRECTION			OFFSET_P_RAY_ORIGIN + F4SO
#define OFFSET_P_MAT_COLOR				OFFSET_P_RAY_DIRECTION + F4SO
#define OFFSET_P_MAT_METALICROUGHNESS	OFFSET_P_MAT_COLOR + F4SO
#define OFFSET_P_NORMAL					OFFSET_P_MAT_METALICROUGHNESS + F2SO
#define OFFSET_P_SURFACEPOINT			OFFSET_P_NORMAL + F4SO
#define OFFSET_P_BARYCOORD				OFFSET_P_SURFACEPOINT + F4SO
#define OFFSET_P_HITDISTANCE			OFFSET_P_BARYCOORD + F4SO
#define OFFSET_P_TRIANGLE				OFFSET_P_HITDISTANCE + F1SO

#define OFFSET_P_SHADOWRAY_ORIGIN		OFFSET_P_TRIANGLE + F4SO
#define OFFSET_P_SHADOWRAY_DIRECTION	OFFSET_P_SHADOWRAY_ORIGIN + F4SO
#define OFFSET_P_LIGHT_INDEX			OFFSET_P_SHADOWRAY_DIRECTION + F4SO
#define OFFSET_P_LIGHT_DISTANCE			OFFSET_P_LIGHT_INDEX + F1SO
#define OFFSET_P_INSHADOW				OFFSET_P_LIGHT_DISTANCE + F1SO

#define OFFSET_P_RADIANCE				OFFSET_P_INSHADOW + F1SO
#define OFFSET_P_THROUGHPUT				OFFSET_P_RADIANCE + F4SO
#define OFFSET_P_LIGHT_THROUGHPUT		OFFSET_P_THROUGHPUT + F4SO
#define OFFSET_P_DIRECT_LIGHT			OFFSET_P_LIGHT_THROUGHPUT + F4SO
#define OFFSET_P_PATH_LENGTH			OFFSET_P_DIRECT_LIGHT + F4SO
#define OFFSET_P_SCREEN_COORD			OFFSET_P_PATH_LENGTH + F1SO

///////////////////////////////////////////////////
// queue offsets
///////////////////////////////////////////////////
#define OFFSET_Q_NEWPATH				0
#define OFFSET_Q_MAT_UE4				OFFSET_Q_NEWPATH + F1SO
#define OFFSET_Q_MAT_GLASS				OFFSET_Q_MAT_UE4 + F1SO
#define OFFSET_Q_EXT_RAY				OFFSET_Q_MAT_GLASS + F1SO
#define OFFSET_Q_SHADOW_RAY				OFFSET_Q_EXT_RAY + F1SO

///////////////////////////////////////////////////
// queue counters offsets
///////////////////////////////////////////////////
#define OFFSET_QC_NEWPATH				0
#define OFFSET_QC_LASTPATHCNT			4
#define OFFSET_QC_MATUE4				8
#define OFFSET_QC_MATGLASS				12
#define OFFSET_QC_EXTRAY_UE4_OFFSET		16
#define OFFSET_QC_EXTRAY_GLASS_OFFSET	20
#define OFFSET_QC_SHADOWRAY				24

///////////////////////////////////////////////////
// define getters
///////////////////////////////////////////////////
#define GET(what, index, bytes)			(OFFSET_##what + 4 * bytes * (index))

// LOADS ALWAYS LOAD FROM POSITION GIVEN BY "index" VARIABLE
#define _pstate_rayOrigin				asfloat(pathState.Load3(GET(P_RAY_ORIGIN, index, 4)))
#define _pstate_rayDirection			asfloat(pathState.Load3(GET(P_RAY_DIRECTION, index, 4)))
#define _pstate_matColor				asfloat(pathState.Load3(GET(P_MAT_COLOR, index, 4)))
#define _pstate_matMetallicRoughness	asfloat(pathState.Load2(GET(P_MAT_METALICROUGHNESS, index, 2)))
#define _pstate_normal					asfloat(pathState.Load3(GET(P_NORMAL, index, 4)))
#define _pstate_surfacePoint			asfloat(pathState.Load3(GET(P_SURFACEPOINT, index, 4)))
#define _pstate_baryCoord				asfloat(pathState.Load3(GET(P_BARYCOORD, index, 4)))
#define _pstate_hitDistance				asfloat(pathState.Load(GET(P_HITDISTANCE, index, 1)))
#define _pstate_triangle				pathState.Load4(GET(P_TRIANGLE, index, 4))
#define _pstate_shadowrayOrigin			asfloat(pathState.Load3(GET(P_SHADOWRAY_ORIGIN, index, 4)))
#define _pstate_shadowrayDirection		asfloat(pathState.Load3(GET(P_SHADOWRAY_DIRECTION, index, 4)))
#define _pstate_lightIndex				pathState.Load(GET(P_LIGHT_INDEX, index, 1))
#define _pstate_lightDistance			asfloat(pathState.Load(GET(P_LIGHT_DISTANCE, index, 1)))
#define _pstate_inShadow				pathState.Load(GET(P_INSHADOW, index, 1))
#define _pstate_radiance				asfloat(pathState.Load3(GET(P_RADIANCE, index, 4)))
#define _pstate_throughput				asfloat(pathState.Load3(GET(P_THROUGHPUT, index, 4)))
#define _pstate_lightThroughput			asfloat(pathState.Load3(GET(P_LIGHT_THROUGHPUT, index, 4)))
#define _pstate_directlight				asfloat(pathState.Load3(GET(P_DIRECT_LIGHT, index, 4)))
#define _pstate_pathLength				pathState.Load(GET(P_PATH_LENGTH, index, 1))
#define _pstate_screenCoord				pathState.Load2(GET(P_SCREEN_COORD, index, 2))

// LOADS ALWAYS LOAD FROM POSITION GIVEN BY "queueIndex" VARIABLE
#define _queue_newPath					queue.Load(GET(Q_NEWPATH, queueIndex, 1))
#define _queue_matUE4					queue.Load(GET(Q_MAT_UE4, queueIndex, 1))
#define _queue_matGlass					queue.Load(GET(Q_MAT_GLASS, queueIndex, 1))
#define _queue_extRay					queue.Load(GET(Q_EXT_RAY, queueIndex, 1))
#define _queue_shadowRay				queue.Load(GET(Q_SHADOW_RAY, queueIndex, 1))

///////////////////////////////////////////////////
// define setters
///////////////////////////////////////////////////
// STORES ALWAYS STORE TO LOCATION POINTED BY "index" VARIABLE
#define _set_pstate_rayOrigin(val)				(pathState.Store3(GET(P_RAY_ORIGIN, index, 4), asuint(val)))
#define _set_pstate_rayDirection(val)			(pathState.Store3(GET(P_RAY_DIRECTION, index, 4), asuint(val)))
#define _set_pstate_matColor(val)				(pathState.Store3(GET(P_MAT_COLOR, index, 4), asuint(val)))
#define _set_pstate_matMetallicRoughness(val)	(pathState.Store2(GET(P_MAT_METALICROUGHNESS, index, 2), asuint(val)))
#define _set_pstate_normal(val)					(pathState.Store3(GET(P_NORMAL, index, 4), asuint(val)))
#define _set_pstate_surfacePoint(val)			(pathState.Store3(GET(P_SURFACEPOINT, index, 4), asuint(val)))
#define _set_pstate_baryCoord(val)				(pathState.Store3(GET(P_BARYCOORD, index, 4), asuint(val)))
#define _set_pstate_hitDistance(val)			(pathState.Store(GET(P_HITDISTANCE, index, 1), asuint(val)))
#define _set_pstate_triangle(val)				(pathState.Store4(GET(P_TRIANGLE, index, 4), val))
#define _set_pstate_shadowrayOrigin(val)		(pathState.Store3(GET(P_SHADOWRAY_ORIGIN, index, 4), asuint(val)))
#define _set_pstate_shadowrayDirection(val)		(pathState.Store3(GET(P_SHADOWRAY_DIRECTION, index, 4), asuint(val)))
#define _set_pstate_lightIndex(val)				(pathState.Store(GET(P_LIGHT_INDEX, index, 1), val))
#define _set_pstate_lightDistance(val)			(pathState.Store(GET(P_LIGHT_DISTANCE, index, 1), asuint(val)))
#define _set_pstate_inShadow(val)				(pathState.Store(GET(P_INSHADOW, index, 1), val))
#define _set_pstate_radiance(val)				(pathState.Store3(GET(P_RADIANCE, index, 4), asuint(val)))
#define _set_pstate_throughput(val)				(pathState.Store3(GET(P_THROUGHPUT, index, 4), asuint(val)))
#define _set_pstate_lightThroughput(val)		(pathState.Store3(GET(P_LIGHT_THROUGHPUT, index, 4), asuint(val)))
#define _set_pstate_directlight(val)			(pathState.Store3(GET(P_DIRECT_LIGHT, index, 4), asuint(val)))
#define _set_pstate_pathLength(val)				(pathState.Store(GET(P_PATH_LENGTH, index, 1), val))
#define _set_pstate_screenCoord(val)			(pathState.Store2(GET(P_SCREEN_COORD, index, 2), val))

#define _set_queue_newPath(index, val)			(queue.Store(GET(Q_NEWPATH, index, 1), val))
#define _set_queue_matUE4(index, val)			(queue.Store(GET(Q_MAT_UE4, index, 1), val))
#define _set_queue_matGlass(index, val)			(queue.Store(GET(Q_MAT_GLASS, index, 1), val))
#define _set_queue_extRay(index, val)			(queue.Store(GET(Q_EXT_RAY, index, 1), val))
#define _set_queue_shadowRay(index, val)		(queue.Store(GET(Q_SHADOW_RAY, index, 1), val))

#define NV_SHADER_EXTN_SLOT u5

#include "nvHLSLExtns.h"

///////////////////////////////////////////////////

struct Ray
{
	float3 origin;
	float3 direction;

	static Ray create(float3 o, float3 d)
	{
		Ray r; r.origin = o; r.direction = d;
		return r;
	}
};

struct Light
{
    float3 position;
    float3 emission;
    float radius;
};

struct Camera
{
    float3 pos;
	float pad0;
	float3 ulc;
	float pad1;
    float3 horizontal;
	float pad2;
	float3 vertical;
	float pad3;
	float2 pixelSize;
    float2 randomSeed;
    uint sampleCounter;
	uint lightCount;
	uint sampleLights;
};

struct BVHNode
{
	float3 min;
	float pad0;

	float3 max;
	float pad1;
	
	int leftIndex;
	int rightIndex;
	bool isLeaf;
	float pad2;
};

struct Triangle
{
	uint3 vtix;
	uint materialID;
};

struct TriangleParameters
{
	float3 normal;
	float pad0;

    float2 texCoord;
    uint materialID;
    float pad1;
};

struct Material
{
    float3 baseColor;
    float metallic;
    float roughness;
};

struct MaterialProperty
{
	float4 baseColor;

    float metallic;
    float roughness;
    float refractIndex;
    float transmittance; // only as pad for now

	int diffuseIndex;
    int metallicRoughnesIndex;
    int normalIndex;

    uint materialType;
};

//struct State
//{
//    Ray ray;
//    float3 hitPoint;
//    float3 baryCoord;
//    Triangle tri;
//    
//	float2 texCoord;
//    float3 normal;
//    MaterialProperty material;
//};

struct Sample
{
    float3 bsdfDir;
    float pdf;
};

//struct PathState // TODO prob align all flt3
//{
//	Ray ray[PATHCOUNT];
//	Material material[PATHCOUNT];
//	float3 normal[PATHCOUNT];
//	float3 surfacePoint[PATHCOUNT]; 
//	float3 baryCoord[PATHCOUNT];
//	float hitDistance[PATHCOUNT];
//	Triangle tri[PATHCOUNT]; // 26F
//
//	Ray shadowRay[PATHCOUNT];
//	int lightIndex[PATHCOUNT];
//	float lightDistance[PATHCOUNT];
//	int inShadow[PATHCOUNT]; // default set to TRUE in controll //9
//
//	float3 radiance[PATHCOUNT];
//	float3 throughtput[PATHCOUNT];
//	float3 lightThroughput[PATHCOUNT];
//	float3 directLight[PATHCOUNT];
//	uint pathLength[PATHCOUNT];
//	uint2 screenCoord[PATHCOUNT]; //15
//}; // 26 + 9 + 15 = 50F / 200B

//struct Queue
//{
//	int newPath[PATHCOUNT];
//	int materialUE4[PATHCOUNT];
//	int materialGlass[PATHCOUNT];
//	int extensionRay[PATHCOUNT];
//	int shadowRay[PATHCOUNT];
//	
//	uint queueCountNewPath;
//	uint queueCountUE4;
//	uint queueCountGlass;
//	uint queueCountExtensionRay;
//	uint queueCountShadowRay;
//};
//
struct PathState // TODO prob align all flt3
{
	Ray ray;
	Material material;
	float4 normal;
	float4 surfacePoint; 
	float4 baryCoord;
	float hitDistance;
	Triangle tri; // 31F

	Ray shadowRay;
	int lightIndex;
	float lightDistance;
	int inShadow; // default set to TRUE in controll //11F

	float4 radiance;
	float4 throughtput;
	float4 lightThroughput;
	float4 directLight;
	uint pathLength;
	uint2 screenCoord; // 19F
}; // 31 + 11 + 19 = 61F / 244B //488 MB

struct Queue
{
	uint newPath;
	uint materialUE4;
	uint materialGlass;
	uint extensionRay;
	uint shadowRay;
	// uint queueCountNewPath; // at the end of struct
	// uint queueCountUE4;
	// uint queueCountGlass;
	// uint queueCountExtensionRay;
	// uint queueCountShadowRay;
};

inline void broadcast(inout uint val)
{
    val |= NvShflXor(val, 16);
    val |= NvShflXor(val, 8);
    val |= NvShflXor(val, 4);
    val |= NvShflXor(val, 2);
    val |= NvShflXor(val, 1);
}
