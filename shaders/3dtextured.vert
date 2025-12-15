#version 450
#extension GL_EXT_shader_8bit_storage : enable

const uint POSITION = 1u;
const uint TEXTURES1 = 2u;
const uint TEXTURES2 = 4u;
const uint TEXTURES3 = 8u;
const uint NORMAL = 16u;
const uint BONES2 = 32u;
const uint COMPRESSED = 0x80000000u;

const float dx = 3.051851e-05;
const float ax = 0.0009770395;
const float bx = 0.0019550342;




struct AABB
{
    vec4 min;
    vec4 max;
};

layout(location = 0) out vec2 texCoords;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
} gs;

layout(set = 2, binding = 0) uniform PerModel {
    mat4 m;
    AABB minMaxBox;
} model;

layout(set = 2, binding = 1) uniform PerModelTextures {
    uint vertexComponents;
    uint numHandles;
    uint vertexStride;
    uint pad2;
    uint textureHandles[12];
} TextureHandles;

layout(set = 2, binding = 2) readonly buffer InputVertices {
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

vec3 pack6decomp(uint offset)
{

    int piX = unpack_i16(offset);

    int piY = unpack_i16(offset+2);

    int piZ = unpack_i16(offset+4);

    vec3 unormPos = (((vec3(float(piX), float(piY), float(piZ)) * dx) + 1.0) * 0.5);

	return mix(model.minMaxBox.min.xyz, model.minMaxBox.max.xyz, unormPos);
}


void main() {
    
    uint comp = TextureHandles.vertexComponents;
    texCoords = vec2(0.0, 0.0);

    uint stride = TextureHandles.vertexStride;

    uint offset = stride * gl_VertexIndex;

    

    if ((comp&COMPRESSED)==COMPRESSED)
    {
        if ((comp&BONES2)==BONES2)
        {
            offset += 4;
        }

         if ((comp & TEXTURES1) == TEXTURES1)
        {
            texCoords = converttexcoords16(offset);
            offset += 4;
        }

        if ((comp & TEXTURES2) == TEXTURES2)
        {
            offset += 4;
        }


        if ((comp&NORMAL)==NORMAL)
        {
            offset += 4;
        }

        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = gs.proj * gs.view * model.m;
            vec4 intPos = vec4(pack6decomp(offset), 1.0f);
            gl_Position = MVP * intPos;
        }

    } 
    else 
    {
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
}