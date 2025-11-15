#include "pch.h"

#include "VKGraph.h"
#include "VKDevice.h"
#include "VKPipelineObject.h"

VKRenderGraph::VKRenderGraph(EntryHandle _renderTargetIndex, DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d)
	:
	VKGraph(allocator, dCount, descCount, pCount, _d),
	renderTargetIndex(_renderTargetIndex)
{
	
};

void VKRenderGraph::DrawScene(RecordingBufferObject* rbo, uint32_t frameNum, VkExtent2D *rect)
{
	uint32_t dynamicCount = static_cast<uint32_t>(dynamicOffsetCount);

	rbo->BeginRenderPassCommand(renderTargetIndex, frameNum, { {0, 0}, *rect });

	float x = static_cast<float>(rect->width), y = static_cast<float>(rect->height);

	rbo->SetViewportCommand(0, 0, x, y, 0.0f, 1.0f);

	rbo->SetScissorCommand(0, 0, rect->width, rect->height);

	std::unique_lock lock(objectGuard);

	for (size_t i = 0; i<pipelineObjCount; i++)
	{

		if (!activeIndicators[i]) continue;

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

			rbo->BindGraphicsPipeline(handle);
			
			for (int descI = 0; i < descriptorCount; i++) {
				rbo->BindDescriptorSets(descriptorId[descI], frameNum, 1, descI, (descI == 0 ? dynamicCount : 0), (descI == 0 ? dynamicOffsets : nullptr));
				descI++;
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

	rbo->EndRenderPassCommand();

	currentPipeline = EntryHandle();
}

VKComputeGraph::VKComputeGraph(DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d)
	: 
	VKGraph(allocator, dCount, descCount, pCount, _d)
{

}

void VKComputeGraph::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{
	uint32_t dynamicCount = static_cast<uint32_t>(dynamicOffsetCount);
	std::unique_lock lock(objectGuard);

	for (size_t i = 0; i < pipelineObjCount; i++)
	{
		EntryHandle objIndex = objects[i];

		if (!activeIndicators[i]) continue;

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

			rbo->BindComputePipeline(handle);

			for (int descI = 0; i<descriptorCount; i++) {
				rbo->BindComputeDescriptorSets(descriptorId[descI], frameNum, 1, descI, (descI == 0 ? dynamicCount : 0), (descI == 0 ? dynamicOffsets : nullptr));
				descI++;
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

VKGraph::VKGraph(DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d)
	:
	dynamicOffsetSize(dCount), pipelineObjSize(pCount),
	pipelineObjCount(0), dynamicOffsetCount(0), dev(_d), descriptorCount(descCount)
{
	dynamicOffsets = reinterpret_cast<uint32_t*>(allocator->Alloc(sizeof(uint32_t) * dCount));
	objects = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * pCount));
	activeIndicators = reinterpret_cast<uint8_t*>(allocator->Alloc(pCount));
	memset(activeIndicators, 0, pCount);
	descriptorId = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * descCount));
	for (size_t i = 0; i < descCount; i++) descriptorId[i] = EntryHandle();
}