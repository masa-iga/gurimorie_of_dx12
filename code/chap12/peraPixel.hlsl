#include "peraHeader.hlsli"

#define NO_EFFECT (0)
#define GREYSCALE (0)
#define INVERSION (0)
#define DOWNGRADE (0)
#define BLUR (0)
#define GAUSSIAN_BLUR (0)
#define GAUSSIAN_BLUR2 (1)
#define EMBOSS (0)
#define SHARPNESS (0)
#define EDGE (0)

float4 peraPs(Output input) : SV_TARGET
{
	const float4 col = tex.Sample(smp, input.uv);

#if NO_EFFECT
	return col;
#endif // NO_EFFECT

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

#if BLUR
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
#endif // BLUR

#if GAUSSIAN_BLUR
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);

	const float dx = 1.0f / w;
	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2( 0 * dx, 2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, input.uv + float2( 1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2( 2 * dx, 2 * dy)) *  1 / 256;
	
	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2( 0 * dx, 1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2( 1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2( 2 * dx, 1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0 * dy)) *  6 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2( 0 * dx, 0 * dy)) * 36 / 256;
	ret += tex.Sample(smp, input.uv + float2( 1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2( 2 * dx, 0 * dy)) *  6 / 256;

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2( 0 * dx, -1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, input.uv + float2( 1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, input.uv + float2( 2 * dx, -1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, input.uv + float2(-1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2( 0 * dx, -2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, input.uv + float2( 1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, input.uv + float2( 2 * dx, -2 * dy)) *  1 / 256;

	return ret;
#endif // GAUSSIAN_BLUR

#if GAUSSIAN_BLUR2
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);
	const float dx = 1.0f / w;

	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += bkweights[0][0] * col;

	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(i * dx, 0));
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(-i * dx, 0));
	}

	return float4(ret.rgb, col.a);
#endif // GAUSSIAN_BLUR2

#if EMBOSS
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);

	const float dx = 1.0f / w;
	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, -2.0f * dy)) * 2;
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2.0f * dy));
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, -2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 0.0f));
	ret += tex.Sample(smp, input.uv);
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 0.0f)) * -1;
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2.0f * dy)) * -1;
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 2.0f * dy)) * -2;

	return ret;
#endif // EMBOSS

#if SHARPNESS
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);

	const float dx = 1.0f / w;
	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, -2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2.0f * dy)) * -1;
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, -2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 0.0f)) * -1;
	ret += tex.Sample(smp, input.uv) * 5;
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 0.0f)) * -1;
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2.0f * dy)) * -1;
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 2.0f * dy)) * 0;

	return ret;
#endif // SHARPNESS

#if EDGE
	float w, h, levels;
	tex.GetDimensions(0 /* mip level*/, w, h, levels);

	const float dx = 1.0f / w;
	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	//ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, -2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(0.0f, -2.0f * dy)) * -1;
	//ret += tex.Sample(smp, input.uv + float2(2.0f * dx, -2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 0.0f)) * -1;
	ret += tex.Sample(smp, input.uv) * 4;
	ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 0.0f)) * -1;
	//ret += tex.Sample(smp, input.uv + float2(-2.0f * dx, 2.0f * dy)) * 0;
	ret += tex.Sample(smp, input.uv + float2(0.0f, 2.0f * dy)) * -1;
	//ret += tex.Sample(smp, input.uv + float2(2.0f * dx, 2.0f * dy)) * 0;

	// To grey scale
	float Y = dot(ret.rgb, float3(0.299, 0.587, 0.114));

	// Inversion
	Y = pow(1.0f - Y, 10.0f);
	Y = step(0.2f, Y);

	return float4(Y, Y, Y, col.a);
#endif // EDGE

	return float4(1.0, 1.0, 1.0, 1.0);
}

float4 verticalBokehPs(Output input) : SV_TARGET
{
	float w, h, level;
	tex.GetDimensions(0, w, h, level);

	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	const float4 col = tex.Sample(smp, input.uv);

	ret += bkweights[0] * col;

	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(0, dy * i));
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(0, -dy * i));
	}

	return col;
}