#version 450

layout(location = 0) in vec2 texCoords;

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
    uint unused;
    uint textureHandles[12];
    mat4 m;
    AABB minMaxBox;
};

layout(set = 1, binding = 0) uniform sampler2D Textures[1024];

layout(set = 2, binding = 0) uniform ModelIndex {
    uint modelIndex;
} modelI;

layout(set = 2, binding = 1) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

void main() {

    PerModel modelData = perModelBuffer.objects[modelI.modelIndex];

    uint textureIndex = modelData.textureHandles[0];

    outColor = texture(Textures[textureIndex], texCoords);

}