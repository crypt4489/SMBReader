#version 460

layout(location = 0) out vec4 color;


struct Plane
{
	vec4 pointInPlane;
	vec4 planeEquation;
};

struct Frustum
{
	Plane nearplane;
	Plane farplane;
	Plane topplane;
	Plane bottomplane;
	Plane rightplane;
	Plane leftplane;
	float nearwidth;
	float nearheight;
	float farDistance;
    float nearDistance;
};

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustum f;
    mat4 world;
} gs;


void main()
{
   vec2 positions[2] = vec2[](
        vec2(0.0, 0.0), // bottom
        vec2(0.0, 1.0) // top
    );

    gl_Position = vec4(positions[gl_VertexIndex & 1], 0.0, 1.0);
    
    color = vec4(1.0, 0.0, 0.0, 1.0);
}