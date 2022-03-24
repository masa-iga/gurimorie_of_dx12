cbuffer SceneParam : register(b0)
{
	matrix view;
	matrix proj;
    matrix invProj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
    float highLuminanceThreshold;
}
