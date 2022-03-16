#include "ssaoHeader.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(VsOut vsOut) : SV_TARGET
{
#if 0
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
#else
    return tex.Sample(smp, vsOut.uv);
#endif
}