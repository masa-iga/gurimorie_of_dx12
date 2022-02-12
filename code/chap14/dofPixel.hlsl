#include "dofHeader.hlsli"

Texture2D<float4> texColor : register(t0);
SamplerState smp : register(s0);

float4 main(VsOut psIn) : SV_TARGET
{
    return texColor.Sample(smp, psIn.uv);
}