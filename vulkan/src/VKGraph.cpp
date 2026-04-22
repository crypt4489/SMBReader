#include "pch.h"

#include "VKGraph.h"
#include "VKDevice.h"

VKRenderGraph::VKRenderGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d)
	:
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, _d)
{
	
};

void VKRenderGraph::DrawScene(RecordingBufferObject* rbo, uint32_t frameNum)
{
}

VKComputeGraph::VKComputeGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d)
	: 
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, _d)
{

}

void VKComputeGraph::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{

	
}

uint32_t VKGraph::AddObject(EntryHandle obj)
{
	uint32_t objIndex = pipelineObjCount.fetch_add(1);
	objectsCopies[objIndex] = obj;
	activeIndicatorsCopies[objIndex] = true;
	return objIndex;
}

void VKGraph::AddDynamicOffset(uint32_t offset)
{
	uint32_t index = dynamicOffsetCount++;
	dynamicOffsets[index] = offset;
}

bool VKGraph::SetActive(uint32_t objIndex, bool active)
{
	activeIndicatorsCopies[objIndex] = active;
	return active;
}

void VKGraph::UpdateLists()
{
	uint32_t count = pipelineObjSize;

	memcpy(objects, objectsCopies, sizeof(EntryHandle) * count);
	memcpy(activeIndicators, activeIndicatorsCopies, sizeof(uint8_t) * count);

	std::swap(activeIndicators, activeIndicatorsCopies);

	std::swap(objects, objectsCopies);
}

VKGraph::VKGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d)
	:
	dynamicOffsetSize(dynamicCount), pipelineObjSize(pipelineCount),
	pipelineObjCount(0), dynamicOffsetCount(0), dev(_d), descriptorCount(descriptorCount)
{
	dynamicOffsets = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * dynamicOffsetSize));
	objects = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * pipelineObjSize));
	objectsCopies = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * pipelineObjSize));
	activeIndicators = reinterpret_cast<uint8_t*>(allocator->Alloc(pipelineObjSize));
	activeIndicatorsCopies = reinterpret_cast<uint8_t*>(allocator->Alloc(pipelineObjSize));
	descriptorId = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * descriptorCount));
	dynamicsPerSet = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * descriptorCount));
	
}