#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) in vec2 adjustedLocalPos;
layout(location = 1) in vec2 rectSize;
layout(location = 2) flat in uint uiDetails; 
layout(location = 3) flat in uint objectID; 
layout(location = 0) out uint outColor;

layout(push_constant) uniform GlobalContext 
{
    float aspect;
} gs;

float roundCorners(vec2 pos, vec2 rectSize, float radius)
{
    vec2 d = abs(pos) - rectSize + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

float rectBorder(vec2 pos, vec2 rectSize, float margin)
{
    vec2 d = abs(pos) - (rectSize - margin);
    
    return (d.x<=0.0 && d.y<=0.0) ? 1.0 : 0.0; //within margin
}

void main() 
{
    float alpha = 1.0;

    uint typeSpecData = GET_TYPE_SPECIFIC_DATA(uiDetails);

    vec2 scaledPos = adjustedLocalPos;

    vec2 scaledRectSize = rectSize;

    scaledPos.x *= gs.aspect;
    scaledRectSize.x *= gs.aspect;

    if ((typeSpecData & ROUNDED_CORNERS) == ROUNDED_CORNERS)
    {
        float dist = roundCorners(scaledPos, scaledRectSize, 0.2);

        float softness = 0.01;

        alpha = 1.0 - smoothstep(0.0, softness, dist);
    }

    if (alpha < 1.0)
        discard;
        
    outColor = objectID;
}