#pragma once

#include "RenderInstance.h"
#include "VKPipelineObject.h"

class VKRenderLoop
{
public:

	VKRenderLoop() = default;

	void RenderLoop(RenderInstance* inst)
	{
		while (!inst->ShouldCloseWindow())
		{
			auto index = inst->BeginFrame();
			if (index == 0xFFFFFFFF) continue;
			VkCommandBuffer cb = inst->GetCurrentCommandBuffer();
			for (auto& pipe : pipelinesInFlight)
			{
				pipe->Draw(cb, 4, inst->GetCurrentFrame());
			}
			inst->SubmitFrame(index);
		}
		inst->WaitOnQueues();
	}


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

private:
	std::vector<VKPipelineObject*> pipelinesInFlight;
};

