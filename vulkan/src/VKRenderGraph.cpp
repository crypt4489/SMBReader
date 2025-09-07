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
	
	std::lock_guard lock(objectGuard);
	for (size_t i = 0; i<pipelineObjCount; i++)
	{

		auto obj = objects[i];

		std::string name = obj->pipelineType;

		if (name != currentPipeline)
		{
			currentPipeline = name;
			rbo.BindPipeline(renderTargetIndex, name);
			rbo.BindDescriptorSets(descriptorname, frameNum, 1, 0, dynamicCount, dynamicOffsets);
		}

		obj->Draw(rbo, frameNum, 1);
	}

	currentPipeline.clear();
}

void VKRenderGraph::AddObject(VKPipelineObject* obj)
{
	std::lock_guard lock(objectGuard);
	objects[pipelineObjCount++] = obj;
}

void VKRenderGraph::AddDynamicOffset(uint32_t offset)
{
	std::lock_guard lock(objectGuard);
	dynamicOffsets[dynamicOffsetCount++] = offset;
}