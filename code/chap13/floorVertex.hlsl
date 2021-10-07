#include "floorHeader.hlsli"

cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
}

cbuffer Transform : register(b1)
{
	matrix world;
}

Output basicVs(float4 pos : POSITION)
{
	Output output;
	output.svpos = mul(mul(mul(proj, view), world), pos);
	output.tpos = mul(lightCamera, pos);

	return output;
}

float4 shadowVs(float4 pos : POSITION) : SV_POSITION
{
	return mul(mul(lightCamera, world), pos);
}