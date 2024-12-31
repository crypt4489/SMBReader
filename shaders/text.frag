#version 450

layout(location = 0) in vec2 texCoords;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D baseSampler;

void main() {
    outColor = texture(baseSampler, texCoords);
}