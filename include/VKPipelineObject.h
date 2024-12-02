#pragma once

#include <array>
#include <vector>

#include "vulkan/vulkan.h"

#include "RenderInstance.h"

class VKPipelineObject
{
public:

	VKPipelineObject()
	{
		//CreatePipelineObject();
	}

	~VKPipelineObject()
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();

		if (graphicsPipeline)
		{
			vkDestroyPipeline(device, graphicsPipeline, nullptr);
		}

		if (pipelineLayout)
		{
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		}

		if (descriptorSetLayout)
		{
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		}
	}

	void CreatePipelineObject() {
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();
		CreateDescriptorSetLayout(device);
		CreateDescriptorSets(device);
		CreatePipelineLayout(device);
		CreatePipeline(device, ::VK::Renderer::gRenderInstance->GetRenderPass());
	}

	VkPipeline GetPipeline() const
	{
		return graphicsPipeline;
	}

	void AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding)
	{
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = binding;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		descSetBindings.push_back(layoutBinding);

		uint32_t frames = ::VK::Renderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = view;
		imageInfo.sampler = sampler;

		for (uint32_t frame = 0; frame < frames; frame++)
		{
			
			imageInfos.push_back(imageInfo);

			VkWriteDescriptorSet descriptorWrite{};

			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			//descriptorWrite.dstSet = descriptorSets[i];
			descriptorWrite.dstBinding = binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			//descriptorWrite.pImageInfo = &imageInfos.back();
			descriptorWrites.push_back(descriptorWrite);
		}
	}

	void Draw(VkCommandBuffer cb, uint32_t vertexCount, uint32_t frame)
	{
		if (descriptorSetLayout)
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[frame], 0, nullptr);

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		vkCmdDraw(cb, vertexCount, 1, 0, 0);
	}

	void AddShader(const std::string &name, VkShaderStageFlagBits flags)
	{
		VkShaderModule mod = ::VK::Renderer::gRenderInstance->GetShaderFromCache(name);
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = flags;
		shaderStageInfo.module = mod;
		shaderStageInfo.pName = "main";
		shaderInfos.push_back(shaderStageInfo);
	}


private:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	std::vector<VkPipelineShaderStageCreateInfo> shaderInfos;
	std::vector<VkDescriptorSetLayoutBinding> descSetBindings;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	std::vector<VkDescriptorImageInfo> imageInfos;
	

	void CreateDescriptorSetLayout(VkDevice device)
	{
		if (!descSetBindings.size())
			return;

		//VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(descSetBindings.size());
		layoutInfo.pBindings = descSetBindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}

		descSetBindings.clear();
	}

	void CreateDescriptorSets(VkDevice device)
	{
		if (!descriptorWrites.size())
			return;
		uint32_t frames = ::VK::Renderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT;
		std::vector<VkDescriptorSetLayout> layouts(frames, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = ::VK::Renderer::gRenderInstance->GetDescriptorPool();
		allocInfo.descriptorSetCount = frames;
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(frames);

		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (uint32_t i = 0; i < descriptorWrites.size(); i+=frames)
		{
			for (uint32_t frame = 0; frame < frames; frame++)
			{
				descriptorWrites[i + frame].dstSet = descriptorSets[frame];
				descriptorWrites[i + frame].pImageInfo = &imageInfos[i + frame];
			}
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

		descriptorWrites.clear();
		imageInfos.clear();

	}

	void CreatePipelineLayout(VkDevice device)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		if (descriptorSetLayout)
		{
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		}
		else 
		{
			pipelineLayoutInfo.setLayoutCount = 0;
			pipelineLayoutInfo.pSetLayouts = nullptr;
		}
		
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void CreatePipeline(VkDevice device, VkRenderPass renderPass)
	{
		// remember to fix the directory these are stored in for all build

	
		if (!shaderInfos.size())
		{
			std::cerr << " No shaders for object\n";
			return;
		}

		std::array<VkDynamicState, 2> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;


		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = ::VK::Renderer::gRenderInstance->GetMSAASamples();
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
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderInfos.size());
		pipelineInfo.pStages = shaderInfos.data();

		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}
	}
};

