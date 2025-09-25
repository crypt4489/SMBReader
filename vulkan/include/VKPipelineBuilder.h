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
	VKPipelineBuilder() = default;
	VKPipelineBuilder(VkRenderPass _rp, VKDevice *d, uint32_t colorBlendImageCount, uint32_t descriptorCount, uint32_t _dynamicStateCount);


	VkPipelineLayout CreatePipelineLayout(VkDescriptorSetLayout* descriptorSetLayout, uint32_t count);

	EntryHandle CreateGraphicsPipeline(EntryHandle* descriptorlaysids,
		size_t descriptorSetCount,
		EntryHandle* shaderHandles,
		size_t shaderCount);

	VkPipelineShaderStageCreateInfo AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags);


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
	VKDevice *majorDev;
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
	PipelineCacheObject co;
};

