#include "pch.h"

#include "VKGraph.h"
#include "VKDevice.h"
#include "VKPipelineObject.h"

VKRenderGraph::VKRenderGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d)
	:
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, _d)
{
	
};

void VKRenderGraph::DrawScene(RecordingBufferObject* rbo, uint32_t frameNum)
{

	uint32_t count = pipelineObjCount.load();

	for (uint32_t i = 0; i < count; i++)
	{

		if (!activeIndicators[i]) continue;

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

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


		if (objHeader->type == GRAPHICS)
		{
			VKGraphicsPipelineObject* obj = (VKGraphicsPipelineObject*)objHeader;
			obj->Draw(rbo, frameNum, descriptorCount);
		}
		else if (objHeader->type == INDIRECTPO)
		{
			VKIndirectPipelineObject* obj = (VKIndirectPipelineObject*)objHeader;
			obj->Draw(rbo, frameNum, descriptorCount);
		}
		
	}

	currentPipeline = EntryHandle();
}

VKComputeGraph::VKComputeGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d)
	: 
	VKGraph(allocator, dynamicCount, descriptorCount, pipelineCount, _d)
{

}

void VKComputeGraph::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{

	uint32_t count = pipelineObjCount.load();

	for (uint32_t i = 0; i < count; i++)
	{
		EntryHandle objIndex = objects[i];

		if (!activeIndicators[i]) continue;

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

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

	currentPipeline = EntryHandle();
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