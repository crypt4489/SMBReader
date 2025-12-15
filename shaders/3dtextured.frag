#version 450

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D Textures[1024];

layout(set = 2, binding = 1) uniform PerModel {
    uint vertexComponents;
    uint numHandles;
    uint vertexStride;
    uint pad2;
    uint textureHandles[12];
} TextureHandles;

void main() {

    uint textureIndex = TextureHandles.textureHandles[0];

    outColor = texture(Textures[textureIndex], texCoords);

}