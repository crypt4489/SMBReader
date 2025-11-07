#version 450


layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inTex;

layout(location = 0) out vec2 texCoords;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
} gs;

layout(set = 1, binding = 0) readonly buffer UniformBuffers {
	mat4 InstancedData[];
};

void main() {
    mat4 MVP = gs.proj * gs.view * InstancedData[gl_InstanceIndex];
    gl_Position = MVP * inPos;
    texCoords = vec2(inTex.xy);
}