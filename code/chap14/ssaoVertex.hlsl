#include "ssaoHeader.hlsli"

VsOut main( float3 pos : POSITION , float2 uv : TEXCOORD)
{
    VsOut vsOut;
    {
        vsOut.svpos = float4(pos, 1.0f);
        vsOut.uv = uv;
    }

	return vsOut;
}