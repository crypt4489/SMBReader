#version 460

#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) in vec4 backGroundColor;
layout(location = 1) flat in uvec4 uiAncillaryData;
layout(location = 2) in vec2 adjustedLocalPos;
layout(location = 3) in vec2 rectSize;
layout(location = 4) flat in uint uiDetails; 
layout(location = 5) flat in uvec4 uiRetainedData;
layout(location = 0) out vec4 outColor;

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

    vec4 fragmentColor = backGroundColor;

    uint typeSpecData = GET_TYPE_SPECIFIC_DATA(uiDetails);

    vec2 scaledPos = adjustedLocalPos;

    vec2 scaledRectSize = rectSize;

    vec4 marginColor = makeColorFrom10_11_10_1(uiAncillaryData.x);

    scaledPos.x *= gs.aspect;
    scaledRectSize.x *= gs.aspect;

    if ((typeSpecData & ROUNDED_CORNERS) == ROUNDED_CORNERS)
    {
        float dist = roundCorners(scaledPos, scaledRectSize, 0.2);

        float softness = 0.01;

        if ((typeSpecData & BORDERED_CONTAINER) == BORDERED_CONTAINER)
        {
            float margin = 0.025;

            float aa = fwidth(dist) * .4;

            float aliasedMarginColor = smoothstep(-margin - aa, -margin + aa, dist);
            
            fragmentColor = mix(fragmentColor, marginColor, aliasedMarginColor);
        }

        alpha = 1.0 - smoothstep(0.0, softness, dist);
    }
    else
    {
        if ((typeSpecData & BORDERED_CONTAINER) == BORDERED_CONTAINER)
        {
            float withinMargin = rectBorder(scaledPos, scaledRectSize, 0.025);

            fragmentColor = mix(marginColor, fragmentColor, withinMargin);
        }
    }

    fragmentColor.w *= alpha;

    if ((uiRetainedData.x & 1) == 1)
    {
        fragmentColor.xyz = vec3(0.0);
    }

    outColor = fragmentColor;
}