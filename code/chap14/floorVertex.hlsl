#include "floorHeader.hlsli"
#include "commonParam.hlsli"

cbuffer Transform : register(b1)
{
	matrix meshWorld;
	matrix axisWorld;
}

Output basicVs(float4 pos : POSITION)
{
	Output output;
	output.svpos = mul(mul(mul(proj, view), meshWorld), pos);
	output.tpos = mul(mul(lightCamera, meshWorld), pos);

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