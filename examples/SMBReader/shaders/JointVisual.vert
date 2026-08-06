#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/Math.iglsl"

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 color;

layout(push_constant) uniform JointContext 
{
    uint startingLocation;
} jointStart;

layout(set = 0, binding = 0) uniform GlobalContext 
{
    mat4 view;
    mat4 proj;
    Frustum f;
    mat4 world;
} gs;

layout(set = 1, binding = 0) readonly buffer JointMeshWorlds
{
    mat4 worldMats[];
} jmw;

layout(set = 1, binding = 1) uniform usamplerBuffer jointParentIndices;

void main()
{
	vec4 pos = (jmw.worldMats[gl_InstanceIndex + jointStart.startingLocation] * vec4(position.xyz, 1.0));
    
	gl_Position = gs.proj * gs.view * pos;

    color = inColor;
}