#include "pch.h"

#include "GlslangCompiler.h"

#include <glslang/Include/ResourceLimits.h>
#include "glslang/Public/resource_limits_c.h"

namespace GLSLANG
{
    static bool notProcessInitialized = true;

    static TBuiltInResource gpuResource{};

    void resource()
    {
        gpuResource.maxLights = 32;
        gpuResource.maxClipPlanes = 6;
        gpuResource.maxTextureUnits = 32;
        gpuResource.maxTextureCoords = 32;
        gpuResource.maxVertexAttribs = 64;
        gpuResource.maxVertexUniformComponents = 4096;
        gpuResource.maxVaryingFloats = 64;
        gpuResource.maxVertexTextureImageUnits = 32;
        gpuResource.maxCombinedTextureImageUnits = 80;
        gpuResource.maxTextureImageUnits = 32;
        gpuResource.maxFragmentUniformComponents = 4096;
        gpuResource.maxDrawBuffers = 32;
        gpuResource.maxVertexUniformVectors = 128;
        gpuResource.maxVaryingVectors = 8;
        gpuResource.maxFragmentUniformVectors = 16;
        gpuResource.maxVertexOutputVectors = 16;
        gpuResource.maxFragmentInputVectors = 15;
        gpuResource.minProgramTexelOffset = -8;
        gpuResource.maxProgramTexelOffset = 7;
        gpuResource.maxClipDistances = 8;
        gpuResource.maxComputeWorkGroupCountX = 65535;
        gpuResource.maxComputeWorkGroupCountY = 65535;
        gpuResource.maxComputeWorkGroupCountZ = 65535;
        gpuResource.maxComputeWorkGroupSizeX = 1024;
        gpuResource.maxComputeWorkGroupSizeY = 1024;
        gpuResource.maxComputeWorkGroupSizeZ = 64;
        gpuResource.maxComputeUniformComponents = 1024;
        gpuResource.maxComputeTextureImageUnits = 16;
        gpuResource.maxComputeImageUniforms = 8;
        gpuResource.maxComputeAtomicCounters = 8;
        gpuResource.maxComputeAtomicCounterBuffers = 1;
        gpuResource.maxVaryingComponents = 60;
        gpuResource.maxVertexOutputComponents = 64;
        gpuResource.maxGeometryInputComponents = 64;
        gpuResource.maxGeometryOutputComponents = 128;
        gpuResource.maxFragmentInputComponents = 128;
        gpuResource.maxImageUnits = 8;
        gpuResource.maxCombinedImageUnitsAndFragmentOutputs = 8;
        gpuResource.maxCombinedShaderOutputResources = 8;
        gpuResource.maxImageSamples = 0;
        gpuResource.maxVertexImageUniforms = 0;
        gpuResource.maxTessControlImageUniforms = 0;
        gpuResource.maxTessEvaluationImageUniforms = 0;
        gpuResource.maxGeometryImageUniforms = 0;
        gpuResource.maxFragmentImageUniforms = 8;
        gpuResource.maxCombinedImageUniforms = 8;
        gpuResource.maxGeometryTextureImageUnits = 16;
        gpuResource.maxGeometryOutputVertices = 256;
        gpuResource.maxGeometryTotalOutputComponents = 1024;
        gpuResource.maxGeometryUniformComponents = 1024;
        gpuResource.maxGeometryVaryingComponents = 64;
        gpuResource.maxTessControlInputComponents = 128;
        gpuResource.maxTessControlOutputComponents = 128;
        gpuResource.maxTessControlTextureImageUnits = 16;
        gpuResource.maxTessControlUniformComponents = 1024;
        gpuResource.maxTessControlTotalOutputComponents = 4096;
        gpuResource.maxTessEvaluationInputComponents = 128;
        gpuResource.maxTessEvaluationOutputComponents = 128;
        gpuResource.maxTessEvaluationTextureImageUnits = 16;
        gpuResource.maxTessEvaluationUniformComponents = 1024;
        gpuResource.maxTessPatchComponents = 120;
        gpuResource.maxPatchVertices = 32;
        gpuResource.maxTessGenLevel = 64;
        gpuResource.maxViewports = 16;
        gpuResource.maxVertexAtomicCounters = 0;
        gpuResource.maxTessControlAtomicCounters = 0;
        gpuResource.maxTessEvaluationAtomicCounters = 0;
        gpuResource.maxGeometryAtomicCounters = 0;
        gpuResource.maxFragmentAtomicCounters = 8;
        gpuResource.maxCombinedAtomicCounters = 8;
        gpuResource.maxAtomicCounterBindings = 1;
        gpuResource.maxVertexAtomicCounterBuffers = 0;
        gpuResource.maxTessControlAtomicCounterBuffers = 0;
        gpuResource.maxTessEvaluationAtomicCounterBuffers = 0;
        gpuResource.maxGeometryAtomicCounterBuffers = 0;
        gpuResource.maxFragmentAtomicCounterBuffers = 1;
        gpuResource.maxCombinedAtomicCounterBuffers = 1;
        gpuResource.maxAtomicCounterBufferSize = 16384;
        gpuResource.maxTransformFeedbackBuffers = 4;
        gpuResource.maxTransformFeedbackInterleavedComponents = 64;
        gpuResource.maxCullDistances = 8;
        gpuResource.maxCombinedClipAndCullDistances = 8;
        gpuResource.maxSamples = 4;
        gpuResource.maxMeshOutputVerticesNV = 256;
        gpuResource.maxMeshOutputPrimitivesNV = 512;
        gpuResource.maxMeshWorkGroupSizeX_NV = 32;
        gpuResource.maxMeshWorkGroupSizeY_NV = 1;
        gpuResource.maxMeshWorkGroupSizeZ_NV = 1;
        gpuResource.maxTaskWorkGroupSizeX_NV = 32;
        gpuResource.maxTaskWorkGroupSizeY_NV = 1;
        gpuResource.maxTaskWorkGroupSizeZ_NV = 1;
        gpuResource.maxMeshViewCountNV = 4;

        gpuResource.limits.nonInductiveForLoops = 1;
        gpuResource.limits.whileLoops = 1;
        gpuResource.limits.doWhileLoops = 1;
        gpuResource.limits.generalUniformIndexing = 1;
        gpuResource.limits.generalAttributeMatrixVectorIndexing = 1;
        gpuResource.limits.generalVaryingIndexing = 1;
        gpuResource.limits.generalSamplerIndexing = 1;
        gpuResource.limits.generalVariableIndexing = 1;
        gpuResource.limits.generalConstantMatrixVectorIndexing = 1;

    }

