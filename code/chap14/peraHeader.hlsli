Texture2D<float4> tex : register(t0);
Texture2D<float4> effectTex : register(t1);
SamplerState smp : register(s0);

struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer PostEffect : register(b0)
{
	float4 bkweights[2];
};