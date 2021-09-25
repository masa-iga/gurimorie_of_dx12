struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D<float> tex : register(t0);
SamplerState smp : register(s0);

float4 main(Output input) : SV_TARGET
{
	const float dep = tex.Sample(smp, input.uv);
	return float4(dep, dep, dep, 1);
}