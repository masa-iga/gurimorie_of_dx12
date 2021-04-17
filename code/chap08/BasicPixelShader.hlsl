#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
SamplerState smp : register(s0);

float4 BasicPs(Output input) : SV_TARGET
{
	const float3 light = normalize(float3(1, -1, 1));
	const float3 lightColor = float3(1, 1, 1);

	const float diffuseB = dot(-light, input.normal.xyz);

	const float3 refLight = normalize(reflect(light, input.normal.xyz));
	const float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	const float2 sphereMapUv = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	const float4 texColor = tex.Sample(smp, input.uv);

	return max(
		diffuseB * diffuse * texColor * sph.Sample(smp, sphereMapUv)
		+ spa.Sample(smp, sphereMapUv) * texColor
		+ float4(specularB * specular.rgb, 1)
		, float4(texColor.rgb * ambient, 1));
}