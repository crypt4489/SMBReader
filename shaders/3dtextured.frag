#version 460
#extension GL_EXT_nonuniform_qualifier : require
layout(location = 0) in vec4 worldPosition;
layout(location = 1) in vec2 texCoords[8];
layout(location = 9) in vec3 inNormal;
layout(location = 10) in vec4 vertColor;
layout(location = 11) flat in uint renderableIndex;
layout(location = 12) in vec4 inTangent;
layout(location = 0) out vec4 outColor;


struct AABB
{
    vec4 min;
    vec4 max;
};

struct GeometryDetails
{
	AABB minMaxBox;
};

struct GeometryRenderable
{
	uint geomDescIndex;
	uint renderableStart;
	uint renderableCount;
	uint pad1;
	mat4 transform;
};

struct MeshRenderable
{
	uint meshIndex;
	uint lightCount; 
	uint instanceCount;
	uint geomInstIndex;
	uint lightIndices[4];
	uint materialStart;
	uint blendLayersStart;
	uint materialCount;
    uint pad2;
	mat4 transform;
};


struct Plane
{
	vec4 pointInPlane;
	vec4 planeEquation;
};

struct Frustum
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

layout(push_constant) uniform ShadowImageConstants {
    uint imageIndex;
    uint currentFrame;
} sic;

layout(set = 0, binding = 0) uniform GlobalContext {
    mat4 view;
    mat4 proj;
    Frustum f;
    mat4 world;
} gs;

struct ShadowMapViewProj
{
    mat4 view;
    mat4 proj;
};



struct ShadowMapView
{
	float xOff;
	float yOff;
	float xScale;
	float yScale;
};



layout(set = 1, binding = 3) uniform texture2D Textures[];
layout(set = 1, binding = 0) uniform sampler samplerLinear;
layout(set = 1, binding = 1) readonly buffer ShadowMap {
    ShadowMapViewProj viewProjs[];
} sm;

layout(set = 1, binding = 2) uniform shadowViews
{
    ShadowMapView views[128];
} sv;

struct LightSource
{
	vec4 color; //w is diffuse intensity;
	vec4 pos; //w is radius for point right
	vec4 direction; //w is specular intensity;
	vec4 ancillary; //for spot, x, y are cosine theta cutoffs //ambient for directional
};

layout(set = 2, binding = 3) readonly buffer GLBuffer {
    LightSource objects[];
} lightBuffer;

layout(set = 2, binding = 4) uniform usamplerBuffer globalLightTypes;

const uint TANGENTNORMALMAPPED = 2u;
const uint ALBEDOMAPPED = 4u;
const uint WORLDNORMALMAPPED = 8u;
const uint VERTEXNORMAL = 16u;

struct Material
{
	uint materialFlags;
	float shininess;
	uint unused2;
	uint unused3;
	vec4 albedoColor;
	uvec4 textureHandles; // y - normalMapCoordinates // x- albedo
    vec4 emissive;
    vec4 specular;
};

layout(set = 2, binding = 5) uniform MaterialContext {
   Material matsData[50];
} mats;

layout(set = 2, binding = 6) readonly buffer RENDBuffer {
    MeshRenderable renderables[];
} rends;

layout(set = 2, binding = 7) uniform usamplerBuffer globalMaterialIndices;

struct BlendDetails
{
	uint type;
    uint alphaMapHandleBlendFactor; //union based on type;
};

layout(set = 2, binding = 8) readonly buffer BLENDBuffer {
    BlendDetails details[];
} blends;

layout(set = 2, binding = 9) uniform usamplerBuffer globalBlendIndices;

layout(set = 2, binding = 10) readonly buffer GeomDescBuffer
{
    GeometryDetails details[];
} geomDetails;

layout(set = 2, binding = 11) readonly buffer GeomInstBuffer
{
    GeometryRenderable geomRenderables[];
} geomRends;

