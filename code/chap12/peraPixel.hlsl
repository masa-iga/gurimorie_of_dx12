#include "peraHeader.hlsli"

#define NORMAL (0)
#define GREYSCALE (0)
#define INVERSION (0)
#define DOWNGRADE (0)
#define FILTER (1)

float4 main(Output input) : SV_TARGET
{
	const float4 col = tex.Sample(smp, input.uv);

#if NORMAL
	return col;
#endif // NORMAL

#if GREYSCALE
	const float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
	return float4(Y, Y, Y, 1.0);
#endif // GREYSCALE

#if INVERSION
	return float4(float3(1.0f, 1.0f, 1.0f) - col.rgb, col.a);
#endif // INVERSION

#if DOWNGRADE
	return float4(col.rgb - fmod(col.rgb, 0.25f), col.a);
#endif // DOWNGRADE

#if FILTER
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);

	const float dx = 1.0f / w;
	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, -2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, -2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 0.0f));
	ret += tex.Sample(smp, input.uv);
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 0.0f));
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 2.0f * dy));

	return ret / 9.0f;
#endif // FILTER

	return float4(1.0, 1.0, 1.0, 1.0);
}