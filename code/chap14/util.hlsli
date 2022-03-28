static float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy)
{
	float4 ret = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ret += tex.Sample(smp, uv + float2(-2 * dx, 2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 2 * dy)) *  1 / 256;
	
	ret += tex.Sample(smp, uv + float2(-2 * dx, 1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, 0 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, 0 * dy)) * 36 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, 0 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, 0 * dy)) *  6 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, -1 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, -1 * dy)) * 24 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, -1 * dy)) * 16 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, -1 * dy)) *  4 / 256;

	ret += tex.Sample(smp, uv + float2(-2 * dx, -2 * dy)) *  1 / 256;
	ret += tex.Sample(smp, uv + float2(-1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 0 * dx, -2 * dy)) *  6 / 256;
	ret += tex.Sample(smp, uv + float2( 1 * dx, -2 * dy)) *  4 / 256;
	ret += tex.Sample(smp, uv + float2( 2 * dx, -2 * dy)) *  1 / 256;

	return ret;
}

static float random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}
