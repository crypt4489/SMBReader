#include "VKRenderGraph.h"
#include <mutex>
VKRenderGraph::VKRenderGraph(EntryHandle _renderTargetIndex) : renderTargetIndex(_renderTargetIndex) {};

void VKRenderGraph::DrawScene(RecordingBufferObject& rbo, uint32_t frameNum)
{
	uint32_t dynamicCount = static_cast<uint32_t>(dynamicOffsets.size());
	
	std::lock_guard lock(objectGuard);
	for (auto& obj : objects)
	{
		std::string name = obj->pipelineType;

		if (name != currentPipeline)
		{
			currentPipeline = name;
			rbo.BindPipeline(renderTargetIndex, name);
			rbo.BindDescriptorSets(descriptorname, frameNum, 1, 0, dynamicCount, dynamicOffsets.data());
		}

		obj->Draw(rbo, frameNum, 1);
	}

	currentPipeline.clear();
}

void VKRenderGraph::AddObject(VKPipelineObject* obj)
{
	std::lock_guard lock(objectGuard);
	objects.push_back(obj);
}