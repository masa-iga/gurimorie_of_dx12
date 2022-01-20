#include "bloomHeader.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 main(VsOut vsOut) : SV_TARGET
{
	return float4(tex.Sample(smp, vsOut.uv).xyz, 1.0f);
}