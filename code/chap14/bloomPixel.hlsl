#include "bloomHeader.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

static float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy);

float4 main(VsOut vsOut) : SV_TARGET
{
#if 1
	return float4(tex.Sample(smp, vsOut.uv).xyz, 1.0f);
#else
	float w = 0.0f, h = 0.0f;
	tex.GetDimensions(w, h);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	return Get5x5GaussianBlur(tex, smp, vsOut.uv, dx, dy);
#endif
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

