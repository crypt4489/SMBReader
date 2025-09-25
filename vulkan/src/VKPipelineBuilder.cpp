#include "pch.h"

#include "VKPipelineBuilder.h"
#include "VKDevice.h"
#include <array>
VKPipelineBuilder::VKPipelineBuilder(VkRenderPass _rp, VKDevice *d, uint32_t colorsBlendAttchCount, uint32_t descriptorCount, uint32_t _dynamicStateCount) 
{
	memset(this, 0, sizeof(VKPipelineBuilder));
	renderPass = _rp; 
	majorDev = d;
	colorBlendAttachmentsCount = colorsBlendAttchCount;
	dynamicStateCount = _dynamicStateCount;
		
	colorBlendAttachments = reinterpret_cast<VkPipelineColorBlendAttachmentState*>(d->AllocFromDeviceCache(sizeof(VkPipelineColorBlendAttachmentState) * colorsBlendAttchCount));
	co.descLayout = reinterpret_cast<EntryHandle*>(d->AllocFromPerDeviceData(sizeof(EntryHandle) * descriptorCount));
	dynamicStates = reinterpret_cast<VkDynamicState*>(d->AllocFromDeviceCache(sizeof(VkDynamicState) * _dynamicStateCount));
}

void VKPipelineBuilder::CreateDynamicStateInfo(VkDynamicState* states, uint32_t count)
{
	for (uint32_t i = 0; i < count; i++)
	{
		dynamicStates[i] = states[i];
	}

	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = dynamicStateCount;
	dynamicStateInfo.pDynamicStates = dynamicStates;

}

void VKPipelineBuilder::CreateVertexInput(VkVertexInputBindingDescription* bindDescription,
		uint32_t bindingCount,
		VkVertexInputAttributeDescription* vertAttributes,
		uint32_t vertAttributecount)
{


	VkVertexInputBindingDescription* innerBind = reinterpret_cast<VkVertexInputBindingDescription*>(majorDev->AllocFromDeviceCache(sizeof(VkVertexInputBindingDescription) * bindingCount));
	VkVertexInputAttributeDescription* innerVertAttributes = reinterpret_cast<VkVertexInputAttributeDescription*>(majorDev->AllocFromDeviceCache(sizeof(VkVertexInputAttributeDescription) * vertAttributecount));


	for (uint32_t i = 0; i < bindingCount; i++)
		innerBind[i] = bindDescription[i];

	for (uint32_t i = 0; i < vertAttributecount; i++)
		innerVertAttributes[i] = vertAttributes[i];

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	vertexInputInfo.vertexBindingDescriptionCount = bindingCount;
	vertexInputInfo.pVertexBindingDescriptions = innerBind;

	vertexInputInfo.vertexAttributeDescriptionCount = vertAttributecount;
	vertexInputInfo.pVertexAttributeDescriptions = innerVertAttributes;
}

void VKPipelineBuilder::CreateInputAssembly(VkPrimitiveTopology topo, bool primitiveRestart)
{
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topo;
	inputAssembly.primitiveRestartEnable = primitiveRestart;
}

void VKPipelineBuilder::CreateViewportState(uint32_t viewportCount, uint32_t scissorCount)
{
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = viewportCount;
	viewportState.scissorCount = scissorCount;
}
void VKPipelineBuilder::CreateRasterizer(VkCullModeFlags cullFlags, VkFrontFace frontFace)
{
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = cullFlags;
	rasterizer.frontFace = frontFace;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;
}
void VKPipelineBuilder::CreateMultiSampling(VkSampleCountFlagBits count)
{
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = count;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;
}
void VKPipelineBuilder::CreateColorBlendAttachment(uint32_t attachmentNumber, VkColorComponentFlags flags)
{
	auto colorBlendAttachment = &colorBlendAttachments[attachmentNumber];
	colorBlendAttachment->colorWriteMask = flags;
	colorBlendAttachment->blendEnable = VK_FALSE;
	colorBlendAttachment->srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment->colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment->alphaBlendOp = VK_BLEND_OP_ADD;

}
void VKPipelineBuilder::CreateColorBlending(VkLogicOp blendOp)
{
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = colorBlendAttachmentsCount;
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;
}
void VKPipelineBuilder::CreateDepthStencil(VkCompareOp depthOp)

{
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;

	depthStencil.depthCompareOp = depthOp;

	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional
}


VkPipelineLayout VKPipelineBuilder::CreatePipelineLayout(VkDescriptorSetLayout *descriptorSetLayout, uint32_t count)
{
	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	
	pipelineLayoutInfo.setLayoutCount = count;
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayout;
	
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(majorDev->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	return pipelineLayout;
}

EntryHandle VKPipelineBuilder::CreateGraphicsPipeline(EntryHandle* descriptorlaysids,
	size_t descriptorSetCount,
	EntryHandle* shaderHandles,
	size_t shaderCount)
{
	VkPipeline graphicsPipeline;

	VkDescriptorSetLayout* layouts = reinterpret_cast<VkDescriptorSetLayout*>(majorDev->AllocFromDeviceCache(sizeof(VkDescriptorSetLayout) * descriptorSetCount));

	for (std::size_t i = 0; i < descriptorSetCount; i++)
	{
		co.descLayout[i] = descriptorlaysids[i];
		layouts[i] = majorDev->GetDescriptorSetLayout(descriptorlaysids[i]);
	}

	VkPipelineLayout pipelineLayout = CreatePipelineLayout(layouts, descriptorSetCount);


	std::pair<VkShaderModule, VkShaderStageFlagBits>* shaders = reinterpret_cast<std::pair<VkShaderModule, VkShaderStageFlagBits>*>(
		majorDev->AllocFromDeviceCache(sizeof(std::pair<VkShaderModule, VkShaderStageFlagBits>) * shaderCount));

	for (std::size_t i = 0; i < shaderCount; i++)
	{
		shaders[i] = majorDev->GetShader(shaderHandles[i]);
	}

	VkPipelineShaderStageCreateInfo* shaderInfos = reinterpret_cast<VkPipelineShaderStageCreateInfo*>(majorDev->AllocFromDeviceCache(sizeof(VkPipelineShaderStageCreateInfo) * shaderCount));

	for (int i = 0; i < shaderCount; i++)
	{
		shaderInfos[i] = AddShader(shaders[i].first, shaders[i].second);
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderCount);
	pipelineInfo.pStages = shaderInfos;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(majorDev->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}


	co.pipeline = graphicsPipeline;
	co.pipelineLayout = pipelineLayout;

	EntryHandle ret = majorDev->CreatePipelineCacheObject(&co);

	return ret;

}

VkPipelineShaderStageCreateInfo VKPipelineBuilder::AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags)
{
	VkPipelineShaderStageCreateInfo shaderStageInfo{};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = flags;
	shaderStageInfo.module = mod;
	shaderStageInfo.pName = "main";
	return shaderStageInfo;
}