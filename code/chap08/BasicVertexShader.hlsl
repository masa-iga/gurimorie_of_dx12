#include "BasicShaderHeader.hlsli"

cbuffer cbuff0 : register(b0)
{
	matrix world;
	matrix viewProj;
}

Output BasicVs(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONE_NO,
	min16uint weight : WEIGHT)
{
	Output output;
	{
		output.svpos = mul(mul(viewProj, world), pos);
		normal.w = 0;
		output.normal = mul(world, normal);
		output.uv = uv;
	}

	return output;
}