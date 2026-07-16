#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) in vec4 backGroundColor;
layout(location = 1) in vec2 adjustedLocalPos;
layout(location = 2) in vec2 rectSize;
layout(location = 3) flat in uint uiDetails; 
layout(location = 0) out vec4 outColor;


float roundCorners(vec2 pos, vec2 rectSize, float radius)
{
    vec2 d = abs(pos) - rectSize + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

void main() 
{
    float alpha = 1.0;

    if ((GET_TYPE_SPECIFIC_DATA(uiDetails) & ROUNDED_CORNERS) == ROUNDED_CORNERS)
    {
        float dist = roundCorners(adjustedLocalPos, rectSize, 0.2);

        float softness = 0.01;

        alpha = 1.0 - smoothstep(0.0, softness, dist);

        if (alpha < 0.001)
            discard;
    }

    outColor = backGroundColor; 

    outColor.w *= alpha;
}