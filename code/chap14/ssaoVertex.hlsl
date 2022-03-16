#include "ssaoHeader.hlsli"

VsOut main( float4 pos : POSITION )
{
    VsOut vsOut;
    {
        vsOut.svpos = pos;
        vsOut.uv = float2(0, 0);
    }

	return vsOut;
}