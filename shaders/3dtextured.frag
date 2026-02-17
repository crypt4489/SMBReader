#version 460
#extension GL_EXT_nonuniform_qualifier : require
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

layout(set = 1, binding = 0) uniform texture2D Textures[];
layout(set = 1, binding = 1) uniform sampler samplerLinear;

layout(set = 2, binding = 0) readonly buffer PMBuffer {
    PerModel objects[];
} perModelBuffer;

struct LightSource
{
	vec4 color; //w is intensity;
	vec4 pos; //w is radius for point right
	vec4 direction; //w is unused;
	vec4 ancillary; //for spot, x, y are cosine theta cutoffs
};

layout(set = 2, binding = 3) readonly buffer GLBuffer {
    LightSource objects[];
} lightBuffer;

layout(set = 2, binding = 4) uniform usamplerBuffer globalLightTypes;

vec4 DoLights(vec3 normal, vec3 worldPos, vec4 color, PerModel modelData)
{

    vec4 cumColor = vec4(0.0, 0.0, 0.0, 1.0);
    for (uint i = 0; i<modelData.lightCount; i++)
    {
        LightSource light = lightBuffer.objects[modelData.lightIndex[i]];

        uint lType = uint(texelFetch(globalLightTypes, int(modelData.lightIndex[i])).r);

        if (lType == 0)
        {
            vec3 lightDir = -light.direction.xyz; 

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            cumColor += vec4(light.color.xyz * diffuse * light.color.w, 1.0);
        }
        else if (lType == 1)
        {
            vec3 lightDir = normalize(light.pos.xyz - worldPos);

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            float distance = length(light.pos.xyz - worldPos);

            cumColor += (vec4(light.color.xyz, 1.0) * diffuse * 1.0/(.01 + (.25 * distance) + (.67 * distance*distance)));
        }
        else if (lType == 2)
        {
            vec3 lightLook =  light.pos.xyz - worldPos;

            float distance = length(lightLook);

            lightLook /= distance;

            vec3 lightDir = -light.direction.xyz;

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            float cosTheta = dot(lightLook, lightDir);

            float spotFactor = smoothstep(cos(light.ancillary.y), cos(light.ancillary.x), cosTheta);

            cumColor += (vec4(light.color.xyz * light.color.w, 1.0) * diffuse * spotFactor * (1.0/(distance*distance)));
        }
    }

    return color * cumColor;
}

void main() {

    PerModel modelData = perModelBuffer.objects[modelIdx];

    uint textureIndex = modelData.textureHandles[0];

    vec4 albedoColor = texture(sampler2D(Textures[textureIndex], samplerLinear), texCoords);

    if (modelData.numHandles > 1)
    {
        textureIndex = modelData.textureHandles[1];

        vec3 normal = texture(sampler2D(Textures[textureIndex], samplerLinear), texCoords2).rgb;

        normal = transpose(inverse(mat3(modelData.m))) * (2.0 * normal.xyz - 1.0);

        albedoColor = DoLights(normal, position, albedoColor, modelData);
    } 
    else 
    {
        vec3 normal = normalize(transpose(inverse(mat3(modelData.m))) * inNormal);

        albedoColor = DoLights(normal, position, albedoColor, modelData);
    }

    outColor = albedoColor;

}