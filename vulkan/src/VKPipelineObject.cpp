#include "pch.h"

#include "VKPipelineObject.h"

#include "VKDevice.h"

#include <array>

VKPipelineObject::VKPipelineObject(DeviceOwnedAllocator* allocator, EntryHandle _pid, EntryHandle* _dsid, uint32_t *_dynamicPerSet, uint32_t descCount, uint32_t moc, uint32_t pcrCount, uint32_t memBarrierCount)
	: 
	pipelineID(_pid),
	maxObjectCapacity(moc), 
	objectCount(0U), 
	pushConstantCount(pcrCount), memBarrierCapacity(memBarrierCount),
	memBarrierCounter(0), descriptorCount(descCount)

{
	objectData = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * moc));
	pushArgs = reinterpret_cast<PushConstantArguments*>(allocator->Alloc(sizeof(PushConstantArguments) * pushConstantCount));
	infos = reinterpret_cast<VkBarrierInfo*>(allocator->Alloc(sizeof(VkBarrierInfo) * memBarrierCount));
	descriptorSetId = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * descCount));
	dynamicPerSet = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * descCount));
	for (uint32_t i = 0; i < descCount; i++) {
		descriptorSetId[i] = _dsid[i];
		dynamicPerSet[i] = _dynamicPerSet[i];
	}
}


VKGraphicsPipelineObject::VKGraphicsPipelineObject(
	VKGraphicsPipelineObjectCreateInfo* createinfo, DeviceOwnedAllocator* allocator)
	:
	VKPipelineObject(allocator, createinfo->pipelinename, createinfo->descriptorsetid, createinfo->dynamicPerSet, createinfo->descCount, createinfo->maxDynCap, createinfo->pushRangeCount, 0),
	vertexCount(createinfo->vertexCount),
	vertexBufferOffset(createinfo->vertexBufferOffset),
	vertexBufferIndex(createinfo->vertexBufferIndex),
	indirectBufferIndex(createinfo->indirectDrawBuffer),
	indirectBufferOffset(createinfo->indirectDrawOffset),
	indexBufferHandle(createinfo->indexBufferHandle),
	indexBufferOffset(createinfo->indexBufferOffset),
	indexCount(createinfo->indexCount),
	instanceCount(createinfo->instanceCount)
{

}


void VKGraphicsPipelineObject::Draw(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = static_cast<uint32_t>(vertexCount);

	uint32_t offset = 0;
	for (uint32_t i = 0; i < descriptorCount; i++)
	{
		rbo->BindDescriptorSets(descriptorSetId[i], frame, 1, firstSet + i, dynamicPerSet[i], &objectData[offset]);
		offset += dynamicPerSet[i];
	}

	if (vertexBufferIndex != ~0ui64)
	{
		rbo->BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	if (indexBufferOffset != ~0ui64)
	{
		rbo->BindIndexBuffer(indexBufferHandle, static_cast<uint32_t>(indexBufferOffset));
		rbo->BindingDrawIndexedCmd(static_cast<uint32_t>(indexCount), instanceCount, 0, 0, 0);
	}
	else {
		rbo->BindingDrawCmd(0, drawSize, 0, instanceCount);
	}
}

void VKGraphicsPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject *rbo,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet)
{

	uint32_t offset = 0;
	for (uint32_t i = 0; i < descriptorCount; i++)
	{
		rbo->BindDescriptorSets(descriptorSetId[i], frame, 1, firstSet + i, dynamicPerSet[i], &objectData[offset]);
		offset += dynamicPerSet[i];
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

VKComputePipelineObject::VKComputePipelineObject(VKComputePipelineObjectCreateInfo* info, DeviceOwnedAllocator* allocator)
	: 
	VKPipelineObject(allocator, info->pipelineId,  info->descriptorId, info->dynamicPerSet, info->descCount, info->maxDynCap, info->pushRangeCount, info->barrierCount),
	x(info->x),
	y(info->y),
	z(info->z)
{

}

void VKPipelineObject::CreatePipelineBarriers(RecordingBufferObject* rbo, VKBarrierLocation location)
{

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
			if (info->location == location)
			{
				switch (info->type)
				{
				case MEMBARRIER:
					lmemBarriers[mbC] = *rbo->vkDeviceHandle->GetMemoryBarrier(info->barrierIndex);
					mbC++;
					break;
				case BUFFBARRIER:
					lbuffMemBarriers[bmbC] = *rbo->vkDeviceHandle->GetBufferMemoryBarrier(info->barrierIndex);
					lbuffMemBarriers[bmbC].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					lbuffMemBarriers[bmbC].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					bmbC++;
					break;
				case IMAGEBARRIER:
					limageMemBarriers[imbC] = *rbo->vkDeviceHandle->GetImageMemoryBarrier(info->barrierIndex);
					limageMemBarriers[imbC].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					limageMemBarriers[imbC].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imbC++;
					break;
				default:
					break;
				}
				info = info->child;
			}
			else
			{
				i++;
				j = 0;
				info = info->next;
				next = info;
			}
		}

		if (next) {

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
			imbC = 0;
			mbC = 0;

			info = next->next;
		}

		i += j;
	}
}

void VKComputePipelineObject::Dispatch(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t offset = 0;
	for (uint32_t i = 0; i < descriptorCount; i++)
	{
		rbo->BindComputeDescriptorSets(descriptorSetId[i], frame, 1, firstSet + i, dynamicPerSet[i], &objectData[offset]);
		offset += dynamicPerSet[i];
	}
	
	CreatePipelineBarriers(rbo, BEFORE);

	rbo->DispatchCommand(x, y, z);

	CreatePipelineBarriers(rbo, AFTER);
}


void VKPipelineObject::AddInfoBarrier(VkBarrierInfo* info, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
									  EntryHandle barrierIndex, VKBarrierLocation location, uint16_t barrierType)
{
	info->srcStageMask = srcStage;
	info->dstStageMask = dstStage;
	info->dependencyFlags = 0;
	info->type = barrierType;
	info->barrierIndex = barrierIndex;
	info->next = nullptr;
	info->child = nullptr;
	info->location = location;
	memBarrierCounter++;
}


void VKPipelineObject::AddBufferMemoryBarrier(
	EntryHandle index, VKBarrierLocation location,
	VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
)
{
	AddInfoBarrier(GetNextBarrierInfo(srcStage, dstStage), srcStage, dstStage, index, location, BUFFBARRIER);
}

VkBarrierInfo* VKPipelineObject::GetNextBarrierInfo(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
	VkBarrierInfo* info = &infos[memBarrierCounter];
	VkBarrierInfo* nextPtr = nullptr;
	for (uint32_t i = 0; i < memBarrierCounter; i++)
	{
		VkBarrierInfo* cand = &infos[i];
		if (cand->srcStageMask == srcStage && cand->dstStageMask == dstStage)
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

	return info;
}

void VKPipelineObject::AddImageMemoryBarrier(
	EntryHandle index, VKBarrierLocation location, VkShaderStageFlags srcStage, VkShaderStageFlags dstStage
)
{
	AddInfoBarrier(GetNextBarrierInfo(srcStage, dstStage), srcStage, dstStage, index, location, IMAGEBARRIER);
}

void VKPipelineObject::AddPushConstant(void* _data, uint32_t size, uint32_t offset, uint32_t bindLocation, VkShaderStageFlags flags)
{
	PushConstantArguments* args = &pushArgs[bindLocation];
	args->data = _data;
	args->size = size;
	args->offset = offset;
	args->stage = flags;
}