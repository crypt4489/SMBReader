#version 460
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_spirv_intrinsics : enable
const uint POSITION = 1u;
const uint TEXTURES1 = 2u;
const uint TEXTURES2 = 4u;
const uint TEXTURES3 = 8u;
const uint NORMAL = 16u;
const uint BONES2 = 32u;
const uint COLOR = 64u;
const uint TANGENT = 128u;
const uint COMPRESSED = 0x80000000u;

const float dx = 3.051851e-05;
const float ax = 0.0009770395;
const float bx = 0.0019550342;


struct AABB
{
    vec4 min;
    vec4 max;
};

struct Renderable
{
	uint meshIndex;
	uint lightCount; 
	uint instanceCount;
	uint pad1;
	uint lightIndices[4];
	uint materialStart;
	uint blendLayersStart;
	uint materialCount;
    uint pad2;
	mat4 transform;
};

struct PerModel
{
    uint vertexComponents;
    uint vertexStride;
    uint indexCount;
	uint firstIndex;
    uint vertexByteOffset;
    uint pad1;
	uint pad2;
	uint pad3;
    AABB minMaxBox;
    vec4 sphere;
};



layout(set = 0, binding = 0) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

layout(set = 0, binding = 1) readonly buffer InputVertices {
	uint8_t vertexData[];
} VertexData;

layout(set = 0, binding = 2) uniform usamplerBuffer globalRenderableIndices;

layout(set = 0, binding = 3) readonly buffer RENDBuffer
{
    Renderable renderables[];
} rends;

layout(set = 0, binding = 4) uniform usamplerBuffer globalSMRenderableStart;
layout(set = 0, binding = 5) uniform usamplerBuffer globalSMPerRendIndices;

struct ShadowMapViewProj
{
    mat4 shadowMapView;
    mat4 shadowMapProj;
};

layout(set = 0, binding = 6) readonly buffer ShadowMap {
    ShadowMapViewProj viewProjs[];
} sm;

struct ShadowMapView
{
	float xOff;
	float yOff;
	float xScale;
	float yScale;
};

layout(set = 0, binding = 7) uniform shadowViews
{
    ShadowMapView views[128];
} sv;

vec4 ReconstructVEC4(uint offset)
{
   uint x = (uint(VertexData.vertexData[offset+3]) << 24 | 
            uint(VertexData.vertexData[offset+2]) << 16 |
            uint(VertexData.vertexData[offset+1]) << 8 |
            uint(VertexData.vertexData[offset+0]));
   uint y = (uint(VertexData.vertexData[offset+7]) << 24 | 
            uint(VertexData.vertexData[offset+6]) << 16 |
            uint(VertexData.vertexData[offset+5]) << 8 |
            uint(VertexData.vertexData[offset+4]));
   uint z = (uint(VertexData.vertexData[offset+11]) << 24 | 
            uint(VertexData.vertexData[offset+10]) << 16 |
            uint(VertexData.vertexData[offset+9]) << 8 |
            uint(VertexData.vertexData[offset+8]));
   uint w = (uint(VertexData.vertexData[offset+15]) << 24 | 
            uint(VertexData.vertexData[offset+14]) << 16 |
            uint(VertexData.vertexData[offset+13]) << 8 |
            uint(VertexData.vertexData[offset+12]));

   return vec4(uintBitsToFloat(x),uintBitsToFloat(y),uintBitsToFloat(z),uintBitsToFloat(w));
}

vec3 ReconstructVEC3(uint offset)
{
   uint x = (uint(VertexData.vertexData[offset+3]) << 24 | 
            uint(VertexData.vertexData[offset+2]) << 16 |
            uint(VertexData.vertexData[offset+1]) << 8 |
            uint(VertexData.vertexData[offset+0]));
   uint y = (uint(VertexData.vertexData[offset+7]) << 24 | 
            uint(VertexData.vertexData[offset+6]) << 16 |
            uint(VertexData.vertexData[offset+5]) << 8 |
            uint(VertexData.vertexData[offset+4]));
   uint z = (uint(VertexData.vertexData[offset+11]) << 24 | 
            uint(VertexData.vertexData[offset+10]) << 16 |
            uint(VertexData.vertexData[offset+9]) << 8 |
            uint(VertexData.vertexData[offset+8]));
   
   return vec3(uintBitsToFloat(x),uintBitsToFloat(y),uintBitsToFloat(z));
}


int unpack_i16(uint offset)
{
    uint lo = uint(VertexData.vertexData[offset + 0]);
    uint hi = uint(VertexData.vertexData[offset + 1]);

    int comp = int((hi << 8) | lo);
    return int((hi << 8) | lo) << 16 >> 16; // sign-extend
}

