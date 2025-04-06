#pragma once

#include "VKPipelineCache.h"
#include "VKPipelineObject.h"
#include "TextManager.h"


class VKRenderGraph
{
public:

	VKRenderGraph() = delete;
	VKRenderGraph(VkRenderPass rp, VKPipelineCache *c) : inst(rp), cache(c) {};

	void DrawScene( 
		VkCommandBuffer& cb, 
		uint32_t frameNum,
		std::vector<VKPipelineObject*>& objs)
	{
		for (auto& obj : objs)
		{
			std::string name = obj->GetPipelineType();
			if (name != currentPipeline)
			{
				currentPipeline = name;
				PipelineCacheObject co = cache->GetPipelineFromCache(name);
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, co.pipeline);
			}
			obj->Draw(cb, frameNum);
		}

		PipelineCacheObject co = cache->GetPipelineFromCache("text");
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, co.pipeline);
		TextManager::DrawTextTM(cb, frameNum);
		currentPipeline.clear();
	}

private:
	
	VkRenderPass& inst;
	VKPipelineCache *cache;
	std::string currentPipeline = "";
	
};

