#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec2 texCoords;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform SampledImage {
    uint imageIndex;
} si;

layout(set = 0, binding = 0) uniform texture2D Textures[3];
layout(set = 0, binding = 1) uniform sampler samplerLinear;

void main() 
{
    outColor = texture(sampler2D(Textures[si.imageIndex], samplerLinear), texCoords);
 
}