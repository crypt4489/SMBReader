#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) in vec4 textColor;
layout(location = 1) in vec2 textCoords;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform texture2D fontMap;
layout(set = 0, binding = 3) uniform sampler samplerLinear;

void main() 
{
    vec3 fontColor = texture(sampler2D(fontMap, samplerLinear), textCoords).rgb;

    if (fontColor == vec3(0.0))
        discard;
   
    outColor = vec4(textColor.xyz * fontColor.xyz, 1.0);
}