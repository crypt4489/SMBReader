#include "pch.h"

#include "VKPipelineObject.h"

#include "VKDevice.h"

#include <array>

VKPipelineObject::VKPipelineObject(EntryHandle _pid, EntryHandle _dsid, uint32_t* data, uint32_t moc, PipelineObjectType _type)
	: pipelineID(_pid), descriptorSetId(_dsid), objectData(data), maxObjectCapacity(moc), objectCount(0U), type(_type)
{

}


VKGraphicsPipelineObject::VKGraphicsPipelineObject(
	VKGraphicsPipelineObjectCreateInfo* createinfo)
	:
	VKPipelineObject(createinfo->pipelinename, createinfo->descriptorsetid, createinfo->data, createinfo->maxDynCap, GRAPHICSPO),
	vertexCount(createinfo->vertexCount),
	vertexBufferOffset(createinfo->vertexBufferOffset),
	vertexBufferIndex(createinfo->vertexBufferIndex),
	indirectBufferIndex(createinfo->indirectDrawBuffer),
	indirectBufferOffset(createinfo->indirectDrawOffset),
	drawType(createinfo->drawType),
	indexBufferHandle(createinfo->indexBufferHandle),
	indexBufferOffset(createinfo->indexBufferOffset),
	indexCount(createinfo->indexCount)
{

}


void VKGraphicsPipelineObject::Draw(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = static_cast<uint32_t>(vertexCount);

	if (EntryHandle() != descriptorSetId) {

		rbo->BindDescriptorSets(descriptorSetId, frame, 1, firstSet, objectCount, objectData);
	}

	if (vertexBufferIndex != ~0ui64)
	{
		rbo->BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	if (indexBufferOffset != ~0ui64)
	{
		rbo->BindIndexBuffer(indexBufferHandle, static_cast<uint32_t>(indexBufferOffset));
		rbo->BindingDrawIndexedCmd(static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
	}
	else {
		rbo->BindingDrawCmd(0, drawSize);
	}
}

void VKGraphicsPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject *rbo,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet)
{
	
	

	if (EntryHandle() != descriptorSetId) {

		rbo->BindDescriptorSets(descriptorSetId, frame, 1, firstSet, objectCount, objectData);
	}
		

	if (vertexBufferIndex != ~0ui64)
	{
		rbo->BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo->BindingIndirectDrawCmd(indirectBufferIndex, drawCount, indirectBufferOffset);
}


void VKPipelineObject::SetPerObjectData(uint32_t _dynamicOffset)
{
	objectData[objectCount++] = _dynamicOffset;
}

VKComputePipelineObject::VKComputePipelineObject(VKComputePipelineObjectCreateInfo* info)
	: 
	VKPipelineObject(info->pipelineId, info->descriptorId, info->data,info->maxDynCap, COMPUTEPO),
	x(info->x),
	y(info->y),
	z(info->z),
	membarriersCount(info->barrierCount),
	barrierInfoCount(info->barrierCount),
	counter(0)
{
	infos = reinterpret_cast<VkBarrierInfo*>(info->data + info->maxDynCap);
	memBarriers = reinterpret_cast<EntryHandle*>(infos + info->barrierCount);
}

void VKComputePipelineObject::Dispatch(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet)
{
	if (EntryHandle() != descriptorSetId) {

		rbo->BindComputeDescriptorSets(descriptorSetId, frame, 1, firstSet, objectCount, objectData);
	}

	rbo->DispatchCommand(x, y, z);

	std::array<VkBufferMemoryBarrier, 5> lbuffMemBarriers{};
	std::array<VkMemoryBarrier, 5> lmemBarriers{};
	std::array<VkImageMemoryBarrier, 5> limageMemBarriers{};
	uint32_t bmbC = 0, mbC = 0, imbC = 0;
	for (uint32_t i = 0; i < membarriersCount; i++)
	{
		VkBarrierInfo* info = &infos[i];
		switch (info->type)
		{
		case MEMBARRIER:

			break;
		case BUFFBARRIER:
			lbuffMemBarriers[bmbC] = *rbo->vkDeviceHandle->GetBufferMemoryBarrier(memBarriers[i]);
			lbuffMemBarriers[bmbC].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			lbuffMemBarriers[bmbC].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bmbC++;
			break;
		case IMAGEBARRIER:

			break;
		default:
			break;
		}
	
	}


	RBOPipelineBarrierArgs args = {
		.srcStageMask = infos[0].srcStageMask,
		.dstStageMask = infos[0].dstStageMask,
		.dependencyFlags = infos[0].depenencyFlags,
		.memoryBarrierCount = mbC,
		.pMemoryBarriers = lmemBarriers.data(),
		.bufferMemoryBarrierCount = bmbC,
		.pBufferMemoryBarriers = lbuffMemBarriers.data(),
		.imageMemoryBarrierCount = imbC,
		.pImageMemoryBarriers = limageMemBarriers.data()
	};

	rbo->BindPipelineBarrierCommand(&args);


}

void VKComputePipelineObject::AddBufferMemoryBarrier(VKDevice* d, EntryHandle bufferIndex, size_t size, size_t offset, uint32_t bindPoint)
{
	EntryHandle barrierIndex = d->CreateBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, bindPoint, 0, 0, bufferIndex, offset, size);
	VkBarrierInfo* info = &infos[counter];
	info->srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	info->dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	info->depenencyFlags = 0;
	info->type = BUFFBARRIER;
	memBarriers[counter] = barrierIndex;
	counter++;
}