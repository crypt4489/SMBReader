#pragma once
#include "VKUtilities.h"
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
	VKPipelineCache(VkDevice _d, VkRenderPass _rp);

	PipelineCacheObject GetPipelineFromCache(const std::string& name);

	PipelineCacheObject CreatePipeline(std::vector<VkDescriptorSetLayout>& descriptorSetLayout,
		std::optional<VkVertexInputBindingDescription> bindDescription,
		std::optional<std::vector<VkVertexInputAttributeDescription>> vertAttributes,
		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>>& shaders,
		VkCompareOp depthOp, VkSampleCountFlagBits sampleCount,
		std::string name);

	VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout>& descriptorSetLayout);

	PipelineCacheObject CreateGraphicsPipeline(std::vector<VkDescriptorSetLayout>& descriptorSetLayout,
		std::optional<VkVertexInputBindingDescription> bindDescription,
		std::optional<std::vector<VkVertexInputAttributeDescription>> vertAttributes,
		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>>& shaders,
		VkCompareOp depthOp, VkSampleCountFlagBits sampleCount);

	VkPipelineShaderStageCreateInfo AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags);

	void DestroyPipelineCache();

	std::unordered_map<std::string, PipelineCacheObject> pipelines;
	VkRenderPass renderPass;
	VkDevice device;
};

