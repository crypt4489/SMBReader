#pragma once

#include "GenericObject.h"
#include "RenderInstance.h"
#include "VKPipelineObject.h"

class VKRenderLoop
{
public:

	VKRenderLoop() = delete;
	VKRenderLoop(RenderInstance& _inst) : inst(_inst) {}

	void RenderLoop(std::vector<GenericObject*> &objs)
	{
		auto index = inst.BeginFrame();
		if (index == 0xFFFFFFFF) return;
		VkCommandBuffer cb = inst.GetCurrentCommandBuffer();
		auto frameNum = inst.GetCurrentFrame();
		for (auto& obj : objs)
		{
			obj->Draw(cb, frameNum);
		}
		inst.SubmitFrame(index);
	}

/*
	void AddPipeline(VKPipelineObject *pipeline)
	{
		if (std::find(pipelinesInFlight.begin(), pipelinesInFlight.end(), pipeline) == std::end(pipelinesInFlight))
			pipelinesInFlight.push_back(pipeline);
	}

	void RemovePipeline(VKPipelineObject *pipeline)
	{
		auto n = std::erase(pipelinesInFlight, pipeline);
		if (!n) std::cout << "Removed pipeline not already added\n";
	}
*/
private:
	//std::vector<VKPipelineObject*> pipelinesInFlight;
	RenderInstance& inst;
};

