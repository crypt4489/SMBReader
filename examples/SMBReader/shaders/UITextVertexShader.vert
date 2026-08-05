#version 460
#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) out vec4 textColor;
layout(location = 1) out vec2 textCoords;
layout(location = 2) flat out uint fontIndex;

layout(set = 0, binding = 0) readonly buffer UIContainers
{
    UIContainer renderables[];
} conts;

layout(set = 0, binding = 1) readonly buffer UITextVertices 
{
	UITextVertex textVerts[];
} textVertices;

layout(set = 0, binding = 4) uniform usamplerBuffer UITextToContainerID;

void main()
{
    uint drawID = gl_DrawID;

    uint uiIndex = uint(texelFetch(UITextToContainerID, int(drawID)).r);

    uvec2 bitfields = conts.renderables[uiIndex].bitfields.zw;

    fontIndex = GET_TEXT_INFO_FONT_INDEX(bitfields.y);

    uint textOffset = GET_TEXT_BUFFER_LOCATION(bitfields.x);

    uint currentVertLocation = gl_VertexIndex % 6;

    uint inTexVertOffset = (gl_VertexIndex / 6) * 4;

    uint textIndices[] = uint[](
        0, 
        1,
        2,
        2,
        1, 
        3
    );

    uint vertOffset = textOffset * 4;

    UITextVertex vert = textVertices.textVerts[vertOffset + inTexVertOffset + textIndices[currentVertLocation]];

    vec2 outVert = 2.0 * vert.pos - 1.0; 

    gl_Position = vec4(outVert, 0.0, 1.0);

    textColor =  makeColorFrom10_11_10_1(conts.renderables[uiIndex].packedData.z);

    textCoords = vert.texCoords;
}