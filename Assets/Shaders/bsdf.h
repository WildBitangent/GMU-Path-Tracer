inline float3 schlickFresnel(float3 r0, float theta)
{
    float m = saturate(1.0 - theta);
    float m2 = m * m;
    return r0 - (1 - r0) * m2 * m2 * m;
}

inline float GGXTrowbridgeReitz(float XdotY, float alpha)
{
    float a2 = alpha * alpha;
    float x = XdotY * XdotY * (a2 - 1) + 1.0;
    return a2 / (PI * x * x);
}

inline float smithSchlickGGX(float XdotY, float alpha)
{
    float a1 = alpha + 1;
    float k = (a1 * a1) / 8;
    return XdotY / (XdotY * (1 - k) + k);
}

inline float lightFalloff(float distance, float radius)
{
    float n = saturate(1 - pow(distance / radius, 4));
    return (n * n) / (distance * distance + 1);
}