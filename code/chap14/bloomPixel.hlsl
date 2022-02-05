#include "bloomHeader.hlsli"

static const uint kShrinkLevel = 8;

Texture2D<float4> texColor : register(t0);
Texture2D<float4> texLumShrink : register(t1);
Texture2D<float4> texLum : register(t2);
SamplerState smp : register(s0);

static float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy);

float4 main(VsOut vsOut) : SV_TARGET
{
	float w = 0.0f, h = 0.0f;
	texLum.GetDimensions(w, h);

    const float dx = 1.0f / w;
    const float dy = 1.0f / h;

    float4 bloomAccum = float4(0.0f, 0.0f, 0.0f, 0.0f);
	{
        float2 uvSize = float2(1.0f, 0.5f);
        float2 uvOfst = float2(0.0f, 0.0f);

        for (uint i = 0; i < kShrinkLevel; ++i)
        {
            const float2 uv = vsOut.uv * uvSize + uvOfst;
            bloomAccum += Get5x5GaussianBlur(texLumShrink, smp, uv, dx, dy);

            uvOfst.y += uvSize.y;
            uvSize *= 0.5f;
        }
    }

    const float4 color = texColor.Sample(smp, vsOut.uv);
    const float4 blur = Get5x5GaussianBlur(texLum, smp, vsOut.uv, dx, dy);
    const float4 shrinkBlur = saturate(bloomAccum);

    return color + blur + shrinkBlur;
}

float4 texCopy(VsOut vsOut) : SV_TARGET
{
    return texLum.Sample(smp, vsOut.uv);
}

static float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy)
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, uv + float2(-2 * dx, 2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 2 * dy)) *  1 / 256;
	
	ret += tex.Sample(smp, uv + float2(-2 * dx, 1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, 0 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 0 * dy)) * 36 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 0 * dy)) *  6 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, -1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, -1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, -1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, -2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, -2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, -2 * dy)) *  1 / 256;

	return ret;
}

