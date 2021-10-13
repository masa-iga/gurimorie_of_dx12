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
	matrix meshWorld;
	matrix axisWorld;
}

Output basicVs(float4 pos : POSITION)
{
	Output output;
	output.svpos = mul(mul(mul(proj, view), meshWorld), pos);
	output.tpos = mul(lightCamera, pos);

	return output;
}

float4 shadowVs(float4 pos : POSITION) : SV_POSITION
{
	return mul(mul(lightCamera, meshWorld), pos);
}

float4 axisVs(float4 pos : POSITION) : SV_POSITION
{
	return mul(mul(mul(proj, view), axisWorld), pos);
}