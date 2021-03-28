#include "BasicShaderHeader.hlsli"

cbuffer cbuff0 : register(b0)
{
	matrix mat;
}

Output BasicVs(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 noneno : BONE_NO,
	min16uint weight : WEIGHT)
{
	Output output;
	{
		output.svpos = mul(mat, pos);
		output.normal = normal;
		output.uv = uv;
	}

	return output;
}