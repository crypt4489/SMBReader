#include "pch.h"

#include "VKPipelineBuilder.h"
#include "VKDevice.h"
#include <array>
VKGraphicsPipelineBuilder::VKGraphicsPipelineBuilder(VkRenderPass _rp, VKDevice *d, uint32_t colorsBlendAttchCount, uint32_t descriptorCount, uint32_t _dynamicStateCount, uint32_t pushConstantCount)
{
	renderPass = _rp; 
	majorDev = d;
	colorBlendAttachmentsCount = colorsBlendAttchCount;
	dynamicStateCount = _dynamicStateCount;
		
	colorBlendAttachments = reinterpret_cast<VkPipelineColorBlendAttachmentState*>(d->AllocFromDeviceCache(sizeof(VkPipelineColorBlendAttachmentState) * colorsBlendAttchCount));
	co.descLayout = reinterpret_cast<EntryHandle*>(d->AllocFromPerDeviceData(sizeof(EntryHandle) * descriptorCount));
	dynamicStates = reinterpret_cast<VkDynamicState*>(d->AllocFromDeviceCache(sizeof(VkDynamicState) * _dynamicStateCount));
	pushConstantsCount = pushConstantCount;
	ranges = reinterpret_cast<VkPushConstantRange*>(d->AllocFromPerDeviceData(sizeof(VkPushConstantRange) * pushConstantCount));
}

void VKGraphicsPipelineBuilder::CreateDynamicStateInfo(VkDynamicState* states, uint32_t count)
{
	for (uint32_t i = 0; i < count; i++)
	{
		dynamicStates[i] = states[i];
	}

	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = dynamicStateCount;
	dynamicStateInfo.pDynamicStates = dynamicStates;

}

void VKGraphicsPipelineBuilder::CreateVertexInput(VkVertexInputBindingDescription* bindDescription,
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

void VKGraphicsPipelineBuilder::CreateInputAssembly(VkPrimitiveTopology topo, bool primitiveRestart)
{
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topo;
	inputAssembly.primitiveRestartEnable = primitiveRestart;
}

void VKGraphicsPipelineBuilder::CreateViewportState(uint32_t viewportCount, uint32_t scissorCount)
{
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = viewportCount;
	viewportState.scissorCount = scissorCount;
}
void VKGraphicsPipelineBuilder::CreateRasterizer(VkCullModeFlags cullFlags, VkFrontFace frontFace, float lineWidth)
{
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = lineWidth;
	rasterizer.cullMode = cullFlags;
	rasterizer.frontFace = frontFace;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;
}
void VKGraphicsPipelineBuilder::CreateMultiSampling(VkSampleCountFlagBits count)
{
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = count;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;
}
void VKGraphicsPipelineBuilder::CreateColorBlendAttachment(uint32_t attachmentNumber, VkColorComponentFlags flags)
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
void VKGraphicsPipelineBuilder::CreateColorBlending(VkLogicOp blendOp)
{
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = blendOp;
	colorBlending.attachmentCount = colorBlendAttachmentsCount;
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;
}
void VKGraphicsPipelineBuilder::CreateDepthStencil(VkCompareOp depthOp, bool depthwriteenable)

{
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable =  VK_TRUE;
	depthStencil.depthWriteEnable = depthwriteenable ? VK_TRUE : VK_FALSE;

	depthStencil.depthCompareOp = depthOp;

	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional
}

EntryHandle VKGraphicsPipelineBuilder::CreateGraphicsPipeline(EntryHandle* descriptorlaysids,
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


	ShaderHandle** shaders = reinterpret_cast<ShaderHandle**>(
		majorDev->AllocFromDeviceCache(sizeof(ShaderHandle*) * shaderCount));

	for (std::size_t i = 0; i < shaderCount; i++)
	{
		shaders[i] = majorDev->GetShader(shaderHandles[i]);
	}

	VkPipelineShaderStageCreateInfo* shaderInfos = reinterpret_cast<VkPipelineShaderStageCreateInfo*>(majorDev->AllocFromDeviceCache(sizeof(VkPipelineShaderStageCreateInfo) * shaderCount));

	for (int i = 0; i < shaderCount; i++)
	{
		shaderInfos[i] = AddShader(shaders[i]->sMod, shaders[i]->flags);
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
VKComputePipelineBuilder::VKComputePipelineBuilder(VKDevice* d, size_t descriptorCount, uint32_t pushConstantCount) 
{
	majorDev = d;
	co.descLayout = reinterpret_cast<EntryHandle*>(d->AllocFromPerDeviceData(sizeof(EntryHandle) * descriptorCount));
	pushConstantsCount = pushConstantCount;
	ranges = reinterpret_cast<VkPushConstantRange*>(d->AllocFromPerDeviceData(sizeof(VkPushConstantRange) * pushConstantCount));
}

EntryHandle VKComputePipelineBuilder::CreateComputePipeline(EntryHandle* descriptorlaysids,
	size_t descriptorSetCount,
	EntryHandle shaderHandles
)
{
	VkPipeline computePipeline;

	VkDescriptorSetLayout* layouts = reinterpret_cast<VkDescriptorSetLayout*>(majorDev->AllocFromDeviceCache(sizeof(VkDescriptorSetLayout) * descriptorSetCount));


	for (std::size_t i = 0; i < descriptorSetCount; i++)
	{
		co.descLayout[i] = descriptorlaysids[i];
		layouts[i] = majorDev->GetDescriptorSetLayout(descriptorlaysids[i]);
	}

	VkPipelineLayout pipelineLayout = CreatePipelineLayout(layouts, descriptorSetCount);
	
	ShaderHandle* shader = majorDev->GetShader(shaderHandles);

	
	pipelineInfo.stage = AddShader(shader->sMod, shader->flags);
	
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = pipelineLayout;


	if (vkCreateComputePipelines(majorDev->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create compute pipeline!");
	}

	co.pipeline = computePipeline;
	co.pipelineLayout = pipelineLayout;

	EntryHandle ret = majorDev->CreatePipelineCacheObject(&co);

	return ret;
}


VkPipelineLayout VKPipelineBuilder::CreatePipelineLayout(VkDescriptorSetLayout* descriptorSetLayout, uint32_t count) {
	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;


	pipelineLayoutInfo.setLayoutCount = count;
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayout;

	pipelineLayoutInfo.pushConstantRangeCount = pushConstantsCount;
	pipelineLayoutInfo.pPushConstantRanges = ranges;

	if (vkCreatePipelineLayout(majorDev->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	return pipelineLayout;
}

VkPipelineShaderStageCreateInfo VKPipelineBuilder::AddShader(VkShaderModule& mod, VkShaderStageFlags flags) {
	VkPipelineShaderStageCreateInfo shaderStageInfo{};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = (VkShaderStageFlagBits)flags;
	shaderStageInfo.module = mod;
	shaderStageInfo.pName = "main";
	return shaderStageInfo;
}

void VKPipelineBuilder::AddPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags stage, uint32_t rangeLocation)
{
	VkPushConstantRange* range = &ranges[rangeLocation];
	range->offset = offset;
	range->size = size;
	range->stageFlags = stage;

}