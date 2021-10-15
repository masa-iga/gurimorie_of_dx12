#include "floorHeader.hlsli"

SamplerState smp : register(s0);
Texture2D<float> lightDepthTex : register(t0);

static const float3 baseColor = float3(0.9f, 0.9f, 0.9f);

// TODO: imple lighting
float4 basicPs(Output input) : SV_TARGET
{
	return float4(baseColor, 1.0f);
}

// TODO: imple lighting
float4 basicWithShadowMapPs(Output input) : SV_TARGET
{
	const float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
	const float2 shadowUV = (posFromLightVP.xy + float2(1, -1)) * float2(0.5, -0.5);
	const float depthFromLight = lightDepthTex.Sample(smp, shadowUV);

	float shadowWeight = 1.0f;

	if (depthFromLight < posFromLightVP.z)
	{
		shadowWeight = 0.5f;
	}

	return float4(shadowWeight * baseColor, 1.0f);
}

float4 axisPs(float4 input : SV_POSITION) : SV_TARGET
{
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}