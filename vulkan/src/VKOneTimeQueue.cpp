#include "VKOneTimeQueue.h"
#include "VKDevice.h"
#include "VKPipelineObject.h"

VKOneTimeQueue::VKOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d)
	:
 pipelineObjSize(pipelineCount), pipelineObjCount(0), dev(_d)
{
	
	objects = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * pipelineObjSize));
}

uint32_t VKOneTimeQueue::AddObject(EntryHandle obj)
{
	uint32_t objIndex = pipelineObjCount.fetch_add(1);
	objects[objIndex] = obj;
	return objIndex;
}

void VKOneTimeQueue::UpdateQueue()
{
	pipelineObjCount = 0;
}


VKGraphicsOneTimeQueue::VKGraphicsOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d)
	:
	VKOneTimeQueue(allocator, pipelineCount, _d)
{

}

void VKGraphicsOneTimeQueue::DrawScene(RecordingBufferObject* rbo, uint32_t frameNum)
{
	uint32_t count = pipelineObjCount.load();

	for (uint32_t i = 0; i < count; i++)
	{

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

			rbo->BindGraphicsPipeline(handle);
		}

		for (uint32_t i = 0; i < objHeader->pushConstantCount; i++)
		{
			PushConstantArguments* args = &objHeader->pushArgs[i];
			rbo->PushConstantsCommand(args->offset, args->size, args->stage, args->data);
		}

		VKGraphicsPipelineObject* obj = (VKGraphicsPipelineObject*)objHeader;
		obj->Draw(rbo, frameNum, 0);
	}

	currentPipeline = EntryHandle();
}


VKComputeOneTimeQueue::VKComputeOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d)
	:
	VKOneTimeQueue(allocator, pipelineCount, _d)
{

}

void VKComputeOneTimeQueue::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{
	uint32_t count = pipelineObjCount.load();

	for (uint32_t i = 0; i < count; i++)
	{
		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;

			rbo->BindComputePipeline(handle);

			uint32_t dynamicOffset = 0;

		}

		for (uint32_t i = 0; i < objHeader->pushConstantCount; i++)
		{
			PushConstantArguments* args = &objHeader->pushArgs[i];
			rbo->PushConstantsCommand(args->offset, args->size, args->stage, args->data);
		}

		VKComputePipelineObject* obj = (VKComputePipelineObject*)objHeader;
		obj->Dispatch(rbo, frameNum, 0);

	}

	currentPipeline = EntryHandle();
}