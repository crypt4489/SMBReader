#version 460

layout(location = 0) in vec4 position;
layout(location = 0) out vec4 outPos;

layout(push_constant) uniform MeshContext {
    mat4 world;
} mesh;

struct Plane
{
	vec4 pointInPlane;
	vec4 planeEquation;
};

struct Frustrum
{
	Plane nearplane;
	Plane farplane;
	Plane topplane;
	Plane bottomplane;
	Plane rightplane;
	Plane leftplane;
	float nearwidth;
	float nearheight;
	float farDistance;
	float nearDistance;
};

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustrum f;
    mat4 world;
} gs;



void main()
{
	outPos = position;

	mat4 viewWithoutTranslate = mat4(mat3(gs.view));

	vec4 pos = (gs.proj * viewWithoutTranslate * position);

	gl_Position = pos.xyww;
}