#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/Math.iglsl"

layout(location = 0) in vec4 position;
layout(location = 0) out vec4 outPos;

layout(push_constant) uniform MeshContext 
{
    mat4 world;
} mesh;

layout(set = 0, binding = 0) uniform GlobalContext 
{
    mat4 view;
    mat4 proj;
    Frustum f;
    mat4 world;
} gs;

void main()
{
	outPos = position;

	mat4 viewWithoutTranslate = mat4(mat3(gs.view));

	vec4 pos = (gs.proj * viewWithoutTranslate * position);

	gl_Position = pos.xyww;
}