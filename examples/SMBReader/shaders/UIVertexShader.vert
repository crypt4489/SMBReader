#version 460
#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) out vec4 backGroundColor;
layout(location = 1) out vec2 adjustedLocalPos;
layout(location = 2) out vec2 rectSize;
layout(location = 3) flat out uint uiDetails; 

layout(set = 0, binding = 0) readonly buffer UIContainers
{
    UIContainer renderables[];
} conts;

layout(set = 0, binding = 1) uniform usamplerBuffer uiIndirectionHandles;

layout(set = 0, binding = 2) readonly buffer UIRetainedContainers
{
    UIRetainedContainer renderables[];
} retainedConts;

void main()
{
    uint uiIndirectionHandle = uint(gl_DrawID);

    uint renderableIndex =  uint(texelFetch(uiIndirectionHandles, int(uiIndirectionHandle)).r);

    UIContainer container = conts.renderables[renderableIndex];

    UIRetainedContainer retainedContainer = retainedConts.renderables[renderableIndex];

    vec2 poses[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    vec2 start = retainedContainer.anchorPoint;

    vec2 end = retainedContainer.absoluteSize;

    vec2 mixer = poses[gl_VertexIndex];

    vec2 pos =  2.0 * ((end * mixer) + start) - 1.0;

    gl_Position = vec4(pos, 0.0, 1.0);

    backGroundColor = container.color;

    rectSize = end;

    adjustedLocalPos = (mixer - 0.5) * 2.0 * end;

    uiDetails = container.bitfields.x;
}