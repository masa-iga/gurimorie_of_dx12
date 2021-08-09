struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};

cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix shadow;
	float3 eye;
}

cbuffer Transform: register(b1)
{
	matrix world;
	matrix boneMat[256];
}

cbuffer Material : register(b2)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
}
