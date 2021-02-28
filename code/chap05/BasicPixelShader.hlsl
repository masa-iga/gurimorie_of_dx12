#include "BasicShaderHeader.hlsli"

float4 BasicPs(Output input) : SV_TARGET
{
	return float4(input.uv, 1, 1);
}