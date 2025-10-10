#include "pch.h"

#include "VKGraph.h"
#include "VKDevice.h"
#include "VKPipelineObject.h"

VKRenderGraph::VKRenderGraph(EntryHandle _renderTargetIndex, void *data, size_t dCount, size_t pCount, VKDevice *_d) 
	:
	VKGraph(data, dCount, pCount, _d),
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

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{

			currentPipeline = handle;

			
			rbo->BindGraphicsPipeline(handle);
			

			rbo->BindDescriptorSets(descriptorId, frameNum, 1, 0, dynamicCount, dynamicOffsets);
		}

		for (uint32_t i = 0; i < objHeader->pushConstantCount; i++)
		{
			PushConstantArguments* args = &objHeader->pushArgs[i];
			rbo->PushConstantsCommand(args->offset, args->size, args->stage, args->data);
		}

		VKGraphicsPipelineObject* obj = (VKGraphicsPipelineObject*)objHeader;
		obj->Draw(rbo, frameNum, 1);
	
		
	}

	rbo->EndRenderPassCommand();

	currentPipeline = EntryHandle();
}

VKComputeGraph::VKComputeGraph(void* data, size_t dCount, size_t pCount, VKDevice* _d)
	: 
	VKGraph(data, dCount, pCount, _d)
{

}

void VKComputeGraph::DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum)
{
	uint32_t dynamicCount = static_cast<uint32_t>(dynamicOffsetCount);
	std::unique_lock lock(objectGuard);
	for (size_t i = 0; i < pipelineObjCount; i++)
	{

		EntryHandle objIndex = objects[i];

		VKPipelineObject* objHeader = dev->GetPipelineObject(objIndex);

		EntryHandle handle = objHeader->pipelineID;

		if (handle != currentPipeline)
		{

			currentPipeline = handle;


			rbo->BindComputePipeline(handle);

			if (descriptorId != EntryHandle())

				rbo->BindComputeDescriptorSets(descriptorId, frameNum, 1, 0, dynamicCount, dynamicOffsets);
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

void VKGraph::AddObject(EntryHandle obj)
{
	std::unique_lock lock(objectGuard);
	objects[pipelineObjCount++] = obj;
}

void VKGraph::AddDynamicOffset(uint32_t offset)
{
	std::unique_lock lock(objectGuard);
	dynamicOffsets[dynamicOffsetCount++] = offset;
}

VKGraph::VKGraph(void* data, size_t dCount, size_t pCount, VKDevice* _d)
	:
	dynamicOffsetSize(dCount), pipelineObjSize(pCount),
	pipelineObjCount(0), dynamicOffsetCount(0), dev(_d)
{
	uintptr_t head = (uintptr_t)data;
	dynamicOffsets = reinterpret_cast<uint32_t*>(head);
	head += sizeof(uint32_t) * dCount;
	objects = reinterpret_cast<EntryHandle*>(head);
}