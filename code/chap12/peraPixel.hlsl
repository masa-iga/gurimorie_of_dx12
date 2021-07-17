#include "peraHeader.hlsli"

#define NORMAL (1)
#define GREYSCALE (0)
#define INVERSION (0)
#define DOWNGRADE (0)

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

	return float4(1.0, 1.0, 1.0, 1.0);
}