    void DeleteShader(GLSLShaderProgram* shaderProgram)
    {
#if defined(USE_SHADERC_COMBINED)  
        glslang_program_delete(shaderProgram->programData);

        glslang_shader_delete(shaderProgram->shaderInfoData);
#endif
    }

    int CompileShader(VkDevice& device, char* data, VkShaderStageFlags stage, GLSLShaderProgram* shaderProgram)
    {
#if defined(USE_SHADERC_COMBINED)  
        glslang_stage_t glslstage = GLSLANG_STAGE_COUNT;

        switch (stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT:
            glslstage = GLSLANG_STAGE_VERTEX;
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            glslstage = GLSLANG_STAGE_FRAGMENT;
            break;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            glslstage = GLSLANG_STAGE_COMPUTE;
            break;
        default:
            return -1;
        }

        if (notProcessInitialized)
        {
            notProcessInitialized = false;
            glslang_initialize_process();
            resource();
        }

        const glslang_input_t input =
        {
            .language = GLSLANG_SOURCE_GLSL,
            .stage = glslstage,
            .client = GLSLANG_CLIENT_VULKAN,
            .client_version = GLSLANG_TARGET_VULKAN_1_3,
            .target_language = GLSLANG_TARGET_SPV,
            .target_language_version = GLSLANG_TARGET_SPV_1_6,
            .code = data,
            .default_version = 100,
            .default_profile = GLSLANG_NO_PROFILE,
            .force_default_version_and_profile = false,
            .forward_compatible = false,
            .messages = GLSLANG_MSG_DEFAULT_BIT,
            .resource = reinterpret_cast<glslang_resource_t*>(&gpuResource)
        };

        glslang_shader_t* shader = glslang_shader_create(&input);

        if (!(shaderProgram->retCode = glslang_shader_preprocess(shader, &input)))
        {
            glslang_shader_delete(shader);

            return -1;
        }

        if (!(shaderProgram->retCode = glslang_shader_parse(shader, &input)))
        {
            glslang_shader_delete(shader);

            return -2;
        }

        glslang_program_t* program = glslang_program_create();
        glslang_program_add_shader(program, shader);

        if (!(shaderProgram->retCode = glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)))
        {
            glslang_program_delete(program);

            glslang_shader_delete(shader);

            return -3;
        }

        glslang_program_SPIRV_generate(program, input.stage);

        if ((shaderProgram->glslangMessages = glslang_program_SPIRV_get_messages(program)))
        {
            return -4;
        }

        shaderProgram->shaderRawData = reinterpret_cast<char*>(glslang_program_SPIRV_get_ptr(program));

        shaderProgram->shaderRawDataSize = glslang_program_SPIRV_get_size(program) * sizeof(unsigned int);

        shaderProgram->shaderInfoData = shader;

        shaderProgram->programData = program;

        return 0;
#else
        return -1;
#endif
    }
}
