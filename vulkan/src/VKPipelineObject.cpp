#include "pch.h"

#include "VKPipelineObject.h"

#include "VKDevice.h"

#include <array>

VKPipelineObject::VKPipelineObject(EntryHandle _pid, EntryHandle _dsid, uint32_t* data, uint32_t moc, PipelineObjectType _type, uint32_t pcrCount, uint32_t memBarrierCount)
	: 
	pipelineID(_pid), descriptorSetId(_dsid), 
	objectData(data), maxObjectCapacity(moc), 
	objectCount(0U), type(_type), 
	pushConstantCount(pcrCount), memBarrierCapacity(memBarrierCount),
	memBarrierCounter(0)

{
	pushArgs = reinterpret_cast<PushConstantArguments*>(objectData + moc);
	infos = reinterpret_cast<VkBarrierInfo*>(pushArgs + pushConstantCount);
}


VKGraphicsPipelineObject::VKGraphicsPipelineObject(
	VKGraphicsPipelineObjectCreateInfo* createinfo)
	:
	VKPipelineObject(createinfo->pipelinename, createinfo->descriptorsetid, createinfo->data, createinfo->maxDynCap, GRAPHICSPO, createinfo->pushRangeCount, 0),
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
	VKPipelineObject(info->pipelineId, info->descriptorId, info->data,info->maxDynCap, COMPUTEPO, info->pushRangeCount, info->barrierCount),
	x(info->x),
	y(info->y),
	z(info->z)
{

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
	uint32_t bmbC = 0, mbC = 0, imbC = 0, i = 0;
	VkBarrierInfo* info = &infos[0];
	while (i < memBarrierCapacity)
	{
		VkBarrierInfo* next = info;
		uint32_t j = 0;
		while (info)
		{
			j++;
			switch (info->type)
			{
			case MEMBARRIER:

				break;
			case BUFFBARRIER:
				lbuffMemBarriers[bmbC] = *rbo->vkDeviceHandle->GetBufferMemoryBarrier(info->barrierIndex);
				lbuffMemBarriers[bmbC].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				lbuffMemBarriers[bmbC].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bmbC++;
				break;
			case IMAGEBARRIER:

				break;
			default:
				break;
			}

			info = info->child;
		}

		

		RBOPipelineBarrierArgs args = {
			.srcStageMask = next->srcStageMask,
			.dstStageMask = next->dstStageMask,
			.dependencyFlags = next->dependencyFlags,
			.memoryBarrierCount = mbC,
			.pMemoryBarriers = lmemBarriers.data(),
			.bufferMemoryBarrierCount = bmbC,
			.pBufferMemoryBarriers = lbuffMemBarriers.data(),
			.imageMemoryBarrierCount = imbC,
			.pImageMemoryBarriers = limageMemBarriers.data()
		};

		rbo->BindPipelineBarrierCommand(&args);

		bmbC = 0;

		i += j;

		info = next->next;
	}


}

void VKPipelineObject::AddBufferMemoryBarrier(
	VKDevice* d, EntryHandle bufferIndex, 
	size_t size, size_t offset, 
	VkAccessFlags srcPoint, VkAccessFlags dstPoint,
	VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
	)
{
	EntryHandle barrierIndex = d->CreateBufferMemoryBarrier(srcPoint, dstPoint, 0, 0, bufferIndex, offset, size);
	VkBarrierInfo* info = &infos[memBarrierCounter];
	VkBarrierInfo* nextPtr = nullptr;
	VkPipelineStageFlags stages = srcStage | dstStage;
	for (uint32_t i = 0; i < memBarrierCapacity; i++)
	{
		VkBarrierInfo* cand = &infos[i];
		if ((cand->srcStageMask | cand->dstStageMask) == stages)
		{
			VkBarrierInfo** next = &cand->child;
			while (*next)
			{
				next = &(*next)->child;
			}
			*next = info;
			break;
		}
		nextPtr = &infos[i];
	}

	if (nextPtr)
	{
		nextPtr->next = info;
	}

	info->srcStageMask = srcStage;
	info->dstStageMask = dstStage;
	info->dependencyFlags = 0;
	info->type = BUFFBARRIER;
	info->barrierIndex = barrierIndex;
	info->next = nullptr;
	info->child = nullptr;
	memBarrierCounter++;
}

void VKPipelineObject::AddPushConstant(void* _data, uint32_t size, uint32_t offset, uint32_t bindLocation, VkShaderStageFlags flags)
{
	PushConstantArguments* args = &pushArgs[bindLocation];
	args->data = _data;
	args->size = size;
	args->offset = offset;
	args->stage = flags;
}