vec4 DoLights(vec3 normal, vec3 worldPos, vec4 color, MeshRenderable currentRenderable, vec3 specularBase, float shininess)
{
    uint lightCount = currentRenderable.lightCount;

    if (lightCount == 0) return color;

    vec4 cumColor = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 camPos = gs.world[3].xyz;

    vec3 DiffuseColor = vec3(0.0);

    vec3 SpecularColor = vec3(0.0);

    vec3 AmbientColor = vec3(0.0);

    vec3 viewPosToWorldPos = normalize(camPos - worldPos);

    float shadowColor = 0.0;

    for (uint i = 0; i<lightCount; i++)
    {
        uint currentLightIndex = currentRenderable.lightIndices[i];

        LightSource light = lightBuffer.objects[currentLightIndex];

        uint lType = uint(texelFetch(globalLightTypes, int(currentLightIndex)).r);

        vec3 lightColor = light.color.xyz/255.0;

        float diffuseIntensity = light.color.w;

        float specularIntensity = light.direction.w;

        vec3 lightDirection = light.direction.xyz;
        
        if (lType == 0)
        {
            vec3 lightingDir = normalize(-lightDirection); 

            vec3 lightReflect = normalize(reflect(lightingDir, normal));

            float diffuse = max(dot(normal, lightingDir), 0.0);

            float specFactor = max(dot(viewPosToWorldPos, lightReflect), 0.0);

            specFactor = pow(specFactor, shininess);

            DiffuseColor += lightColor * diffuse * diffuseIntensity;

            AmbientColor += light.ancillary.xyz;

            SpecularColor += lightColor * specularBase * specFactor * specularIntensity;

            uint shadowMapIndex = sic.imageIndex + sic.currentFrame;

            ShadowMapViewProj sViewProj = sm.viewProjs[currentLightIndex];
            ShadowMapView sViewRange = sv.views[currentLightIndex];

            vec4 smTransCoords = sViewProj.proj * sViewProj.view * worldPosition;

            vec3 projCoords = smTransCoords.xyz / smTransCoords.w;

            vec2 projCoordsPair = projCoords.xy * 0.5 + 0.5;

            vec2 sViewOff = vec2(sViewRange.xOff, sViewRange.yOff);
            vec2 sViewScale = vec2(sViewRange.xScale, sViewRange.yScale);

            projCoordsPair  = (sViewScale * projCoordsPair) + sViewOff;

            float closestDepth = texture(sampler2D(Textures[shadowMapIndex], samplerLinear), projCoordsPair).r;

            float currentDepth = projCoords.z;

            float bias = max(0.05 * (1.0 - dot(normal, lightingDir)), 0.005);  

            if ((currentDepth-bias) >= closestDepth)
            {
                shadowColor = 1.0;
            } 
            else if ((projCoords.z > 1.0))
            {
                shadowColor = 0.0;
            }
        }
        else if (lType == 1)
        {
            vec3 lightDir = normalize(light.pos.xyz - worldPos);

            float diffuse = max(dot(normal.xyz, lightDir), 0.0);

            float distance = length(light.pos.xyz - worldPos);

            float attenuation = (1.0/(.01 + (.25 * distance) + (.67 * distance * distance)));

            vec3 lightReflect = normalize(reflect(lightDir, normal));

            float specFactor = max(dot(viewPosToWorldPos, lightReflect), 0.0);

            specFactor = pow(specFactor, shininess);

            DiffuseColor += lightColor * diffuse * diffuseIntensity * attenuation;

            AmbientColor += light.ancillary.xyz * attenuation;

            SpecularColor += lightColor * specularBase * specFactor * specularIntensity;

        }
        else if (lType == 2)
        {
            vec3 lightLook =  light.pos.xyz - worldPos;

            float distance = length(lightLook);

            lightLook /= distance;

            vec3 lightingDir = normalize(-lightDirection);

            float diffuse = max(dot(normal.xyz, lightingDir), 0.0);

            float cosTheta = dot(lightLook, lightingDir);

            float spotFactor = smoothstep(cos(light.ancillary.y), cos(light.ancillary.x), cosTheta);

            vec3 lightReflect = normalize(reflect(lightingDir, normal));

            float specFactor = max(dot(viewPosToWorldPos, lightReflect), 0.0);

            specFactor = pow(specFactor, shininess);

            DiffuseColor += lightColor * diffuse * diffuseIntensity * spotFactor * (1.0/(distance*distance));

            SpecularColor += lightColor * specularBase * specFactor * specularIntensity * spotFactor * (1.0/(distance*distance));
        }
    }

    return vec4(color.xyz *   (AmbientColor + (1.0 - shadowColor) * (DiffuseColor + SpecularColor)), color.w);
}

struct PixelColorData
{
    vec3 diffuse;
    vec3 specular;
    vec3 normal;
    vec3 emissive;
    float shininess;
};

