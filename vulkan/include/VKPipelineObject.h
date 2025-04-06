#pragma once

#include <array>
#include <vector>

#include "vulkan/vulkan.h"

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKPipelineCache.h"

class VKPipelineObject
{
public:

	VKPipelineObject() = delete;
	VKPipelineObject(
		std::string name, 
		size_t *vCount, 
		VkBuffer buffer,
		size_t frames
		) 
		: pipelineType(name), vertexCount(vCount), vertexBuffer(buffer), framesCount(frames) {
	}

	~VKPipelineObject() = default;

	void CreatePipelineObject(VkDevice device) {
		auto po = VKRenderer::gRenderInstance->GetPipeline(pipelineType);
		CreateDescriptorSets(device, po.descLayout);
	}

	void AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding)
	{
		

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = view;
		imageInfo.sampler = sampler;

		for (uint32_t frame = 0; frame < framesCount; frame++)
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

	void Draw(VkCommandBuffer& cb, uint32_t frame)
	{
		uint32_t drawSize = static_cast<uint32_t>(*vertexCount);

		auto po = VKRenderer::gRenderInstance->GetPipeline(pipelineType);

		if (po.descLayout)
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po.pipelineLayout, 0, 1, &descriptorSets[frame], 0, nullptr);

		if (vertexBuffer)
		{
			VkBuffer vertexBuffers[] = { vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
		}

		vkCmdDraw(cb, drawSize, 1, 0, 0);
	}

	void DrawIndirectOneBuffer(
		VkCommandBuffer& cb,
		VkBuffer& drawBuffer,
		uint32_t drawCount, uint32_t frame)
	{
		auto po = VKRenderer::gRenderInstance->GetPipeline(pipelineType);

		if (po.descLayout)
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po.pipelineLayout, 0, 1, &descriptorSets[frame], 0, nullptr);
	
		if (vertexBuffer)
		{
			VkBuffer vertexBuffers[] = { vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
		}

		vkCmdDrawIndirect(cb, drawBuffer, 0, drawCount, sizeof(VkDrawIndirectCommand));
	}

	std::string GetPipelineType() const
	{
		return pipelineType;
	}

private:

	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	std::vector<VkDescriptorImageInfo> imageInfos;

	std::string pipelineType;
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	size_t *vertexCount = nullptr;
	size_t framesCount = 0UL;

	void CreateDescriptorSets(VkDevice &device, VkDescriptorSetLayout& descriptorSetLayout)
	{
		if (!descriptorWrites.size())
			return;
		uint32_t frames = VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT;
		std::vector<VkDescriptorSetLayout> layouts(frames, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = VKRenderer::gRenderInstance->GetDescriptorPool();
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
};

