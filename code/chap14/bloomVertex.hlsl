#include "bloomHeader.hlsli"

VsOut main(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	VsOut vsOut;
	{
		vsOut.svpos = pos;
		vsOut.uv = uv;
	}
	return vsOut;
}