PixelColorData ZeroColorData()
{
    PixelColorData data;

    data.diffuse = vec3(0.0);
    data.specular = vec3(0.0);
    data.normal = vec3(0.0);
    data.emissive = vec3(0.0);
    data.shininess = 0.0; 

    return data;
}


const uint ConstantAlpha = 1;
const uint BlendMap = 2;

float GetPixelBlendWeight(BlendDetails details, uint textureIndex)
{
    float ret = 1.0; 

    switch(details.type)
    {
        case ConstantAlpha:
            ret = uintBitsToFloat(details.alphaMapHandleBlendFactor);
            break;
        case BlendMap:
            ret = texture(sampler2D(Textures[details.alphaMapHandleBlendFactor], samplerLinear), texCoords[0]).r;
            break;
        default:
            break;
    }

    return ret;
}

mat3 AdjointMatrix(mat4 worldTransform)
{
    mat3 scoped = mat3(worldTransform);
    return transpose(determinant(scoped) * inverse(scoped));
}

void main() {

    MeshRenderable currentRenderable = rends.renderables[renderableIndex];

    GeometryRenderable lGeomRenderable = geomRends.geomRenderables[currentRenderable.geomInstIndex];

    uint materialStart = currentRenderable.materialStart;

    uint blendStart = currentRenderable.blendLayersStart;

    uint textureIndex = 0;

    PixelColorData accumulatedColorData = ZeroColorData();

    bool lightAffected = false;

    mat3 normalMatrix = AdjointMatrix(lGeomRenderable.transform * currentRenderable.transform);

    for (uint i = 0; i<currentRenderable.materialCount; i++)
    {

        uint materialIndex = uint(texelFetch(globalMaterialIndices, int(materialStart + i)).r);

        uint blendIndex = uint(texelFetch(globalBlendIndices, int(blendStart + i)).r);

        Material mat = mats.matsData[materialIndex];

        BlendDetails details = blends.details[blendIndex];

        PixelColorData colorData = ZeroColorData();

        float w =  GetPixelBlendWeight(details, textureIndex);

        colorData.diffuse = mat.albedoColor.xyz;
        colorData.specular = mat.specular.xyz;
        colorData.emissive = mat.emissive.xyz;
        colorData.shininess = mat.shininess;
        
        if ((mat.materialFlags & ALBEDOMAPPED) == ALBEDOMAPPED)
        {
           colorData.diffuse *= texture(sampler2D(Textures[mat.textureHandles.x], samplerLinear), texCoords[textureIndex++]).rgb;
        }
    
        if ((mat.materialFlags & WORLDNORMALMAPPED) == WORLDNORMALMAPPED)
        {
            vec3 normal = texture(sampler2D(Textures[mat.textureHandles.y], samplerLinear), texCoords[textureIndex++]).rgb;

            colorData.normal = normalMatrix * (2.0 * normal.xyz - 1.0);

            lightAffected = true;
        } 
        else if ((mat.materialFlags & VERTEXNORMAL) == VERTEXNORMAL)
        {
            colorData.normal = inNormal;

            lightAffected = true;
        }
        else if ((mat.materialFlags & TANGENTNORMALMAPPED) == TANGENTNORMALMAPPED)
        {
            vec3 N = normalize(inNormal);
            vec3 T = normalize(inTangent.xyz);

            T = normalize(T - N * dot(N, T));
            vec3 B = inTangent.w * cross(N, T);

            mat3 TBN = mat3(T, B, N);

            vec3 normal = texture(sampler2D(Textures[mat.textureHandles.y], samplerLinear), texCoords[textureIndex++]).rgb;

            colorData.normal = TBN * (2.0 * normal.xyz - 1.0);

            lightAffected = true;
        }

        accumulatedColorData.diffuse += (colorData.diffuse * w);
        accumulatedColorData.normal += (colorData.normal * w);
        accumulatedColorData.specular += (colorData.specular * w);
        accumulatedColorData.emissive += (colorData.emissive * w);
        accumulatedColorData.shininess += (colorData.shininess * w);
    }

    vec4 albedoColor = vec4(accumulatedColorData.diffuse, 1.0);

    if (lightAffected)
    {
        albedoColor = DoLights(normalize(accumulatedColorData.normal), worldPosition.xyz, albedoColor, currentRenderable, accumulatedColorData.specular, accumulatedColorData.shininess);
    }

    outColor = vertColor * (albedoColor + vec4(accumulatedColorData.emissive, 0.0));
}