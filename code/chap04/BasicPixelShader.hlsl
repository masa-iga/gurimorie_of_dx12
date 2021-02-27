struct Input {
	float4 pos : POSITION;
	float4 svpos : SV_POSITION;
};

float4 BasicPs(Input input) : SV_TARGET
{
	return float4(input.pos.x, input.pos.y, 1, 1);
}