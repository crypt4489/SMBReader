#pragma once

#include "RenderInstance.h"
#include "VKPipelineObject.h"

#include <vector>
#include <string>
class VKRenderGraph
{
public:

	VKRenderGraph() = delete;
	VKRenderGraph(uint32_t _renderTargetIndex);

	void DrawScene(
		uint32_t frameNum,
		std::vector<VKPipelineObject*>& objs,
		glm::mat4& view,
		glm::mat4& proj
	);

	void CreateRenderPassDescriptorSet(uint32_t frames);

	void CreateUniformBuffers(RenderInstance* inst, uint32_t frames);

	void UpdateUniformBuffer(glm::mat4& proj, glm::mat4& view, uint32_t frameNum);

private:
	
	uint32_t renderTargetIndex;
	
	OffsetIndex uniformOffset;

	std::string currentPipeline = "";
};

