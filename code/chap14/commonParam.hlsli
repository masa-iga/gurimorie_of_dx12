/* Common resource, assigned to space 1 */
cbuffer SceneParam : register(b0, space1)
{
	matrix view;
	matrix proj;
    matrix invProj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
    float highLuminanceThreshold;
}
