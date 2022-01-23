#include "BasicShaderHeader.hlsli"

struct PixelOutput
{
	float4 col : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 highLum : SV_TARGET2;
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);
Texture2D<float> lightDepthTex : register(t4);
SamplerState smp : register(s0);
SamplerState smpToon : register(s1);
SamplerComparisonState smpShadow : register(s2);

static const float kBias = 0.005f;

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

float4 BasicWithShadowInstancePs(Output input) : SV_TARGET
{
	if (input.instNo == 1)
	{
		// shadow
		return float4(0, 0, 0, 1);
	}

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

float4 BasicWithShadowMapPs(Output input) : SV_TARGET
{
	const float3 light = normalize(float3(1, -1, 1));
	const float3 lightColor = float3(1, 1, 1);

	const float diffuseB = saturate(dot(-light, input.normal.xyz));
	const float4 toonDif = toon.Sample(smpToon, float2(0, 1.0f - diffuseB));

	const float3 refLight = normalize(reflect(light, input.normal.xyz));
	const float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	const float2 sphereMapUv = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	const float4 texColor = tex.Sample(smp, input.uv);

	const float3 posFromLightVP = input.tpos.xyz / input.tpos.w;

	const float2 shadowUV = (posFromLightVP.xy + float2(1, -1)) * float2(0.5, -0.5);
	const float depthFromLight = lightDepthTex.SampleCmp(smpShadow, shadowUV, posFromLightVP.z - kBias);
	const float shadowWeight = lerp(0.5f, 1.0f, depthFromLight); // be 0.5f if the value is 0.0f

	const float4 ret = max(
		toonDif // brightness (toon)
		* diffuse // diffuse
		* texColor // texture color
		* sph.Sample(smp, sphereMapUv) // sphere map (multiply)
		+ saturate(spa.Sample(smp, sphereMapUv) * texColor // sphere map (add)
			+ float4(specularB * specular.rgb, 1)) // (specular)
		, float4(texColor.rgb * ambient, 1)); // (ambient)

	return float4(ret.rgb * shadowWeight, ret.a);
}

PixelOutput MrtWithShadowMapPs(Output input)
{
	const float3 light = normalize(float3(1, -1, 1));
	const float3 lightColor = float3(1, 1, 1);

	const float diffuseB = saturate(dot(-light, input.normal.xyz));
	const float4 toonDif = toon.Sample(smpToon, float2(0, 1.0f - diffuseB));

	const float3 refLight = normalize(reflect(light, input.normal.xyz));
	const float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	const float2 sphereMapUv = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	const float4 texColor = tex.Sample(smp, input.uv);

	const float3 posFromLightVP = input.tpos.xyz / input.tpos.w;

	const float2 shadowUV = (posFromLightVP.xy + float2(1, -1)) * float2(0.5, -0.5);
	const float depthFromLight = lightDepthTex.SampleCmp(smpShadow, shadowUV, posFromLightVP.z - kBias);
	const float shadowWeight = lerp(0.5f, 1.0f, depthFromLight); // be 0.5f if the value is 0.0f

	const float4 ret = max(
		toonDif // brightness (toon)
		* diffuse // diffuse
		* texColor // texture color
		* sph.Sample(smp, sphereMapUv) // sphere map (multiply)
		+ saturate(spa.Sample(smp, sphereMapUv) * texColor // sphere map (add)
			+ float4(specularB * specular.rgb, 1)) // (specular)
		, float4(texColor.rgb * ambient, 1)); // (ambient)

	PixelOutput output;
	{
		output.col = float4(ret.rgb * shadowWeight, ret.a);
		output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
		output.normal.a = 1;

		const float y = dot(float3(0.299f, 0.587f, 0.114f), output.col.xyz);
		output.highLum = (y > 0.99f) ? output.col : 0.0f;
	}

	return output;
}