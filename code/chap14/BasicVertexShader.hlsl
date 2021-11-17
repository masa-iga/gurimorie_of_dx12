#include "BasicShaderHeader.hlsli"

Output BasicVs(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONENO,
	min16uint weight : WEIGHT)
{
	Output output;
	{
		const float w = weight / 100.0f;
		const matrix bm = boneMat[boneno[0]] * w + boneMat[boneno[1]] * (1 - w);
		const float4 wpos = mul(mul(world, bm), pos);

		output.svpos = mul(mul(proj, view), wpos);
		output.pos = mul(view, wpos);

		normal.w = 0;
		output.normal = mul(world, normal);
		output.vnormal = mul(view, output.normal);

		output.uv = uv;
		output.ray = normalize(pos.xyz - eye);
		output.tpos = mul(lightCamera, wpos);
		output.instNo = 0;
	}

	return output;
}

Output BasicWithShadowVs(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONENO,
	min16uint weight : WEIGHT,
	uint instNo : SV_InstanceID)
{
	Output output;
	{
		const float w = weight / 100.0f;
		const matrix bm = boneMat[boneno[0]] * w + boneMat[boneno[1]] * (1 - w);
		float4 wpos = mul(mul(world, bm), pos);

		if (instNo == 1)
		{
			// shadow transform
			wpos = mul(shadow, wpos);
		}

		output.svpos = mul(mul(proj, view), wpos);
		output.pos = mul(view, wpos);

		normal.w = 0;
		output.normal = mul(world, normal);
		output.vnormal = mul(view, output.normal);

		output.uv = uv;
		output.ray = normalize(pos.xyz - eye);
		output.tpos = mul(lightCamera, wpos);
		output.instNo = instNo;
	}

	return output;
}

float4 shadowVs(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONENO,
	min16uint weight : WEIGHT) : SV_POSITION
{
	const float fWeight = float(weight) / 100.0f;
	const matrix conBone = boneMat[boneno.x] * fWeight + boneMat[boneno.y] * (1.0f - fWeight);
	const float4 wpos = mul(mul(world, conBone), pos);

	return mul(lightCamera, wpos);
}