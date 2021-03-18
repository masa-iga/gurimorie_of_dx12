#include "BasicShaderHeader.hlsli"

cbuffer cbuff0 : register(b0)
{
	matrix mat;
}

Output BasicVs(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	Output output;
	output.svpos = mul(mat, pos);
	output.uv = uv;

	return output;
}