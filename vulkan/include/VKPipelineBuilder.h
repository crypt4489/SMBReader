#pragma once
#include "IndexTypes.h"
#include "VKUtilities.h"
#include "VKTypes.h"
#include <cstdint>

struct PipelineCacheObject
{
	EntryHandle* descLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

struct VKPipelineBuilder
{
	VKDevice* majorDev;
	PipelineCacheObject co;
	VkPushConstantRange* ranges;
	uint32_t pushConstantsCount;
	uint32_t pad;

	VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout* descriptorSetLayout, uint32_t count);

	VkPipelineShaderStageCreateInfo AddShader(VkShaderModule& mod, VkShaderStageFlags flags);

	void AddPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags stage, uint32_t rangeLocation);
};

struct VKGraphicsPipelineBuilder : public VKPipelineBuilder
{
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

	void CreateRasterizer(VkCullModeFlags cullFlags, VkFrontFace frontFace, float lineWidth);

	void CreateMultiSampling(VkSampleCountFlagBits count);

	void CreateColorBlendAttachment(uint32_t attachmentNumber, VkColorComponentFlags flags, 
		VkBool32 blendOpEnable, VkBlendFactor srcColor, 
		VkBlendFactor dstColor, VkBlendFactor srcAlpha, 
		VkBlendFactor dstAlpha, 
		VkBlendOp colorOp, VkBlendOp alphaOp);

	void CreateColorBlending(VkBool32 logicOpEnable, VkLogicOp logicOp);

	void CreateDepthStencil(VkCompareOp depthOp, bool depthenable, bool depthwriteenable, bool stencilEnable, VkStencilOpState* front, VkStencilOpState* back);

	VkRenderPass renderPass;
	VkDynamicState* dynamicStates;
	VkPipelineColorBlendAttachmentState* colorBlendAttachments;
	VkPipelineDynamicStateCreateInfo dynamicStateInfo;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkGraphicsPipelineCreateInfo pipelineInfo;
	uint32_t dynamicStateCount;
	uint32_t colorBlendAttachmentsCount;
	
};

struct VKComputePipelineBuilder : public VKPipelineBuilder
{
	VKComputePipelineBuilder(VKDevice* d, size_t descriptorCount, uint32_t pushConstantCount);
	VkComputePipelineCreateInfo pipelineInfo;

	EntryHandle CreateComputePipeline(EntryHandle* descriptorlaysids,
		size_t descriptorSetCount,
		EntryHandle shaderHandles);

};