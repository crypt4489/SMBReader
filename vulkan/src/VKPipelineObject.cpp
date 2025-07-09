#include "VKPipelineObject.h"

#include <array>
#include <vector>
#include <functional>

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKPipelineCache.h"

VKPipelineObject::VKPipelineObject(
	std::string name,
	size_t vertexBufferIndex_,
	size_t vertexBufferOffset_,
	size_t* vCount,
	uint32_t renderTarget)
	:
	pipelineType(name),
	vertexCount(vCount),
	vertexBufferOffset(vertexBufferOffset_),
	vertexBufferIndex(vertexBufferIndex_),
	mainRenderTarget(renderTarget)
{

}

void VKPipelineObject::Draw(VkCommandBuffer& cb, uint32_t frame, uint32_t setCount)
{
	uint32_t drawSize = static_cast<uint32_t>(*vertexCount);

	auto po = VKRenderer::gRenderInstance->GetVulkanPipeline(mainRenderTarget, pipelineType);

	if (po->descLayout) {
		std::array<uint32_t, 1> dynamicOffsets = { 0 };

		uint32_t dynCount = static_cast<uint32_t>(dynamicOffsets.size());

		if (!perObjectShaderData)
			dynCount = 0;

		VkDescriptorSet set = VKRenderer::gRenderInstance->GetDescriptorSet(pipelineType, frame);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po->pipelineLayout, setCount, 1, &set, dynCount, dynamicOffsets.data());
	}

	if (vertexBufferIndex != ~0ui32)
	{
		VkBuffer vertexBuffers[] = { VKRenderer::gRenderInstance->GetDynamicUniformBuffer() };
		VkDeviceSize offsets[] = { vertexBufferOffset };
		vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
	}

	vkCmdDraw(cb, drawSize, 1, 0, 0);
}

void VKPipelineObject::DrawIndirectOneBuffer(
	VkCommandBuffer& cb,
	VkBuffer& drawBuffer,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t setCount,
	size_t indirectDrawBufferOffset)
{
	
	auto po = VKRenderer::gRenderInstance->GetVulkanPipeline(mainRenderTarget, pipelineType);

	if (po->descLayout) {
		std::array<uint32_t, 1> dynamicOffsets = { dynamicOffset };

		uint32_t dynCount = static_cast<uint32_t>(dynamicOffsets.size());

		if (!perObjectShaderData)
			dynCount = 0;

		VkDescriptorSet set = VKRenderer::gRenderInstance->GetDescriptorSet(pipelineType, frame);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, po->pipelineLayout, setCount, 1, &set, dynCount, dynamicOffsets.data());
	}
		

	if (vertexBufferIndex != ~0ui32)
	{
		VkBuffer vertexBuffers[] = { VKRenderer::gRenderInstance->GetDynamicUniformBuffer() };
		VkDeviceSize offsets[] = { vertexBufferOffset };
		vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
	}

	vkCmdDrawIndirect(cb, drawBuffer, indirectDrawBufferOffset, drawCount, sizeof(VkDrawIndirectCommand));
}

std::string VKPipelineObject::GetPipelineType() const
{
	return pipelineType;
}

void VKPipelineObject::SetPerObjectData(void* data, size_t dataSize, OffsetIndex& _dynamicOffset)
{
	perObjectShaderData = data;
	perObjectShaderDataSize = dataSize;
	dynamicOffset = std::move(_dynamicOffset);
}