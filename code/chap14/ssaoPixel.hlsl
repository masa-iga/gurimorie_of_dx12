#include "ssaoHeader.hlsli"
#include "commonParam.hlsli"

Texture2D<float> texDepth : register(t0);
Texture2D<float4> texNormal : register(t1);
SamplerState smp : register(s0);

float4 main(VsOut vsOut) : SV_TARGET
{
#if 0
	return float4(vsOut.uv.x, vsOut.uv.y, 0.0f, 1.0f);
#else
    return texDepth.Sample(smp, vsOut.uv);
    //return texNormal.Sample(smp, vsOut.uv);
#endif
}