#include "peraHeader.hlsli"

float4 main(Output input) : SV_TARGET
{
	float w, h, level;
	tex.GetDimensions(0, w, h, level);

	const float dy = 1.0f / h;
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);
	const float4 col = tex.Sample(smp, input.uv);

	ret += bkweights[0] * col;

	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(0, dy * i));
		ret += bkweights[i >> 2][(uint)i % 4] * tex.Sample(smp, input.uv + float2(0, -dy * i));
	}

	return col;
}