vec2 converttexcoords16(uint offset)
{
    int tiX = unpack_i16(offset);

    int tiY = unpack_i16(offset+2);

	return vec2(float(tiX), float(tiY)) * dx * 16.0;;
}


vec3 convertnormal(uint offset)
{
    uint lo = uint(VertexData.vertexData[offset + 0]);
    uint lo2 = uint(VertexData.vertexData[offset + 1]);
    uint hi = uint(VertexData.vertexData[offset + 2]);
    uint hi2 = uint(VertexData.vertexData[offset + 3]);

    int normal = int(((hi2&0xff)<<24) | ((hi&0xff)<<16) | ((lo2&0xff)<<8) | (lo&0xff));

    int normx = normal << 21;
    int normy = (normal << 10) & 0xfffff800;
    int normz = (normal & 0xffc00000);


    float x = float(normx) / 2145386496.0;
    float y = float(normy) / 2145386496.0;
    float z = float(normz) / 2143289344.0;

    vec3 ret = vec3(x, y, z);

    return ret;
}

vec3 pack6decomp(uint offset, PerModel model)
{

    int piX = unpack_i16(offset);

    int piY = unpack_i16(offset+2);

    int piZ = unpack_i16(offset+4);

    vec3 unormPos = (((vec3(float(piX), float(piY), float(piZ)) * dx) + 1.0) * 0.5);

    vec3 min = model.minMaxBox.min.xyz;
    vec3 max = model.minMaxBox.max.xyz;

    vec3 pos = mix(min, max, unormPos);

	return pos.xyz;
}

vec4 DecompressTangent(uint offset)
{
    uint lo = uint(VertexData.vertexData[offset + 0]);
    uint lo2 = uint(VertexData.vertexData[offset + 1]);
    uint hi = uint(VertexData.vertexData[offset + 2]);
    uint hi2 = uint(VertexData.vertexData[offset + 3]);

    int ctangent = int(((hi2&0xff)<<24) | ((hi&0xff)<<16) | ((lo2&0xff)<<8) | (lo&0xff));

	uint w = (ctangent & 1);
       
    float wf = -1.0;

    if (w == 1)
    {
        wf = 1.0;
    }

    int zi = int((ctangent & 0xffc00000));
	int yi = int((ctangent & 0x003ff000))<<10;
	int xi = int((ctangent & 0x00000ffc))<<20;

	float x = float(xi) / 2143289344.0;
	float y = float(yi) / 2143289344.0;
	float z = float(zi) / 2143289344.0;


    vec4 ret = vec4(x, y, z, wf);

	return ret;
}

vec4 DecompressColor(uint offset)
{
    uint r = uint(VertexData.vertexData[offset + 0]);
    uint g = uint(VertexData.vertexData[offset + 1]);
    uint b = uint(VertexData.vertexData[offset + 2]);
    uint a = uint(VertexData.vertexData[offset + 3]);

    return vec4(float(r)/255.0, float(g)/255.0, float(b)/255.0, float(a)/255.0);
}

void main() 
{    
    uint renderableIndex = uint(texelFetch(globalRenderableIndices, gl_DrawID).r);

    uint shadowMapBase = uint(texelFetch(globalSMRenderableStart, int(renderableIndex)).r);

    uint shadowViewProjOffset = uint(texelFetch(globalSMPerRendIndices, int(shadowMapBase) + gl_InstanceIndex).r);

    ShadowMapViewProj viewProj = sm.viewProjs[shadowViewProjOffset];

    ShadowMapView viewSize = sv.views[shadowViewProjOffset];

    Renderable currentRenderable = rends.renderables[renderableIndex];

    PerModel modelData = perModelBuffer.objects[currentRenderable.meshIndex];

    uint comp = modelData.vertexComponents;

    uint stride = modelData.vertexStride;

    uint offset = ((stride * (gl_VertexIndex+1)) + modelData.vertexByteOffset) - 6;

    vec2 viewBase = 2.0 * vec2(viewSize.xOff, viewSize.yOff) - 1.0;

    vec4 scale = vec4(2.0 * viewSize.xScale, 2.0 * viewSize.yScale, 1.0, 1.0);


    if ((comp&COMPRESSED)==COMPRESSED)
    {
        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = viewProj.shadowMapProj * viewProj.shadowMapView * currentRenderable.transform;
            vec4 intPos = vec4(pack6decomp(offset, modelData), 1.0f);
            
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
            mat4 MVP = viewProj.shadowMapProj * viewProj.shadowMapView * currentRenderable.transform;
            vec4 intPos = ReconstructVEC4(offset);
            //gl_Position = (scale * ((MVP * intPos) + ndcOff)) + vec4(viewBase, 0.0, 0.0);
        }
    }
}