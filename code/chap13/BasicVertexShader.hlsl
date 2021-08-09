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
		pos = mul(bm, pos);
#if 1
		float4 wpos = mul(world, pos);
		wpos = mul(shadow, wpos);
#else
		const float4 wpos = mul(world, pos);
#endif
		output.svpos = mul(mul(proj, view), wpos);
		output.pos = mul(view, wpos);

		normal.w = 0;
		output.normal = mul(world, normal);
		output.vnormal = mul(view, output.normal);

		output.uv = uv;
		output.ray = normalize(pos.xyz - eye);
	}

	return output;
}