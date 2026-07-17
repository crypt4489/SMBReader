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

bool rectBorder(vec2 pos, vec2 rectSize, float margin)
{
    vec2 d = abs(pos) - (rectSize - margin);
    
    return (d.x<=0.0 && d.y<=0.0); //not within margin
}

void main() 
{
    float alpha = 1.0;

    vec4 fragmentColor = backGroundColor;

    uint typeSpecData = GET_TYPE_SPECIFIC_DATA(uiDetails);

    if ((typeSpecData & ROUNDED_CORNERS) == ROUNDED_CORNERS)
    {
        float dist = roundCorners(adjustedLocalPos, rectSize, 0.2);

        float softness = 0.01;

        if ((typeSpecData & BORDERED_CONTAINER) == BORDERED_CONTAINER)
        {
            float margin = 0.025;

            float marginDraw = dist+margin;

            if (marginDraw > 0.0)
            {
                fragmentColor.xyz = vec3(0.0);
            }
        }

        alpha = 1.0 - smoothstep(0.0, softness, dist);
    }
    else
    {
        if ((typeSpecData & BORDERED_CONTAINER) == BORDERED_CONTAINER)
        {
            bool withinMargin = rectBorder(adjustedLocalPos, rectSize, 0.025);

            if (!withinMargin)
            {
                fragmentColor.xyz = vec3(0.0);
            }
        }
    }

    fragmentColor.w *= alpha;

    outColor = fragmentColor;
}