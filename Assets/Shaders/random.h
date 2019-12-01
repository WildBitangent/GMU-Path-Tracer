cbuffer Cam : register(b0)
{
    Camera cam;
};

static float2 seed;

inline float rand()
{
    //seed -= float2(sin(cam.randomSeed), cos(cam.randomSeed));
    seed -= cam.randomSeed;
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}