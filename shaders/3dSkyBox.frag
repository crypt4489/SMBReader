#version 460

layout(location = 0) in vec4 inPos;
layout(location = 0) out vec4 color;

layout(set = 1, binding = 0) uniform samplerCube Texture;


void main()
{
	color = texture(Texture, normalize(inPos.xyz));
}