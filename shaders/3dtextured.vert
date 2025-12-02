#version 450


layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inTex;

layout(location = 0) out vec2 texCoords;
layout(location = 1) flat out int instanceID;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
} gs;

layout(set = 2, binding = 0) uniform PerModel {
    mat4 m;
} model;

void main() {
    mat4 MVP = gs.proj * gs.view * model.m;
    gl_Position = MVP * inPos;
    texCoords = vec2(inTex.xy);
}