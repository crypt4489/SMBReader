#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoords[8];
layout(location = 10) in vec3 inNormal;
layout(location = 11) in vec4 vertColor;
layout(location = 12) flat in uint renderableIndex;
layout(location = 0) out vec4 outColor;


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
	uint materialCount;
	uint blendLayersStart;
	uint blendLayerCount;
	mat4 transform;
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

struct LightSource
{
	vec4 color; //w is intensity;
	vec4 pos; //w is radius for point right
	vec4 direction; //w is unused;
	vec4 ancillary; //for spot, x, y are cosine theta cutoffs //ambient for directional
};

layout(set = 2, binding = 3) readonly buffer GLBuffer {
    LightSource objects[];
} lightBuffer;

layout(set = 2, binding = 4) uniform usamplerBuffer globalLightTypes;


const uint ALBEDOMAPPED = 4u;
const uint NORMALMAPPED = 8u;
const uint VERTEXNORMAL = 16u;

struct Material
{
	uint materialFlags;
	uint unused1;
	uint unused2;
	uint unused3;
	vec4 albedoColor;
	uvec4 textureHandles; // y - normalMapCoordinates // x- albedo
};

layout(set = 2, binding = 5) uniform MaterialContext {
   Material matsData[85];
} mats;

layout(set = 2, binding = 6) readonly buffer RENDBuffer {
    Renderable renderables[];
} rends;

layout(set = 2, binding = 7) uniform usamplerBuffer globalMaterialIndices;

vec4 DoLights(vec3 normal, vec3 worldPos, vec4 color, Renderable modelData)
{
    if (modelData.lightCount == 0) return color;

    vec4 cumColor = vec4(0.0, 0.0, 0.0, 1.0);

    for (uint i = 0; i<modelData.lightCount; i++)
    {
        LightSource light = lightBuffer.objects[modelData.lightIndices[i]];

        uint lType = uint(texelFetch(globalLightTypes, int(modelData.lightIndices[i])).r);

        if (lType == 0)
        {
            vec3 lightDir = normalize(-light.direction.xyz); 

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            cumColor += ((vec4((light.color.xyz/255.0) * diffuse * light.color.w, 1.0) * color) + (color * vec4(light.ancillary.xyz, 1.0)));
        }
        
        else if (lType == 1)
        {
            vec3 lightDir = normalize(light.pos.xyz - worldPos);

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            float distance = length(light.pos.xyz - worldPos);

            float attenuation = (1.0/(.01 + (.25 * distance) + (.67 * distance * distance)));

            cumColor += (vec4((light.color.xyz/255.0) * light.color.w, 1.0) * diffuse * attenuation * color) + (vec4(light.ancillary.xyz, 1.0) * attenuation * color);
        }
        else if (lType == 2)
        {
            vec3 lightLook =  light.pos.xyz - worldPos;

            float distance = length(lightLook);

            lightLook /= distance;

            vec3 lightDir = normalize(-light.direction.xyz);

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            float cosTheta = dot(lightLook, lightDir);

            float spotFactor = smoothstep(cos(light.ancillary.y), cos(light.ancillary.x), cosTheta);

            cumColor += (vec4((light.color.xyz/255.0) * light.color.w, 1.0) * diffuse * spotFactor * (1.0/(distance*distance)) * color);
        }
    }

    return vec4(cumColor.xyz, 1.0);
}

void main() {

    Renderable modelData = rends.renderables[renderableIndex];

    vec4 totalColor = vertColor;

    uint materialStart = modelData.materialStart;

    uint textureIndex = 0;

    for (uint i = 0; i<modelData.materialCount; i++)
    {

        uint mateiralIndex = uint(texelFetch(globalMaterialIndices, int(materialStart + i)).r);

        Material mat = mats.matsData[mateiralIndex];

        vec4 albedoColor = mat.albedoColor;
        
        if ((mat.materialFlags & ALBEDOMAPPED) == ALBEDOMAPPED)
        {
           albedoColor *= texture(sampler2D(Textures[mat.textureHandles.x], samplerLinear), texCoords[textureIndex++]);
        }
    
        if ((mat.materialFlags & NORMALMAPPED) == NORMALMAPPED)
        {
            vec3 normal = texture(sampler2D(Textures[mat.textureHandles.y], samplerLinear), texCoords[textureIndex++]).rgb;

            normal = transpose(inverse(mat3(modelData.transform))) * (2.0 * normal.xyz - 1.0);

            albedoColor = DoLights(normal, position, albedoColor, modelData);
        } 

        else if ((mat.materialFlags & VERTEXNORMAL) == VERTEXNORMAL)
        {
            vec3 normal = normalize(transpose(inverse(mat3(modelData.transform))) * inNormal);

            albedoColor = DoLights(normal, position, albedoColor, modelData);
        }

        totalColor *= albedoColor;
    }

    outColor = totalColor;
}