struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
	float4 tpos : TPOS;
	uint instNo : SV_InstanceID;
};

cbuffer SceneParam : register(b0)
{
	matrix view;
	matrix proj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
    float highLuminanceThreshold;
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
