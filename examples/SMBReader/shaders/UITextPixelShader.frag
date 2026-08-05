#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_nonuniform_qualifier : require

#include "include/UI.iglsl"

layout(location = 0) in vec4 textColor;
layout(location = 1) in vec2 textCoords;
layout(location = 2) flat in uint fontIndex;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform texture2D fontMap[];
layout(set = 0, binding = 3) uniform sampler samplerLinear;

void main() 
{
    float alpha = texture(sampler2D(fontMap[fontIndex], samplerLinear), textCoords).r;
   
    outColor = vec4(textColor.xyz, alpha);
}