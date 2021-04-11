#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
SamplerState smp : register(s0);

float4 BasicPs(Output input) : SV_TARGET
{
	const float3 light = normalize(float3(1, -1, 1));
	const float brightness = dot(-light, input.normal.xyz);

	return float4(brightness, brightness, brightness, 1)
		* diffuse
		* tex.Sample(smp, input.uv)
		* sph.Sample(smp, input.uv);
}