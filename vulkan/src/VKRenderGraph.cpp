#include "pch.h"

#include "VKRenderGraph.h"
#include <mutex>
VKRenderGraph::VKRenderGraph(EntryHandle _renderTargetIndex, void *data, size_t dCount, size_t pCount) 
	: renderTargetIndex(_renderTargetIndex), dynamicOffsetSize(dCount), pipelineObjSize(pCount),
	pipelineObjCount(0), dynamicOffsetCount(0)
{
	uintptr_t head = (uintptr_t)data;
	dynamicOffsets = reinterpret_cast<uint32_t*>(head);
	head += sizeof(uint32_t) * dCount;
	objects = reinterpret_cast<VKPipelineObject**>(head);
};

void VKRenderGraph::DrawScene(RecordingBufferObject& rbo, uint32_t frameNum)
{
	uint32_t dynamicCount = static_cast<uint32_t>(dynamicOffsetCount);
	
	std::unique_lock lock(objectGuard);
	for (size_t i = 0; i<pipelineObjCount; i++)
	{

		auto obj = objects[i];

		EntryHandle handle = obj->pipelineType;

		if (handle != currentPipeline)
		{
			currentPipeline = handle;
			rbo.BindPipeline(renderTargetIndex, handle);
			rbo.BindDescriptorSets(descriptorId, frameNum, 1, 0, dynamicCount, dynamicOffsets);
		}

		obj->Draw(rbo, frameNum, 1);
	}

	currentPipeline = EntryHandle();
}

void VKRenderGraph::AddObject(VKPipelineObject* obj)
{
	std::unique_lock lock(objectGuard);
	objects[pipelineObjCount++] = obj;
}

void VKRenderGraph::AddDynamicOffset(uint32_t offset)
{
	std::unique_lock lock(objectGuard);
	dynamicOffsets[dynamicOffsetCount++] = offset;
}