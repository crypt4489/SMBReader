#include "pch.h"

#include "VKGraph.h"
#include "VKDevice.h"
#include "VKPipelineObject.h"

VKRenderGraph::VKRenderGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d)
	:
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, maxConcurrentAccesses, _d)
{
	
};

void VKRenderGraph::DrawScene(RecordingBufferObject* rbo, uint32_t frameNum)
{
	std::shared_lock lock(objectGuard);

	for (uint32_t i = 0; i<1; i++)
	{

		if (!activeIndicators[i]) continue;

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline[frameNum])
		{
			currentPipeline[frameNum] = handle;

			rbo->BindGraphicsPipeline(handle);
			
			uint32_t dynamicOffset = 0;

			for (uint32_t descI = 0; descI < descriptorCount; descI++) {
				rbo->BindDescriptorSets(descriptorId[descI], frameNum, 1, descI, dynamicsPerSet[descI], &dynamicOffsets[dynamicOffset]);
				dynamicOffset += dynamicsPerSet[descI];
			}
		}

		for (uint32_t i = 0; i < objHeader->pushConstantCount; i++)
		{
			PushConstantArguments* args = &objHeader->pushArgs[i];
			rbo->PushConstantsCommand(args->offset, args->size, args->stage, args->data);
		}

		VKGraphicsPipelineObject* obj = (VKGraphicsPipelineObject*)objHeader;
		obj->Draw(rbo, frameNum, descriptorCount);
	}

	currentPipeline[frameNum] = EntryHandle();
}

VKComputeGraph::VKComputeGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d)
	: 
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, maxConcurrentAccesses, _d)
{

}

void VKComputeGraph::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{
	std::shared_lock lock(objectGuard);

	for (uint32_t i = 0; i < pipelineObjCount; i++)
	{
		EntryHandle objIndex = objects[i];

		if (!activeIndicators[i]) continue;

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline[frameNum])
		{
			currentPipeline[frameNum] = handle;

			rbo->BindComputePipeline(handle);

			uint32_t dynamicOffset = 0;

			for (uint32_t descI = 0; descI<descriptorCount; descI++) {
				rbo->BindComputeDescriptorSets(descriptorId[descI], frameNum, 1, descI, dynamicsPerSet[descI], &dynamicOffsets[dynamicOffset]);
				dynamicOffset += dynamicsPerSet[descI];
			}
		}

		for (uint32_t i = 0; i < objHeader->pushConstantCount; i++)
		{
			PushConstantArguments* args = &objHeader->pushArgs[i];
			rbo->PushConstantsCommand(args->offset, args->size, args->stage, args->data);
		}

		VKComputePipelineObject* obj = (VKComputePipelineObject*)objHeader;
		obj->Dispatch(rbo, frameNum, descriptorCount);

	}

	currentPipeline[frameNum] = EntryHandle();
}

uint32_t VKGraph::AddObject(EntryHandle obj)
{
	std::unique_lock lock(objectGuard);
	uint32_t objIndex = pipelineObjCount++;
	objects[objIndex] = obj;
	activeIndicators[objIndex] = true;
	return objIndex;
}

void VKGraph::AddDynamicOffset(uint32_t offset)
{
	std::unique_lock lock(objectGuard);
	dynamicOffsets[dynamicOffsetCount++] = offset;
}

bool VKGraph::SetActive(uint32_t objIndex, bool active)
{
	std::unique_lock lock(objectGuard);
	bool ret = ((bool)activeIndicators[objIndex]) == active;
	if (!ret) 
		activeIndicators[objIndex] = active;
	return ret;
}

VKGraph::VKGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d)
	:
	dynamicOffsetSize(dynamicCount), pipelineObjSize(pipelineCount),
	pipelineObjCount(0), dynamicOffsetCount(0), dev(_d), descriptorCount(descriptorCount), maxFramesInFlight(maxConcurrentAccesses)
{
	dynamicOffsets = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * dynamicOffsetSize));
	objects = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * pipelineObjSize));
	activeIndicators = reinterpret_cast<uint8_t*>(allocator->Alloc(pipelineObjSize));
	descriptorId = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * descriptorCount));
	dynamicsPerSet = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * descriptorCount));
	currentPipeline = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * maxFramesInFlight));
	for (uint32_t i = 0; i < maxConcurrentAccesses; i++)
		currentPipeline[i] = EntryHandle();
}