#version 450
#extension GL_EXT_shader_8bit_storage : enable

const uint POSITION = 0x00000001u;
const uint TEXTURES1 = 0x00000002u;

layout(location = 0) out vec2 texCoords;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
} gs;

layout(set = 2, binding = 0) uniform PerModel {
    mat4 m;
} model;

layout(set = 2, binding = 1) uniform PerModelTextures {
    uint vertexComponents;
    uint numHandles;
    uint vertexStride;
    uint pad2;
    uint textureHandles[12];
} TextureHandles;

layout(set = 2, binding = 3) readonly buffer InputVertices {
	uint8_t vertexData[];
} VertexData;

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

void main() {
    
    uint comp = TextureHandles.vertexComponents;
    texCoords = vec2(0.0, 0.0);

    uint stride = TextureHandles.vertexStride;

    uint offset = stride * gl_VertexIndex;

    if ((comp & POSITION) == POSITION)
    {
        mat4 MVP = gs.proj * gs.view * model.m;
        vec4 intPos = ReconstructVEC4(offset);
        gl_Position = MVP * intPos;
    }

    if ((comp & TEXTURES1) == TEXTURES1)
    {
        texCoords = vec2(ReconstructVEC4(offset+16).xy);
    }
}