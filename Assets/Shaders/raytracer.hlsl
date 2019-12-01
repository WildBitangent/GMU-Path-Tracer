#define threadCountX 32
#define threadCountY 8
#define threadCount threadCountX * threadCountY
#define FLT_MAX 3.402823466e+38
#define EPSILON 1e-4
#define STACKSIZE 16 // stack with depth of 16 is enough even for scene with 1.5M triangles TODO mb change in future / use of shared memory and subgroups
#define PI 3.14159265359

//#define LIGHT float3(4, 8, 8)
#define LIGHT float3(2,23.5,0)
#define LIGHT2 float3(0, 4.5, 2)
#define LIGHTRADIUS 100.0
#define EMISSION float3(40, 40, 0) * 20
#define EMISSION2 float3(80, 80, 40)

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

struct State
{
    Ray ray;
    float3 hitPoint;
    float3 baryCoord;
    Triangle tri;
    
	float2 texCoord;
    float3 normal;
    MaterialProperty material;
};

struct Sample
{
    float3 bsdfDir;
    float pdf;
};

cbuffer Cam : register(b0)
{
	Camera cam;
};

//cbuffer Model : register(b1)
//{
//    float4x4 mat;
//    uint numTriangles;
//};

cbuffer Material : register(b1)
{
	MaterialProperty materialProp[128]; // hopefully there won't be more than 128 materials (dx12 has unbound descriptors)
};

//////////////////////////////////

RWTexture2DArray<float4> output : register(u0);
StructuredBuffer<BVHNode> tree : register(t0);
StructuredBuffer<Triangle> indices : register(t1);
Buffer<float3> vertices : register(t2);
StructuredBuffer<TriangleParameters> triParams : register(t3);
Texture2DArray diffuse : register(t4);
Texture2DArray metallicRoughness : register(t5);
Texture2DArray normals : register(t6);
SamplerState samplerState : register(s0);

static float2 seed;
static State state;

//////////////////////////////////

