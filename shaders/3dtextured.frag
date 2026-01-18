#version 460
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec2 texCoords;
layout(location = 1) flat in uint modelIdx;

layout(location = 0) out vec4 outColor;


struct AABB
{
    vec4 min;
    vec4 max;
};

struct PerModel
{
    uint vertexComponents;
    uint numHandles;
    uint vertexStride;
    uint indexCount;
	uint instanceCount;
	uint firstIndex;
    uint firstVertex;
    uint textureHandles[9];
    mat4 m;
    AABB minMaxBox;
    vec4 sphere;
};


layout(set = 1, binding = 0) uniform sampler2D Textures[1024];

layout(set = 2, binding = 0) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

struct LightSource
{
	vec4 color; //w is theta
	vec4 pos; //w is radius for point light
};

layout(set = 2, binding = 3) readonly buffer GLBuffer {
    LightSource objects[];
} lightBuffer;

void main() {

    PerModel modelData = perModelBuffer.objects[modelIdx];

    uint textureIndex = modelData.textureHandles[0];

    outColor = texture(Textures[textureIndex], texCoords);

}