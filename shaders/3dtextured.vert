#version 450

layout(location = 0) out vec2 texCoords;

vec3 positions[4] = vec3[](
    vec3(-10.0, -10.0, 0.0),
    vec3(10.0, -10.0, 0.0),
    vec3(-10.0, 10.0, 0.0),
    vec3(10.0, 10.0, 0.0)
);

vec2 texcoords[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
);

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
} gs;

layout(set = 1, binding = 0) uniform PerModel {
    mat4 m;
} model;

void main() {
    mat4 MVP = gs.proj * gs.view * model.m;
    gl_Position = MVP * vec4(positions[gl_VertexIndex], 1.0);
    texCoords = texcoords[gl_VertexIndex];
}