#pragma once
#include "VKUtilities.h"
#include "VKTypes.h"
#include "VertexTypes.h"
#include <optional>
#include <unordered_map>
#include <vector>



struct PipelineCacheObject
{
	VkDescriptorSetLayout descLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

class VKPipelineCache
{
public:
	VKPipelineCache() = default;
	VKPipelineCache(VkDevice _d, VkRenderPass _rp, VKDevice *d);

	PipelineCacheObject* operator[](std::string name);

	PipelineCacheObject* GetPipelineFromCache(const std::string& name);

	PipelineCacheObject CreatePipeline(
		std::string* descriptorSetLayoutNames,
		size_t descriptorSetCount,
		VkVertexInputBindingDescription* bindDescription,
		uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertAttributes,
		size_t vertAttributecount,
		std::string* shaderNames,
		size_t shaderCount,
		VkCompareOp depthOp, VkSampleCountFlagBits sampleCount,
		std::string name);

	VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout* descriptorSetLayout, uint32_t count);

	PipelineCacheObject CreateGraphicsPipeline(VkDescriptorSetLayout* descriptorSetLayouts,
		size_t descriptorSetCount,
		VkVertexInputBindingDescription* bindDescription,
		uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertAttributes,
		size_t vertAttributecount,
		std::pair<VkShaderModule, VkShaderStageFlagBits>* shaders,
		size_t shaderCount,
		VkCompareOp depthOp, VkSampleCountFlagBits sampleCount);

	VkPipelineShaderStageCreateInfo AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags);

	void DestroyPipelineCache();

	std::unordered_map<std::string, PipelineCacheObject> pipelines;
	VkRenderPass renderPass;
	VkDevice device;
	VKDevice *majorDev;
};

