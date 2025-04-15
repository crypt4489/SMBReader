#pragma once

#include <array>
#include <vector>
#include <functional>

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
		size_t _offset
		) 
		: pipelineType(name), vertexCount(vCount), buffer(buffer), offset(_offset) {
	}

	~VKPipelineObject() = default;

	void Draw(VkCommandBuffer& cb, uint32_t frame, uint32_t setCount)
	{
		uint32_t drawSize = static_cast<uint32_t>(*vertexCount);

		auto po = VKRenderer::gRenderInstance->GetPipeline(pipelineType);
		auto dsc = VKRenderer::gRenderInstance->GetDescriptorSetCache();
		VkDescriptorSet set = dsc->GetDescriptorSetPerFrame(pipelineType, frame);

		std::array<uint32_t, 1> dynamicOffsets = { dynamicOffset };

		uint32_t dynCount = static_cast<uint32_t>(dynamicOffsets.size());

		if (!perObjectShaderData)
			dynCount = 0;

		if (po.descLayout)
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po.pipelineLayout, setCount, 1, &set, dynCount, dynamicOffsets.data());

		if (buffer)
		{
			VkBuffer vertexBuffers[] = { buffer };
			VkDeviceSize offsets[] = { offset };
			vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
		}

		vkCmdDraw(cb, drawSize, 1, 0, 0);
	}

	void DrawIndirectOneBuffer(
		VkCommandBuffer& cb,
		VkBuffer& drawBuffer,
		uint32_t drawCount, 
		uint32_t frame, 
		uint32_t setCount,
		size_t indirectDrawBufferOffset)
	{
		auto po = VKRenderer::gRenderInstance->GetPipeline(pipelineType);
		auto dsc = VKRenderer::gRenderInstance->GetDescriptorSetCache();
		VkDescriptorSet set = dsc->GetDescriptorSetPerFrame(pipelineType, frame);

		std::array<uint32_t, 1> dynamicOffsets = { dynamicOffset };

		uint32_t dynCount = static_cast<uint32_t>(dynamicOffsets.size());

		if (!perObjectShaderData)
			dynCount = 0;


		if (po.descLayout)
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po.pipelineLayout, setCount, 1, &set, dynCount, dynamicOffsets.data());
	
		if (buffer)
		{
			VkBuffer vertexBuffers[] = { buffer };
			VkDeviceSize offsets[] = { offset };
			vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
		}

		vkCmdDrawIndirect(cb, drawBuffer, indirectDrawBufferOffset, drawCount, sizeof(VkDrawIndirectCommand));
	}

	std::string GetPipelineType() const
	{
		return pipelineType;
	}

	void SetPerObjectData(void* data, size_t dataSize, uint32_t _dynamicOffset)
	{
		perObjectShaderData = data;
		perObjectShaderDataSize = dataSize;
		dynamicOffset = _dynamicOffset;
	}


	void* perObjectShaderData;
	VkDeviceSize perObjectShaderDataSize;
	uint32_t dynamicOffset;

private:
	std::string pipelineType;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceSize offset = 0U;
	size_t *vertexCount = nullptr;
	
	
};

