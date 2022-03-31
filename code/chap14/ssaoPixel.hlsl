#include "ssaoHeader.hlsli"
#include "commonParam.hlsli"
#include "util.hlsli"

Texture2D<float> texDepth : register(t0);
Texture2D<float4> texNormal : register(t1);
SamplerState smp : register(s0);

static float2 getDxDy(Texture2D<float> tex);

float4 main(VsOut vsOut) : SV_TARGET
{
    const float dp = texDepth.Sample(smp, vsOut.uv);

    float4 respos = mul(
        invProj,
        float4(vsOut.uv * float2(2, -2) + float2(-1, 1), dp, 1));
    respos.xyz /= respos.w;

    const float dx = getDxDy(texDepth).x;
    const float dy = getDxDy(texDepth).y;
    const float3 norm = normalize((texNormal.Sample(smp, vsOut.uv).xyz * 2) - 1);
    const int trycnt = 256;
    const float radius = 0.5f;
    float div = 0.0f;
    float ao = 0.0f;

    if (dp < 1.0f)
    {
        [unroll] for (int i = 0; i < trycnt; ++i)
        {
            const float rnd1 = random(float2(i * dx, i * dy)) * 2 - 1;
            const float rnd2 = random(float2(rnd1, i * dy)) * 2 - 1;
            const float rnd3 = random(float2(rnd2, rnd1)) * 2 - 1;

            float3 omega = normalize(float3(rnd1, rnd2, rnd3));
            omega = normalize(omega);

            float dt = dot(norm, omega);
            float sgn = sign(dt);
            omega *= sign(dt);

            float4 rpos = mul(proj, float4(respos.xyz + omega * radius, 1));
            rpos.xyz /= rpos.w;

            dt *= sgn;
            div += dt;

            ao += step(texDepth.Sample(smp, (rpos.xy + float2(1, -1)) * float2(0.5f, -0.5f)), rpos.z) * dt;
        }

        ao /= div;
    }
    
    return 1.0f - ao;
}

static float2 getDxDy(Texture2D<float> tex)
{
    float w = 0;
    float h = 0;
    texDepth.GetDimensions(w, h);

    return float2(1.0f / w, 1.0f / h);
}
