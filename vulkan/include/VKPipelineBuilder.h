#pragma once
#include "IndexTypes.h"
#include "VKUtilities.h"
#include "VKTypes.h"
#include <optional>
#include <unordered_map>
#include <vector>



struct PipelineCacheObject
{
	EntryHandle* descLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

class VKPipelineBuilder
{
public:
	VKDevice* majorDev;
	PipelineCacheObject co;
	uint32_t pushConstantsCount;
	VkPushConstantRange* ranges;

	VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout* descriptorSetLayout, uint32_t count);

	VkPipelineShaderStageCreateInfo AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags);

	void AddPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags stage, uint32_t rangeLocation);
};

class VKGraphicsPipelineBuilder : public VKPipelineBuilder
{
public:
	VKGraphicsPipelineBuilder() = delete;
	VKGraphicsPipelineBuilder(VkRenderPass _rp, VKDevice *d, uint32_t colorBlendImageCount, uint32_t descriptorCount, uint32_t _dynamicStateCount, uint32_t pushConstantCount);


	EntryHandle CreateGraphicsPipeline(EntryHandle* descriptorlaysids,
		size_t descriptorSetCount,
		EntryHandle* shaderHandles,
		size_t shaderCount);

	void CreateDynamicStateInfo(VkDynamicState* states, uint32_t count);

	void CreateVertexInput(VkVertexInputBindingDescription* bindDescription,
		uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertAttributes,
		uint32_t vertAttributecount);

	void CreateInputAssembly(VkPrimitiveTopology topo, bool primitiveRestart);

	void CreateViewportState(uint32_t viewportCount, uint32_t scissorCount);

	void CreateRasterizer(VkCullModeFlags cullFlags, VkFrontFace frontFace);

	void CreateMultiSampling(VkSampleCountFlagBits count);

	void CreateColorBlendAttachment(uint32_t attachmentNumber, VkColorComponentFlags flags);

	void CreateColorBlending(VkLogicOp blendOp);

	void CreateDepthStencil(VkCompareOp depthOp);

	VkRenderPass renderPass;
	
	uint32_t colorBlendAttachmentsCount;

	VkDynamicState* dynamicStates;
	uint32_t dynamicStateCount;

	VkPipelineDynamicStateCreateInfo dynamicStateInfo;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineColorBlendAttachmentState* colorBlendAttachments;
	
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	
};

class VKComputePipelineBuilder : public VKPipelineBuilder
{
public:
	VKComputePipelineBuilder(VKDevice* d, size_t descriptorCount, uint32_t pushConstantCount);
	VkComputePipelineCreateInfo pipelineInfo;

	EntryHandle CreateComputePipeline(EntryHandle* descriptorlaysids,
		size_t descriptorSetCount,
		EntryHandle shaderHandles);

};