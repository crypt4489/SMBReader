#version 460

layout(location = 0) in vec4 backGroundColor;
layout(location = 0) out vec4 outColor;

void main() 
{
    outColor = backGroundColor; 
}