#include "dofHeader.hlsli"

Texture2D<float4> texColor : register(t0);
Texture2D<float4> tesShrinkColor : register(t1);
Texture2D<float> texDepth : register(t2);
SamplerState smp : register(s0);

float4 main(VsOut psIn) : SV_TARGET
{
    return float4(0, 0, 1, 1);
}

float4 copyTex(VsOut psIn) : SV_TARGET
{
    return texColor.Sample(smp, psIn.uv);
}