#pragma once

#include "VKPipelineObject.h"
#include "VKDevice.h"
#include "ThreadManager.h"

#include <vector>
#include <string>


class VKRenderGraph
{
public:

	VKRenderGraph() = default;
	VKRenderGraph(VKRenderGraph&& other) = delete;
	
	VKRenderGraph(const VKRenderGraph& other) = delete;
	VKRenderGraph& operator=(const VKRenderGraph& other) = delete;

	VKRenderGraph& operator=(VKRenderGraph&& other) = delete;

	VKRenderGraph(EntryHandle _renderTargetIndex, void *data, size_t dCount, size_t pCount);

	void DrawScene(RecordingBufferObject& rbo, uint32_t frameNum);

	void AddObject(VKPipelineObject* obj);

	void AddDynamicOffset(uint32_t offset);
	
	EntryHandle renderTargetIndex;

	std::string currentPipeline = "";

	std::string descriptorname = "";

	uint32_t* dynamicOffsets;

	SharedExclusiveFlag objectGuard;

	VKPipelineObject** objects;

	size_t dynamicOffsetCount, dynamicOffsetSize;
	size_t pipelineObjCount, pipelineObjSize;
};

