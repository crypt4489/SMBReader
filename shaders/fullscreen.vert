#version 450

layout(location = 0) out vec2 texCoords;

void main()
{
    vec2 positions[4] = vec2[](
        vec2(-1.0, -1.0), // bottom-left
        vec2( 1.0, -1.0), // bottom-right
        vec2(-1.0,  1.0), // top-left
        vec2( 1.0,  1.0)  // top-right
    );

    vec2 uvs[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    texCoords = uvs[gl_VertexIndex];
}