#version 450

layout(location = 0) in vec2 texCoords;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D Textures[1024];

layout(set = 2, binding = 1) uniform PerModel {
    uint numHandles;
    uint diffuseTexture;
    uint normalTexture;
} TextureHandles;

void main() {

    uint textureIndex = TextureHandles.diffuseTexture;

    outColor = texture(Textures[textureIndex], texCoords);

}