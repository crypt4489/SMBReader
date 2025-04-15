#pragma once

#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKPipelineCache.h"
#include "VKPipelineObject.h"
#include "TextManager.h"

#include <vector>
class VKRenderGraph
{
public:

	VKRenderGraph() = delete;
	VKRenderGraph(VkRenderPass rp, VKPipelineCache *c, VKDescriptorSetCache *dsc) : inst(rp), cache(c), dscache(dsc) {};

	void DrawScene( 
		VkCommandBuffer& cb, 
		uint32_t frameNum,
		std::vector<VKPipelineObject*>& objs,
		glm::mat4& view,
		glm::mat4& proj
		)
	{
		UpdateUniformBuffer(proj, view, frameNum);
		std::string mrp = "mainrenderpass";
		VkDescriptorSet set = dscache->GetDescriptorSetPerFrame(mrp, frameNum);
		RenderInstance* rend = ::VKRenderer::gRenderInstance;
		for (auto& obj : objs)
		{
			std::string name = obj->GetPipelineType();
			
			rend->UpdateDynamicGlobalBuffer(obj->perObjectShaderData, obj->perObjectShaderDataSize, obj->dynamicOffset, frameNum);

			if (name != currentPipeline)
			{
				currentPipeline = name;
				PipelineCacheObject co = cache->GetPipelineFromCache(name);
				vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, co.pipelineLayout, 0, 1, &set, 0, nullptr);
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, co.pipeline);
			}
			obj->Draw(cb, frameNum, 1);
		}

		PipelineCacheObject co = cache->GetPipelineFromCache("text");
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, co.pipeline);
		TextManager::DrawTextTM(cb, frameNum);
		currentPipeline.clear();
	}

	void CreateRenderPassDescriptorSet(VkDevice device, VkDescriptorPool pool, VKDescriptorLayoutCache *dlcache, uint32_t frames)
	{
		std::string mainrenderpass = "mainrenderpass";
		auto ref = dlcache->GetLayout("mainrenderpass");
		
		DescriptorSetBuilder dsb{};
		dsb.AllocDescriptorSets(device, pool, ref, frames);
		dsb.AddUniformBuffer(device, uniformBuffer, sizeof(glm::mat4) * 2, 0, frames, uniformOffset);
		dscache->AddDesciptorSet(mainrenderpass, dsb.descriptorSets);
		
	}

	void CreateUniformBuffers(RenderInstance *inst, uint32_t frames)
	{
		std::tie(uniformBuffer, memoryMapped) = inst->GetPageFromUniformBuffer(sizeof(glm::mat4) * 2 * frames, uniformOffset);
	}

	void UpdateUniformBuffer(glm::mat4& proj, glm::mat4& view, uint32_t frameNum)
	{
		proj[1][1] *= -1;
		char* data = ((char*)memoryMapped) + (frameNum * 2 * sizeof(glm::mat4));
		memcpy(data, &view, sizeof(glm::mat4));
		memcpy(data + sizeof(glm::mat4), &proj, sizeof(glm::mat4));
	}

private:
	
	VkRenderPass& inst;
	VKPipelineCache* cache;
	VKDescriptorSetCache* dscache;
	std::string currentPipeline = "";
	VkBuffer uniformBuffer;
	VkDeviceSize uniformOffset;
	void* memoryMapped;
};

