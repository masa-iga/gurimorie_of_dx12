cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
}

float4 main(float4 pos : POSITION) : SV_POSITION
{
	return mul(mul(proj, view), pos);
}