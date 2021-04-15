#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
SamplerState smp : register(s0);

float4 BasicPs(Output input) : SV_TARGET
{
	const float3 light = normalize(float3(1, -1, 1));
	const float brightness = dot(-light, input.normal.xyz);
	const float2 sphereMapUv = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);
	const float4 color = tex.Sample(smp, input.uv);

	return float4(brightness, brightness, brightness, 1)
		* diffuse
		* color
		* sph.Sample(smp, sphereMapUv)
		+ spa.Sample(smp, sphereMapUv)
		+ color * float4(ambient, 1);
}