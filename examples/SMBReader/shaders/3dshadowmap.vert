#version 460
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_spirv_intrinsics : enable
#extension GL_GOOGLE_include_directive: require

#include "include/Math.iglsl"
#include "include/Mesh.iglsl"
#include "include/Lights.iglsl"

layout(set = 0, binding = 0) readonly buffer PMBuffer 
{
    MeshDetails objects[];
} perModelBuffer;

layout(set = 0, binding = 1) readonly buffer InputVertices 
{
	uint8_t vertexData[];
} VertexData;

#include "include/MeshVertexFetch.iglsl"

layout(set = 0, binding = 2) uniform usamplerBuffer globalRenderableIndices;

layout(set = 0, binding = 3) readonly buffer RENDBuffer
{
    MeshRenderable renderables[];
} rends;

layout(set = 0, binding = 4) uniform usamplerBuffer globalSMRenderableStart;
layout(set = 0, binding = 5) uniform usamplerBuffer globalSMPerRendIndices;

layout(set = 0, binding = 8) readonly buffer GeomDescBuffer
{
    GeometryDetails details[];
} geomDetails;

layout(set = 0, binding = 9) readonly buffer GeomInstBuffer
{
    GeometryRenderable geomRenderables[];
} geomRends;

layout(set = 0, binding = 6) readonly buffer ShadowMap 
{
    ShadowMapViewProj viewProjs[];
} sm;

layout(set = 0, binding = 7) uniform shadowViews
{
    ShadowMapView views[128];
} sv;

void main() 
{    
    uint renderableIndex = uint(texelFetch(globalRenderableIndices, gl_DrawID).r);

    uint shadowMapBase = uint(texelFetch(globalSMRenderableStart, int(renderableIndex)).r);

    uint shadowViewProjOffset = uint(texelFetch(globalSMPerRendIndices, int(shadowMapBase) + gl_InstanceIndex).r);

    ShadowMapViewProj viewProj = sm.viewProjs[shadowViewProjOffset];

    ShadowMapView viewSize = sv.views[shadowViewProjOffset];

    MeshRenderable currentRenderable = rends.renderables[renderableIndex];

    GeometryRenderable lGeomRenderable = geomRends.geomRenderables[currentRenderable.geomInstIndex];

    GeometryDetails lGeomDetails = geomDetails.details[lGeomRenderable.geomDescIndex];

    MeshDetails modelData = perModelBuffer.objects[currentRenderable.meshIndex];

    uint comp = modelData.vertexComponents;

    uint stride = modelData.vertexStride;

    uint offset = ((stride * (gl_VertexIndex+1)) + modelData.vertexByteOffset) - 6;

    vec2 viewBase = 2.0 * vec2(viewSize.xOff, viewSize.yOff) - 1.0;

    vec4 scale = vec4(2.0 * viewSize.xScale, 2.0 * viewSize.yScale, 1.0, 1.0);

    mat4 meshWorld = lGeomRenderable.transform * currentRenderable.transform;

    if ((comp&COMPRESSED)==COMPRESSED)
    {
        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = viewProj.shadowMapProj * viewProj.shadowMapView * meshWorld;
            vec4 intPos = vec4(pack6decomp(offset, lGeomDetails.minMaxBox), 1.0f);
            
            vec4 outPos = (MVP * intPos);

            outPos = vec4(outPos.xyz / outPos.w, 1.0);

            vec4 ndcScale = vec4(0.5, 0.5, 1.0, 1.0);
            vec4 ndcOffset = vec4(0.5, 0.5, 0.0, 0.0);

            vec4 outPosNormalize = ndcScale * outPos + ndcOffset;

            gl_Position = scale * outPosNormalize + vec4(viewBase, 0.0, 0.0);
        }
    } 
    else 
    {
        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = viewProj.shadowMapProj * viewProj.shadowMapView * meshWorld;
            vec4 intPos = ReconstructVEC4(offset);
            //gl_Position = (scale * ((MVP * intPos) + ndcOff)) + vec4(viewBase, 0.0, 0.0);
        }
    }
}