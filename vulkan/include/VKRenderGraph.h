#pragma once

#include "RenderInstance.h"
#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKPipelineObject.h"
#include "VKPipelineCache.h"


#include <vector>
#include <string>
class VKRenderGraph
{
public:

	VKRenderGraph() = delete;
	VKRenderGraph(VkRenderPass rp, VKPipelineCache* c, VKDescriptorSetCache* dsc);

	void DrawScene(
		VkCommandBuffer& cb,
		uint32_t frameNum,
		std::vector<VKPipelineObject*>& objs,
		glm::mat4& view,
		glm::mat4& proj
	);

	void CreateRenderPassDescriptorSet(VkDevice device, VkDescriptorPool pool, VKDescriptorLayoutCache* dlcache, uint32_t frames);

	void CreateUniformBuffers(RenderInstance* inst, uint32_t frames);

	void UpdateUniformBuffer(glm::mat4& proj, glm::mat4& view, uint32_t frameNum);

private:
	
	VkRenderPass& inst;
	VKPipelineCache* cache;
	VKDescriptorSetCache* dscache;
	std::string currentPipeline = "";
	OffsetIndex uniformOffset;
};

