#version 460
#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_include_directive: require

#include "include/UI.iglsl"

layout(location = 0) out vec4 textColor;
layout(location = 1) out vec2 textCoords;

layout(set = 0, binding = 0) readonly buffer UIContainers
{
    UIContainer renderables[];
} conts;

layout(set = 0, binding = 1) readonly buffer UITextVertices 
{
	UITextVertex textVerts[];
} textVertices;

void main()
{
    uint textOffset = gl_VertexIndex / 6;

    uint currentVertLocation = gl_VertexIndex % 6;

    uint textIndices[] = uint[](
        0, 
        1,
        2,
        2,
        1, 
        3
    );

    uint vertOffset = 0;

    UITextVertex vert = textVertices.textVerts[vertOffset + textIndices[currentVertLocation]];

    vec2 outVert = 2.0 * vert.pos - 1.0; 

    gl_Position = vec4(outVert, 0.0, 1.0);

    textColor = vec4(vec3(0.0), 1.0);

    textCoords = vert.texCoords;
}