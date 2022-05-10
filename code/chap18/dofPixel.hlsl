#include "dofHeader.hlsli"
#include "util.hlsli"

#define DEBUG (0)

Texture2D<float4> texColor : register(t0);
Texture2D<float4> texShrinkColor : register(t1);
Texture2D<float> texDepth : register(t2);
SamplerState smp : register(s0);

static const float2 kDepthSample = float2(0.5f, 0.5f);
static const float kAdjust = 0.5;
static const uint kNumLevel = 8;

static void computeShrinkLevel(out uint level, out float interpolate, float2 uv);

float4 main(VsOut psIn) : SV_TARGET
{
    int no = 0;
    float fp = 0.0f;

    computeShrinkLevel(no, fp, psIn.uv);

    if (no >= (int)kNumLevel)
        return float4(0, 0, 0, 0);

    float w = 0.0f, h = 0.0f;
    texShrinkColor.GetDimensions(w, h);
    const float dx = 1.0f / w;
    const float dy = 1.0f / h;

    float2 uvSize = float2(1.0f, 0.5f);
    float2 uvOfsset = float2(0.0f, 0.0f);

    float4 retColor[2];
    retColor[0] = texColor.Sample(smp, psIn.uv);

#if DEBUG
    {
        const float kUnit = 1.0f / kNumLevel;
        const float r = no * kUnit;
        return float4(r, r, r, r);
    }
#endif // DEBUG

    if (no == 0)
    {
        retColor[1] = Get5x5GaussianBlur(
            texShrinkColor,
            smp,
            psIn.uv * uvSize + uvOfsset,
            dx,
            dy);
    }
    else
    {
        [unroll] for (int i = 1; i <= 8; ++i)
        {
            if (i - no < 0)
            {
                uvOfsset.y += uvSize.y;
                uvSize *= 0.5f;
                continue;
            }

            retColor[i - no] = Get5x5GaussianBlur(
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

    return lerp(retColor[0], retColor[1], fp);
}

float4 copyTex(VsOut psIn) : SV_TARGET
{
    return texColor.Sample(smp, psIn.uv);
}

static void computeShrinkLevel(out uint level, out float interpolate, float2 uv)
{
    const float depthDiff = abs(texDepth.Sample(smp, kDepthSample) - texDepth.Sample(smp, uv));
    const float depthDiff2 = pow(depthDiff, kAdjust);
    const float t = depthDiff2 * kNumLevel;
    interpolate = modf(t, level);
}
