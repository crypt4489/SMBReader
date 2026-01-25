#version 460
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec2 texCoords2;
layout(location = 3) in vec3 inNormal;
layout(location = 4) flat in uint modelIdx;
layout(location = 0) out vec4 outColor;


struct AABB
{
    vec4 min;
    vec4 max;
};

struct PerModel
{
    uint vertexComponents;
    uint numHandles;
    uint vertexStride;
    uint indexCount;
	uint instanceCount;
	uint firstIndex;
    uint vertexByteOffset;
    uint lightCount;
    uint textureHandles[4];
    uint lightIndex[4];
    mat4 m;
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
};

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustrum f;
    mat4 world;
} gs;

layout(set = 1, binding = 0) uniform sampler2D Textures[1024];

layout(set = 2, binding = 0) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

struct LightSource
{
	vec4 color; //w is theta
	vec4 pos; //w is radius for point light
};

layout(set = 2, binding = 3) readonly buffer GLBuffer {
    LightSource objects[];
} lightBuffer;

void main() {

    PerModel modelData = perModelBuffer.objects[modelIdx];

    uint textureIndex = modelData.textureHandles[0];

    vec4 albedoColor = texture(Textures[textureIndex], texCoords);

    if (modelData.numHandles > 1)
    {
        textureIndex = modelData.textureHandles[1];

        vec3 normal = texture(Textures[textureIndex], texCoords2).bgr;

        normal = transpose(inverse(mat3(modelData.m))) * (2.0 * normal.xyz - 1.0);

        LightSource light = lightBuffer.objects[modelData.lightIndex[0]];

        vec3 lightDir = normalize(light.pos.xyz - position);

        float diffuse = max(dot(normal.xyz, lightDir), 0.0);

        float distance = length(light.pos.xyz - position);

        albedoColor *= (vec4(229, 211, 191, 1.0) * diffuse * 1.0/(.01 + (.25 * distance) + (.67 * distance*distance)));
       
    
    } 
    else 
    {
        vec3 normal = normalize(transpose(inverse(mat3(modelData.m))) * inNormal);

        LightSource light = lightBuffer.objects[modelData.lightIndex[0]];

        vec3 lightDir = normalize(light.pos.xyz - position);

        float diffuse = max(dot(normal.xyz, lightDir), 0.0);

        float distance = length(light.pos.xyz - position);

        albedoColor *= (vec4(229, 211, 191, 1.0) * diffuse * 1.0/(.01 + (.25 * distance) + (.67 * distance*distance)));
    }

    outColor = albedoColor;

}