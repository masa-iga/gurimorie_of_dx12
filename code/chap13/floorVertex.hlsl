cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
}

float4 basicVs(float4 pos : POSITION) : SV_POSITION
{
	return mul(mul(proj, view), pos);
}

float4 shadowVs(float4 pos : POSITION) : SV_POSITION
{
	return mul(lightCamera, pos);
}