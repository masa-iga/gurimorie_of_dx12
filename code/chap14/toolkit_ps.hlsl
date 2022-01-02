cbuffer OutputColor : register(b0)
{
	float r0;
	float g0;
	float b0;
	float a0;
}

cbuffer OutputColor : register(b1)
{
	float r1;
	float g1;
	float b1;
	float a1;
}

float4 main() : SV_TARGET
{
	return float4(r0, g0, b0, a0);
}

float4 main2() : SV_TARGET
{
	return float4(r1, g1, b1, a1);
}
