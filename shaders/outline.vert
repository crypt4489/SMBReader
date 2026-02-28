#version 460
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_ARB_shader_draw_parameters : require

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

layout(push_constant) uniform OutlineContext {
    vec4 color;
    float uniScale;
} outline;


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
    float nearDistance;
};


layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustrum f;
    mat4 world;
} gs;


layout(set = 2, binding = 0) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

layout(set = 2, binding = 1) readonly buffer InputVertices {
	uint8_t vertexData[];
} VertexData;

layout(set = 2, binding = 2) uniform usamplerBuffer globalRenderableIndices;

layout(set = 2, binding = 3) readonly buffer RENDBuffer
{
    Renderable renderables[];
} rends;

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


void main() {
    

    uint renderableIndex = uint(texelFetch(globalRenderableIndices, gl_DrawID).r);

    Renderable currentRenderable = rends.renderables[renderableIndex];

    PerModel modelData = perModelBuffer.objects[currentRenderable.meshIndex];

    uint comp = modelData.vertexComponents;

    uint stride = modelData.vertexStride;

    uint offset = (stride * gl_VertexIndex) + modelData.vertexByteOffset;

    outColor = outline.color;

    mat4 scaleMatrix = currentRenderable.transform;
    scaleMatrix[0] *= outline.uniScale;
    scaleMatrix[1] *= outline.uniScale;
    scaleMatrix[2] *= outline.uniScale;

    if ((comp&COMPRESSED)==COMPRESSED)
    {
        if ((comp&BONES2)==BONES2)
        {
            offset += 4;
        }

        if ((comp & TEXTURES1) == TEXTURES1)
        {
            //texCoords = converttexcoords16(offset);
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

        if ((comp&TANGENT)==TANGENT)
        {
            offset += 4;
        }

        if ((comp&COLOR)==COLOR)
        {
       
            offset += 16;
        }

        if ((comp & POSITION) == POSITION)
        {
            mat4 VP = gs.proj * gs.view;
            vec4 intPos = vec4(pack6decomp(offset, modelData), 1.0f);
            gl_Position = VP  * scaleMatrix * intPos;
        }

    } 
    else 
    {
        if ((comp & POSITION) == POSITION)
        {
            mat4 MVP = gs.proj * gs.view * scaleMatrix;
            vec4 intPos = ReconstructVEC4(offset);
            gl_Position = MVP * intPos;
        }
    }
}