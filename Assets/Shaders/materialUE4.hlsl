#include "structs.h"
#include "bsdf.h"
#include "random.h" 

////////////////////////////////////////////

struct State
{
    Ray ray;
    Material material;
    float3 normal;
};

////////////////////////////////////////////

RWStructuredBuffer<PathState> pathState : register(u1); 
RWStructuredBuffer<Queue> queue : register(u2);
globallycoherent RWByteAddressBuffer queueCounters : register(u3);
StructuredBuffer<Light> lights : register(t3);

////////////////////////////////////////////


float3 ue4Sample(in State state)
{
    float3 N = state.normal;
    float3 V = -state.ray.direction;

    float2 r = float2(rand(), rand());
	
    float diffuseRatio =/* 0.5 **/ (1.0 - state.material.metallic);
    float3 direction;
	
	// choose another up vector, if they are lined
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

	// importance sample diffuse vs specular direction
    if (rand().x < diffuseRatio) // TODO this is quite divergent
    {
        float x = sqrt(r.x);
        float phi = 2.0 * PI * r.y;
        direction.x = x * cos(phi);
        direction.y = x * sin(phi);
        direction.z = sqrt(max(0.0, 1.0 - direction.x * direction.x - direction.y * direction.y));
		
        direction = tangent * direction.x + bitangent * direction.y + N * direction.z;
    }
    else
    {
        float a = state.material.roughness * state.material.roughness;
        float phi = 2 * PI * r.x;

        float cosTheta = sqrt((1 - r.y) / (1 + (a * a - 1) * r.y));
        float sinTheta = sqrt(1 - cosTheta * cosTheta);

        float3 H;
        H.x = sinTheta * cos(phi);
        H.y = sinTheta * sin(phi);
        H.z = cosTheta;

        direction = tangent * H.x + bitangent * H.y + N * H.z;
        direction = 2.0 * dot(V, direction) * direction - V;
    }

    return direction;
}

float ue4Pdf(in State state, float3 direction)
{
    float3 N = state.normal;
    float3 V = -state.ray.direction;
    float3 L = direction;

    float diffuseRatio = /*0.5 * */(1.0 - state.material.metallic);
    float specularRatio = 1 - diffuseRatio;
	
    float3 H = normalize(L + V);

    float NdotH = abs(dot(N, H));
    float pdfGGXTR = GGXTrowbridgeReitz(NdotH, state.material.roughness * state.material.roughness) * NdotH;

	// calculate diffuse and specular pdf
    float pdfSpec = pdfGGXTR / (4.0 * abs(dot(V, H)));
    float pdfDiff = abs(dot(L, N)) * (1.0 / PI);

	// mix pdfs according to their ratios
    return diffuseRatio * pdfDiff + specularRatio * pdfSpec;
}

float3 ue4Evaluate(in State state, float3 direction)
{
    float3 N = state.normal;
    float3 V = -state.ray.direction;
    float3 L = direction;

    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if (NdotL <= 0.0 || NdotV <= 0.0)
        return float3(0, 0, 0);

    float3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H); 
	
    float D = GGXTrowbridgeReitz(NdotH, state.material.roughness * state.material.roughness);
    float G = smithSchlickGGX(NdotL, state.material.roughness) * smithSchlickGGX(NdotV, state.material.roughness);
    float3 specColor = lerp(0.037 * float3(1, 1, 1), state.material.baseColor.rgb, state.material.metallic);
    float F0 = -schlickFresnel(0, LdotH);
    float3 F = lerp(specColor, float3(1, 1, 1), F0 * float3(1, 1, 1));
    
    return (state.material.baseColor.xyz / PI) * (1.0 - state.material.metallic) + (D * F * G) / (4 * NdotL * NdotV);
}


[numthreads(threadCountX, 1, 1)]
void main(uint3 gid : SV_GroupID, uint tid : SV_GroupIndex, uint3 giseed : SV_DispatchThreadID)
{
    uint stride = threadCountX * numGroups;
	uint queueElementCount = queueCounters.Load(OFFSET_MATUE4);
	uint extQueueOffset = queueCounters.Load(OFFSET_EXTRAY);
	
	seed = giseed.xy;

    for (int i = 0; i < 16; i++)
    {
        uint queueIndex = tid + 256 * gid.x + i * stride;
        if (queueIndex >= queueElementCount)
            break;
		
        int index = queue[queueIndex].materialUE4;

        State state;
        Sample sample;
        float throughput = float3(0, 0, 0);

		// fill the state
        state.ray = pathState[index].ray;
        state.normal = pathState[index].normal;
        state.material = pathState[index].material;
		
        sample.bsdfDir = ue4Sample(state);
        sample.pdf = ue4Pdf(state, sample.bsdfDir);
		
        if (sample.pdf > 0.0)
            throughput = ue4Evaluate(state, sample.bsdfDir) * abs(dot(state.normal, sample.bsdfDir)) / sample.pdf;
		
        pathState[index].lightThroughput = throughput;

		// create extended ray
        float3 surfacePoint = pathState[index].surfacePoint;
        state.ray = Ray::create(surfacePoint + sample.bsdfDir * EPSILON, sample.bsdfDir);

        pathState[index].ray = state.ray; 
		queue[extQueueOffset + queueIndex].extensionRay = index;

		// set directLight
        float3 normal = pathState[index].normal;
        int lightIndex = pathState[index].lightIndex;
        float3 lightDir = pathState[index].shadowRay.direction;
        float distance = pathState[index].lightDistance;
        Light light = lights[lightIndex];

        bool legitLight = dot(lightDir, normal) > 0.0;
		uint shadowBallot = NvBallot(legitLight);
        uint shadowRayCount = countbits(shadowBallot);
        uint shadowRayOffset = 0;

        if (NvGetLaneId() == 0)
            queueCounters.InterlockedAdd(OFFSET_SHADOWRAY, shadowRayCount, shadowRayOffset);

        broadcast(shadowRayOffset);
        uint shadowIndex = NvWaveMultiPrefixExclusiveAdd(1, shadowBallot);
		
        if (legitLight)
            pathState[index].directLight = ue4Evaluate(state, lightDir) * light.emission * lightFalloff(distance, light.radius);

		queue[shadowRayOffset + shadowIndex].shadowRay = index;
	}
	
	
	// update extension ray queue count
	if (tid + gid.x == 0)
		queueCounters.Store(OFFSET_EXTRAY, extQueueOffset + queueElementCount);
}