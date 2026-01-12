#version 460
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_ARB_shader_draw_parameters : require


struct DebugDrawStruct
{
	vec4 center;
	vec4 scale;
    vec4 color;
    vec4 halfExtents;
};

struct Plane
{
	vec4 pointInPlane;
	vec4 planeEquation;
};

struct Frustrum
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
};


layout(location = 0) out vec4 color;
layout(location = 1) flat out uint modelIndex;


layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustrum f;
    mat4 world;
} gs;


layout(set = 1, binding = 0) readonly buffer DDSBuffer {
    DebugDrawStruct objects[];
} ddsbuffer;

layout(set = 1, binding = 1) uniform usamplerBuffer globalDebugIndices;

layout(set = 1, binding = 2) uniform usamplerBuffer globalDebugTypes;


vec4 boxStrides[8] =


{
    vec4(-1.0, 1.0, -1.0, 0.0),
    vec4(1.0, 1.0, -1.0, 0.0),
    vec4(1.0, 1.0, 1.0, 0.0),
    vec4(-1.0, 1.0, 1.0, 0.0),
     vec4(-1.0, -1.0, -1.0, 0.0),
    vec4(1.0, -1.0, -1.0, 0.0),
    vec4(1.0, -1.0, 1.0, 0.0),
    vec4(-1.0, -1.0, 1.0, 0.0)


};

uint boxIndices[24] =
{
0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7
};


mat4 CreateBillboardMatrix(vec3 camWorld, vec4 objPos)
{
    vec3 look = normalize(camWorld - objPos.xyz);

    vec3 right = normalize(cross(vec3(0, 1.0, 0), look));
    vec3 up = cross(look, right);

    mat4 ret = mat4(1.0);
    ret[0].xyz = right;
    ret[1].xyz = up;
    ret[2].xyz = look;

    return ret;
}

void main() 
{
    uint modelIndex = uint(texelFetch(globalDebugIndices, gl_DrawID).r);

    uint debugType = uint(texelFetch(globalDebugTypes, int(modelIndex)).r);

    DebugDrawStruct mainStruct = ddsbuffer.objects[modelIndex];

    color = mainStruct.color;

    mat4 VP = gs.proj * gs.view;

    if (debugType == 1)
    {
        vec4 pos = (mainStruct.center + (boxStrides[boxIndices[gl_VertexIndex]] * mainStruct.halfExtents) * mainStruct.scale);

        gl_Position = VP * pos;
    } 
    else if (debugType == 2)
    {
       
       uint sphereIndex = uint(gl_VertexIndex) / 2;
       sphereIndex += uint(gl_VertexIndex) & 1;
    
        float r = mainStruct.halfExtents.x;
        uint numSlices = floatBitsToUint(mainStruct.halfExtents.y);
        float x = r * cos(radians((360/numSlices)*sphereIndex));
        float y = r * sin(radians((360/numSlices)*sphereIndex));
 
        vec4 pos = vec4(mainStruct.center.x+x, mainStruct.center.y+y, mainStruct.center.z, 1.0);

        mat4 billboardMat = CreateBillboardMatrix(gs.world[3].xyz, pos);

        gl_Position = VP  * pos;

    }
}