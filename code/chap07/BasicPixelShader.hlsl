#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 BasicPs(Output input) : SV_TARGET
{
	return float4(0, 0, 0, 1);
}