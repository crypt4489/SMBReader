#include "VKPipelineCache.h"
#include "VKDevice.h"
VKPipelineCache::VKPipelineCache(VkDevice _d, VkRenderPass _rp, VKDevice *d) : device(_d), renderPass(_rp), majorDev(d)
{

}

PipelineCacheObject* VKPipelineCache::GetPipelineFromCache(const std::string& name)
{
	auto found = pipelines.find(name);
	if (found == std::end(pipelines))
	{
		throw std::runtime_error("Cannot find pipeline from cache");
	}
	return &(found->second);

}

PipelineCacheObject* VKPipelineCache::operator[](std::string name)
{
	return GetPipelineFromCache(name);
}

PipelineCacheObject VKPipelineCache::CreatePipeline(
	std::string* descriptorSetLayoutNames,
	size_t descriptorSetCount,
	VkVertexInputBindingDescription* bindDescription,
	uint32_t bindingCount,
	VkVertexInputAttributeDescription* vertAttributes,
	size_t vertAttributecount,
	EntryHandle* shaderHandles,
	size_t shaderCount,
	VkCompareOp depthOp, VkSampleCountFlagBits sampleCount,
	std::string name)
{
	

	VkDescriptorSetLayout *layouts = reinterpret_cast<VkDescriptorSetLayout*>(majorDev->AllocFromDeviceCache(sizeof(VkDescriptorSetLayout)*descriptorSetCount));


	std::pair<VkShaderModule, VkShaderStageFlagBits> *shaders = reinterpret_cast<std::pair<VkShaderModule, VkShaderStageFlagBits>*>(
																majorDev->AllocFromDeviceCache(sizeof(std::pair<VkShaderModule, VkShaderStageFlagBits>) * shaderCount));

	auto& dslc = majorDev->descriptorLayoutCache;

	for (std::size_t i = 0; i < descriptorSetCount; i++)
	{
		layouts[i] = dslc.GetLayout(descriptorSetLayoutNames[i]);
	}

	for (std::size_t i = 0; i < shaderCount; i++)
	{
		shaders[i] = majorDev->GetShader(shaderHandles[i]);
	}

	auto co = CreateGraphicsPipeline(layouts, descriptorSetCount, 
		bindDescription, bindingCount,
		vertAttributes, vertAttributecount, 
		shaders, shaderCount, 
		depthOp, sampleCount
	);

	pipelines[name] = co;
	return co;
}

VkPipelineLayout VKPipelineCache::CreatePipelineLayout(VkDescriptorSetLayout *descriptorSetLayout, uint32_t count)
{
	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	
	pipelineLayoutInfo.setLayoutCount = count;
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayout;
	
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	return pipelineLayout;
}

PipelineCacheObject VKPipelineCache::CreateGraphicsPipeline(VkDescriptorSetLayout* descriptorSetLayouts,
	size_t descriptorSetCount,
	VkVertexInputBindingDescription* bindDescription,
	uint32_t bindingCount,
	VkVertexInputAttributeDescription* vertAttributes,
	size_t vertAttributecount,
	std::pair<VkShaderModule, VkShaderStageFlagBits>* shaders,
	size_t shaderCount,
	VkCompareOp depthOp, VkSampleCountFlagBits sampleCount)
{
	VkPipeline graphicsPipeline;

	VkPipelineLayout pipelineLayout = CreatePipelineLayout(descriptorSetLayouts, descriptorSetCount);

	std::array<VkDynamicState, 2> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineShaderStageCreateInfo* shaderInfos = reinterpret_cast<VkPipelineShaderStageCreateInfo*>(majorDev->AllocFromDeviceCache(sizeof(VkPipelineShaderStageCreateInfo) * shaderCount));

	for (int i = 0; i < shaderCount; i++)
	{
		shaderInfos[i] = AddShader(shaders[i].first, shaders[i].second);
	}

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	vertexInputInfo.vertexBindingDescriptionCount = bindingCount;
	vertexInputInfo.pVertexBindingDescriptions = bindDescription;
	
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttributecount);
	vertexInputInfo.pVertexAttributeDescriptions = vertAttributes;
	

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;


	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;


	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = sampleCount;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderCount);
	pipelineInfo.pStages = shaderInfos;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
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

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	PipelineCacheObject co = {
		.descLayout = (descriptorSetCount > 0) ? descriptorSetLayouts[0] : VK_NULL_HANDLE,
		.pipelineLayout = pipelineLayout,
		.pipeline = graphicsPipeline
	};

	return co;
}

VkPipelineShaderStageCreateInfo VKPipelineCache::AddShader(VkShaderModule& mod, VkShaderStageFlagBits flags)
{
	VkPipelineShaderStageCreateInfo shaderStageInfo{};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = flags;
	shaderStageInfo.module = mod;
	shaderStageInfo.pName = "main";
	return shaderStageInfo;
}

void VKPipelineCache::DestroyPipelineCache()
{
	for (auto& i : pipelines)
	{
		vkDestroyPipeline(device, i.second.pipeline, nullptr);

		vkDestroyPipelineLayout(device, i.second.pipelineLayout, nullptr);
	}
}