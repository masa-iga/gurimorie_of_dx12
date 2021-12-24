cbuffer OutputColor : register(b0)
{
	float r;
	float g;
	float b;
	float a;
}

float4 main() : SV_TARGET
{
	return float4(r, g, b, a);
}