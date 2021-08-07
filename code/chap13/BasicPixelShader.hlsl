#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);
SamplerState smp : register(s0);
SamplerState smpToon : register(s1);

float4 BasicPs(Output input) : SV_TARGET
{
	const float3 light = normalize(float3(1, -1, 1));
	const float3 lightColor = float3(1, 1, 1);

	const float diffuseB = saturate(dot(-light, input.normal.xyz));
	const float4 toonDif = toon.Sample(smpToon, float2(0, 1.0f - diffuseB));

	const float3 refLight = normalize(reflect(light, input.normal.xyz));
	const float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	const float2 sphereMapUv = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	const float4 texColor = tex.Sample(smp, input.uv);

	return max(
		toonDif // brightness (toon)
		* diffuse // diffuse
		* texColor // texture color
		* sph.Sample(smp, sphereMapUv) // sphere map (multiply)
		+ saturate(spa.Sample(smp, sphereMapUv) * texColor // sphere map (add)
			+ float4(specularB * specular.rgb, 1)) // (specular)
		, float4(texColor.rgb * ambient, 1)); // (ambient)
}