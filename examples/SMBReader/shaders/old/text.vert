#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 outCoords;
layout(location = 1) out vec4 outColor;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    outCoords = texCoords;
    outColor = color;
}