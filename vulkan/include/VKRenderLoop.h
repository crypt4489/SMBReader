#pragma once

#include "GenericObject.h"
#include "RenderInstance.h"
#include "VKPipelineObject.h"

class VKRenderLoop
{
public:

	VKRenderLoop() = delete;
	VKRenderLoop(RenderInstance& _inst);

	void RenderLoop(std::vector<GenericObject*>& objs);

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

