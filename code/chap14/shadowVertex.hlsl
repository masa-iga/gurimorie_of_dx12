struct Output
{
	float4 svpos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Output main(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	Output output;
	{
		output.svpos = pos;
		output.uv = uv;
	}

	return output;
}