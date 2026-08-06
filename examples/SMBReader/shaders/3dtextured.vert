#version 460
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_spirv_intrinsics : enable
#extension GL_GOOGLE_include_directive: require

#include "include/Math.iglsl"
#include "include/Mesh.iglsl"

layout(location = 0) out vec4 worldPosition;
layout(location = 1) out vec2 texCoords[8];
layout(location = 9) out vec3 normal;
layout(location = 10) out vec4 outColor;
layout(location = 11) flat out uint renderableIndex;
layout(location = 12) out vec4 tangent;

layout(set = 0, binding = 0) uniform GlobalContext 
{
    mat4 view;
    mat4 proj;
    Frustum f;
    mat4 world;
} gs;

layout(set = 2, binding = 0) readonly buffer PMBuffer 
{
    MeshDetails objects[];
} perModelBuffer;

layout(set = 2, binding = 1) readonly buffer InputVertices 
{
	uint8_t vertexData[];
} VertexData;

#include "include/MeshVertexFetch.iglsl"

layout(set = 2, binding = 2) uniform usamplerBuffer globalRenderableIndices;

layout(set = 2, binding = 6) readonly buffer RENDBuffer
{
    MeshRenderable renderables[];
} rends;

layout(set = 2, binding = 10) readonly buffer GeomDescBuffer
{
    GeometryDetails details[];
} geomDetails;

layout(set = 2, binding = 11) readonly buffer GeomInstBuffer
{
    GeometryRenderable geomRenderables[];
} geomRends;

void main() 
{
    renderableIndex = uint(texelFetch(globalRenderableIndices, gl_DrawID).r);

    MeshRenderable currentRenderable = rends.renderables[renderableIndex];

    MeshDetails modelData = perModelBuffer.objects[currentRenderable.meshIndex];

    GeometryRenderable lGeomRenderable = geomRends.geomRenderables[currentRenderable.geomInstIndex];

    GeometryDetails lGeomDetails = geomDetails.details[lGeomRenderable.geomDescIndex];
    
    mat4 worldMatrix = lGeomRenderable.transform * currentRenderable.transform; 

    uint comp = modelData.vertexComponents;
    normal = vec3(0.0);
    outColor = vec4(1.0);

    uint stride = modelData.vertexStride;

    uint offset = (stride * gl_VertexIndex) + modelData.vertexByteOffset;

    mat3 normalMatrix = AdjointMatrix(worldMatrix);

    if ((comp&COMPRESSED)==COMPRESSED)
    {
        if ((comp&BONES2)==BONES2)
        {
            offset += 4;
        }

        if ((comp & TEXTURES1) == TEXTURES1)
        {
            texCoords[0] = converttexcoords16(offset);
            offset += 4;
        }

        if ((comp & TEXTURES2) == TEXTURES2)
        {
            texCoords[1] = converttexcoords16(offset);
            offset += 4;
        }

        if ((comp & TEXTURES3) == TEXTURES3)
        {
            texCoords[2] = converttexcoords16(offset);
            offset += 4;
        }


        if ((comp&NORMAL)==NORMAL)
        {
            normal = normalMatrix * convertnormal(offset);

            offset += 4;
        }

        if ((comp&TANGENT)==TANGENT)
        {
            vec4 tangentLocal = DecompressTangent(offset);

            vec3 tangentCopy = normalMatrix * tangentLocal.xyz;
            
            tangent = vec4(tangentCopy, tangentLocal.w);

            offset += 4;
        }

        if ((comp&COLOR)==COLOR)
        {
            outColor = DecompressColor(offset);
            offset += 4;
        }

        if ((comp & POSITION) == POSITION)
        {
            mat4 VP = gs.proj * gs.view;
            vec4 intPos = vec4(pack6decomp(offset, lGeomDetails.minMaxBox), 1.0f);
            gl_Position = VP * worldMatrix * intPos;
            worldPosition = worldMatrix * intPos;
        }
    } 
    else 
    {
        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = gs.proj * gs.view * worldMatrix;
            vec4 intPos = ReconstructVEC4(offset);
            gl_Position = MVP * intPos;
            worldPosition = worldMatrix * intPos;
            offset += 16;
        }

        if ((comp&COLOR)==COLOR)
        {
            outColor = ReconstructVEC4(offset);
            offset += 16;
        }

        if ((comp & TEXTURES1) == TEXTURES1)
        {
            texCoords[0] = vec2(ReconstructVEC4(offset+16).xy);
        }

        if ((comp & TEXTURES2) == TEXTURES2)
        {
            texCoords[1] = vec2(ReconstructVEC4(offset+16+8).xy);

        }
    }
}