#pragma once
#include "IndexTypes.h"
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
	VKPipelineCache(VkRenderPass _rp, VKDevice *d);

	PipelineCacheObject* operator[](EntryHandle name);

	PipelineCacheObject* GetPipelineFromCache(EntryHandle name);

	EntryHandle CreatePipeline(
		EntryHandle* descriptorlaysids,
		size_t descriptorSetCount,
		VkVertexInputBindingDescription* bindDescription,
		uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertAttributes,
		size_t vertAttributecount,
		EntryHandle* shaderHandles,
		size_t shaderCount,
		VkCompareOp depthOp, VkSampleCountFlagBits sampleCount);

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

	VkRenderPass renderPass;
	VKDevice *majorDev;
};