inline float rand()
{
    //seed -= float2(sin(cam.randomSeed), cos(cam.randomSeed));
    seed -= cam.randomSeed;
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

inline Ray getPrimaryRay(float2 coord)
{
    float2 r = float2(rand(), rand()) * 2 - 1;
    float2 uv = (coord + r) * cam.pixelSize;

    return Ray::create(cam.pos, normalize(cam.ulc + uv.x * cam.horizontal - uv.y * cam.vertical));
}

bool shadowIntersector(in Ray ray, float3 v0, float3 v1, float3 v2, out float distance)
{
    float3 e1 = v1 - v0;
    float3 e2 = v2 - v0;
	
    float3 pvec = cross(ray.direction, e2);
    float det = dot(e1, pvec);

    if (det > -EPSILON && det < EPSILON)
        return false;
		
    float invDet = 1.0 / det;
    float3 tvec = ray.origin - v0;
    float u = dot(tvec, pvec) * invDet;
	
    if (u < 0.0f || u > 1.0f)
        return false;
		
    float3 qvec = cross(tvec, e1);
    float v = dot(ray.direction, qvec) * invDet;
	
    if (v < 0.0 || u + v > 1.0)
        return false;

    float t = dot(e2, qvec) * invDet;
    if (t > EPSILON && t < 1 / EPSILON)
    {
        distance = length(ray.direction * t);
        return true;
    }
    return false;
}

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
	
    if (t > EPSILON && t < 1 / EPSILON) // ray intersection
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
		for (int idx = 0; idx > -1; )
		{
			node = tree[idx];

			if (node.isLeaf)
			{
				for (uint i = node.leftIndex; i < node.rightIndex; i++)
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

bool isShadowed(in Ray ray, float lightDistance)
{
    int stack[STACKSIZE];
    uint ptr = 0;
    stack[ptr++] = -1;

	// early test
    BVHNode node = tree[0];
    if (rayAABBIntersection(node.min, node.max, ray) > 0.0)
    {
        for (int idx = 0; idx > -1;)
        {
            node = tree[idx];

            if (node.isLeaf)
            {
                for (uint i = node.leftIndex; i < node.rightIndex; i++)
                {
                    uint3 ii = indices[i].vtix;
                    float3 v0 = vertices[ii.x];
                    float3 v1 = vertices[ii.y];
                    float3 v2 = vertices[ii.z];

                    float distance;
                    if (shadowIntersector(ray, v0, v1, v2, distance) && distance < lightDistance)
                        //if (materialProp[indices[i].materialID].materialType == 0)
                            return true;
                }
            }
            else
            {
                BVHNode left = tree[node.leftIndex];
                BVHNode right = tree[node.rightIndex];

                float leftHit = rayAABBIntersection(left.min, left.max, ray);
                float rightHit = rayAABBIntersection(right.min, right.max, ray);

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

    return false;
}

void getHitInfos()
{
    uint3 indices = state.tri.vtix;

    float2 t0 = triParams[indices.x].texCoord;
    float2 t1 = triParams[indices.y].texCoord;
    float2 t2 = triParams[indices.z].texCoord;

    float3 n0 = triParams[indices.x].normal;
    float3 n1 = triParams[indices.y].normal;
    float3 n2 = triParams[indices.z].normal;

    state.texCoord = t0 * state.baryCoord.x + t1 * state.baryCoord.y + t2 * state.baryCoord.z;
    state.normal = n0 * state.baryCoord.x + n1 * state.baryCoord.y + n2 * state.baryCoord.z;
	
    state.material = materialProp[state.tri.materialID];
    //state.material.roughness = saturate(state.material.roughness + cam.sampleCounter * 0.0001);
	
    if (state.material.diffuseIndex >= 0)
        state.material.baseColor = diffuse.SampleLevel(samplerState, float3(state.texCoord, state.material.diffuseIndex), 0);

    if (state.material.metallicRoughnesIndex >= 0)
    {
        float2 data = metallicRoughness.SampleLevel(samplerState, float3(state.texCoord, state.material.metallicRoughnesIndex), 0);
        state.material.metallic = data.x;
        state.material.roughness = data.y;
    }

    if (state.material.normalIndex >= 0)
    {
        float3 data = normals.SampleLevel(samplerState, float3(state.texCoord, state.material.normalIndex), 0);
        data = data * 2.0 - 1.0; // TODO maybe normalize

		// flip the normal, if the ray is coming from behind
        float3 normal = dot(state.normal, state.ray.direction) <= 0.0 ? state.normal : state.normal * -1.0;

		// orthonormal basis
        float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, normal));
        float3 bitangent = cross(normal, tangent);
		
        state.normal = tangent * data.x + bitangent * data.y + normal * data.z; // TODO normalize?
    }
}

float3 schlickFresnel(float3 r0, float theta)
{
    float m = saturate(1.0 - theta);
    float m2 = m * m;
    return r0 - (1 - r0) * m2 * m2 * m;
}

float3 glassSample()
{
    float3 normal = dot(state.normal, state.ray.direction) <= 0.0 ? state.normal : state.normal * -1.0;

	// refraction
    float n1 = 1.0;
    float n2 = state.material.refractIndex;

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

float glassPdf()
{
    return 1.0;
}

float3 glassEvaluate()
{
    return state.material.baseColor;
}



float GGXTrowbridgeReitz(float XdotY, float alpha)
{
    float a2 = alpha * alpha;
    float x = XdotY * XdotY * (a2 - 1) + 1.0;
    return a2 / (PI * x * x);
}

float smithSchlickGGX(float XdotY, float alpha)
{
    float a1 = alpha + 1;
    float k = (a1 * a1) / 8;
    return XdotY / (XdotY * (1 - k) + k);
}

float3 ue4Sample()
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

float ue4Pdf(float3 direction)
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

float3 ue4Evaluate(float3 direction)
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

	
    //return float3(D, 0, 0);

    
    return (state.material.baseColor.xyz / PI) * (1.0 - state.material.metallic) + (D * F * G) / (4 * NdotL * NdotV);
}

float powerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

float3 sampleSphere(float2 u)
{
    float z = 1.0 - 2.0 * u.x;
    float r = sqrt(max(0.f, 1.0 - z * z));
    float phi = 2.0 * PI * u.y;
    float x = r * cos(phi);
    float y = r * sin(phi);

    return float3(x, y, z);
}

float3 sampleLight(out float3 normal, out float3 emission)
{
    float3 surfacePos = LIGHT + sampleSphere(float2(rand(), rand())) * LIGHTRADIUS;
    normal = surfacePos - LIGHT;
    //emission = light.emission * numOfLights;
    emission = EMISSION;
	
    return surfacePos;
}

inline float lightFalloff(float distance, float radius)
{
    float n = saturate(1 - pow(distance / radius, 4));
    return (n * n) / (distance * distance + 1);
}

float3 computeDirectLight()
{
    float3 radiance = float3(0, 0, 0);
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

    float3 surfacePos = state.hitPoint + state.normal * EPSILON;
    float3 lightDir = light.position - surfacePos;
    float distance = length(lightDir);

    lightDir = normalize(lightDir);
	
    if (dot(lightDir, state.normal) <= 0.0 /* || dot(lightDir, lightNormal) >= 0.0*/)
        return radiance;

    bool inShadow = isShadowed(Ray::create(surfacePos, lightDir), distance - EPSILON);
	
    if (!inShadow)
        radiance = ue4Evaluate(lightDir) * light.emission * lightFalloff(distance, light.radius); 

    return radiance;
}

[numthreads(threadCountX, threadCountY, 1)]
void main(uint3 gid : SV_DispatchThreadID, uint tid : SV_GroupIndex)
{
	state.ray = getPrimaryRay(gid.xy);

    seed = gid.xy;
	   
	float3 radiance = float3(0, 0, 0);
    float3 throughput = float3(1, 1, 1);
    Sample sample;

    for (uint i = 0; any(throughput > 0.0001); i++)
	{
		float t = rayBVHIntersection();

		if (t == FLT_MAX)
        {
            radiance += float3(1, 1, 1) * throughput * 0.3;
            //radiance = float3(1000000, 0, 0);
			break;
        }

		getHitInfos();
        //state.material.metallic = 1.0;
        //state.material.roughness = 0.0;
        state.material.roughness = max(0.014, state.material.roughness); // gotta clip roughness - floating point precission

		if (state.material.materialType == 0) // UE4-ish
        {
			// add direct light
            radiance += computeDirectLight() * throughput;

            sample.bsdfDir = ue4Sample();
            sample.pdf = ue4Pdf(sample.bsdfDir);

			if (sample.pdf > 0.0)
            {
                throughput *= (ue4Evaluate(sample.bsdfDir) * abs(dot(state.normal, sample.bsdfDir)) / sample.pdf); // TODO hmmm
                //if (any(ue4Evaluate(sample.bsdfDir) > 1.1))
                //{
                //    radiance = float3(10000000, 0, 0);
                //    break;
                //}

            }
			else
            {
                //radiance = float3(0, 1, 0);
                break;
            }

			//if (any(radiance > 2.1))
   //         {
   //             radiance = float3(1000000, 0, 0);
   //             break;
   //         }
        }
		else // GLASS
        {
            sample.bsdfDir = glassSample();
            //sample.pdf = glassPdf();
            throughput *= glassEvaluate();
        }

		// russian roulette
        if (i > 20) // todo start at ~100 ?
        {
            float p = max(throughput.x, max(throughput.y, throughput.z));
            if (rand() * 0.004 > p)
                break;

            throughput *= 1 / p;
        }

		// create new ray to trace
        state.ray = Ray::create(state.hitPoint + sample.bsdfDir * EPSILON, sample.bsdfDir);
    }

    radiance = saturate(radiance);

    if (cam.sampleCounter == 0)
    {
        output[gid.xyz] = float4(radiance, 1.0);
        output[uint3(gid.xy, 1)] = float4(0, 0, 0, 0);
    }
	else
    {
		// kahan summation
        float4 sum = output[gid] * cam.sampleCounter;
        float4 c = output[uint3(gid.xy, 1)];
		
        float4 y = float4(radiance, 1.0) - c;
        float4 t = sum + y;

        output[gid.xyz] = t / (cam.sampleCounter + 1);
        output[uint3(gid.xy, 1)] = (t - sum) - y;

        //output[gid.xyz] = (output[gid.xyz] * cam.sampleCounter + float4(radiance, 0)) / (cam.sampleCounter + 1);
    }
} 