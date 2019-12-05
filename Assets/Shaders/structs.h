#define threadCountX 256
#define numGroups 512 // TODO is there better way?
#define FLT_MAX 3.402823466e+38
#define EPSILON 1e-4
#define STACKSIZE 16 // stack with depth of 16 is enough even for scene with 1.5M triangles TODO mb change in future / use of shared memory and subgroups
#define PI 3.1415926535897932384626433832795
#define INVPI 0.31830988618379067153776752674503
#define PATHCOUNT 2097152 // 2^21 2M paths TODO define at compilation time

//#define LIGHT float3(4, 8, 8)
#define LIGHT float3(2,23.5,0)
#define LIGHT2 float3(0, 4.5, 2)
#define LIGHTRADIUS 100.0
#define EMISSION float3(40, 40, 0) * 20
#define EMISSION2 float3(80, 80, 40)

// queue counters
#define OFFSET_NEWPATH 0
#define OFFSET_LASTPATHCNT 4
#define OFFSET_MATUE4 8
#define OFFSET_MATGLASS 12
//#define OFFSET_EXTRAY 16
#define OFFSET_EXTRAY_UE4_OFFSET 16
#define OFFSET_EXTRAY_GLASS_OFFSET 20
#define OFFSET_SHADOWRAY 24

#define NV_SHADER_EXTN_SLOT u5

#include "nvHLSLExtns.h"


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
	float3 ulc;
    float3 horizontal;
	float3 vertical;
	float2 pixelSize;
    uint sampleCounter;
    float2 randomSeed;
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
    float4 baseColor;
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
	float3 normal;
	float3 surfacePoint; 
	float3 baryCoord;
	float hitDistance;
	Triangle tri; // 26F

	Ray shadowRay;
	int lightIndex;
	float lightDistance;
	int inShadow; // default set to TRUE in controll //9

	float3 radiance;
	float3 throughtput;
	float3 lightThroughput;
	float3 directLight;
	uint pathLength;
	uint2 screenCoord; //15
}; // 26 + 9 + 15 = 50F / 200B

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

groupshared uint temp[8];

inline void broadcast(inout uint val)
{
    val |= NvShflXor(val, 16);
    val |= NvShflXor(val, 8);
    val |= NvShflXor(val, 4);
    val |= NvShflXor(val, 2);
    val |= NvShflXor(val, 1);
}
