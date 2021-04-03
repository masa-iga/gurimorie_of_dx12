struct Output
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer cbuff0 : register(b0)
{
	matrix world;
	matrix viewProj;
}

cbuffer Material : register(b1)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
}
