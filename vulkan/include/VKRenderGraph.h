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

	VKRenderGraph(uint32_t _renderTargetIndex);

	void DrawScene(RecordingBufferObject& rbo, uint32_t frameNum);

	void AddObject(VKPipelineObject* obj);
	
	uint32_t renderTargetIndex;

	std::string currentPipeline = "";

	std::string descriptorname = "";

	std::vector<uint32_t> dynamicOffsets;

	SharedExclusiveFlag objectGuard;

	std::vector<VKPipelineObject*> objects;
};

