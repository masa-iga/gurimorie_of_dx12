struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D<float> texR : register(t0);
Texture2D<float4> texRgba : register(t1);
SamplerState smp : register(s0);

float4 main(Output input) : SV_TARGET
{
	const float dep = texR.Sample(smp, input.uv);
	return float4(dep, dep, dep, 1);
}

float4 mainRgba(Output input) : SV_TARGET
{
	return texRgba.Sample(smp, input.uv);
}

float4 black(Output input) : SV_TARGET
{
	return float4(0, 0, 0, 1);
}