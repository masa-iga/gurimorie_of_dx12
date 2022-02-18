#include "dofHeader.hlsli"
#include "util.hlsli"

Texture2D<float4> texColor : register(t0);
Texture2D<float4> texShrinkColor : register(t1);
Texture2D<float> texDepth : register(t2);
SamplerState smp : register(s0);

static const float2 kDepthSample = float2(0.5f, 0.5f);
static const float kAdjust = 0.5;
static const uint kNumLevel = 8;

float4 main(VsOut psIn) : SV_TARGET
{
    const float depthDiff = abs(texDepth.Sample(smp, kDepthSample) - texDepth.Sample(smp, psIn.uv));
    const float depthDiff2 = pow(depthDiff, kAdjust);
    const float t = depthDiff2 * kNumLevel;
    float no = 0.0f;
    const float fp = modf(t, no);

    float w = 0.0f, h = 0.0f;
    texShrinkColor.GetDimensions(w, h);
    const float dx = 1.0f / w;
    const float dy = 1.0f / h;

    float2 uvSize = float2(1.0f, 0.5f);
    float2 uvOfsset = float2(0.0f, 0.0f);

    float4 retColor0 = texColor.Sample(smp, psIn.uv);
    float4 retColor1 = float4(0, 0, 0, 0);

    if (no == 0.0f)
    {
        retColor1 = Get5x5GaussianBlur(
            texShrinkColor,
            smp,
            psIn.uv * uvSize + uvOfsset,
            dx,
            dy);
    }
    else
    {
        for (uint i = 1; i <= 8; ++i)
        {
            if (i - no < 0)
                continue;

            retColor1 = Get5x5GaussianBlur(
                texShrinkColor,
                smp,
                psIn.uv * uvSize + uvOfsset,
                dx,
                dy);

            uvOfsset.y += uvSize.y;
            uvSize *= 0.5f;

            if (i - no > 1)
                break;
        }
    }

    return lerp(retColor0, retColor1, fp);
}

float4 copyTex(VsOut psIn) : SV_TARGET
{
    return texColor.Sample(smp, psIn.uv);
}