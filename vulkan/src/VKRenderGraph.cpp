#include "VKRenderGraph.h"

#include "TextManager.h"
VKRenderGraph::VKRenderGraph(uint32_t _renderTargetIndex) : renderTargetIndex(_renderTargetIndex) {};

void VKRenderGraph::DrawScene(
	uint32_t frameNum,
	std::vector<VKPipelineObject*>& objs,
	glm::mat4& view,
	glm::mat4& proj
)
{
	RenderInstance* rend = VKRenderer::gRenderInstance;
	UpdateUniformBuffer(proj, view, frameNum);
	auto rbo = rend->GetCurrentCommandBuffer();
	std::string mrp = "mainrenderpass";
	for (auto& obj : objs)
	{
		std::string name = obj->GetPipelineType();

		rend->UpdateDynamicGlobalBuffer(obj->perObjectShaderData, obj->perObjectShaderDataSize, obj->dynamicOffset, frameNum);

		if (name != currentPipeline)
		{
			currentPipeline = name;
			rbo.BindPipeline(renderTargetIndex, name);
			rbo.BindDescriptorSets(mrp, frameNum, 1, 0, 0, nullptr);
		}

		obj->Draw(rbo, frameNum, 1);
	}

	rbo.BindPipeline(renderTargetIndex, "text");
	TextManager::DrawTextTM(rbo, frameNum);
	currentPipeline.clear();
}

void VKRenderGraph::CreateRenderPassDescriptorSet(uint32_t frames)
{
	std::string mainrenderpass = "mainrenderpass";
	RenderInstance* rend = VKRenderer::gRenderInstance;

	DescriptorSetBuilder dsb = rend->CreateDescriptorSet(mainrenderpass, frames);
	dsb.AddUniformBuffer(VKRenderer::gRenderInstance->GetDynamicUniformBuffer(), sizeof(glm::mat4) * 2, 0, frames, uniformOffset);
	dsb.AddDescriptorsToCache(mainrenderpass);

}

void VKRenderGraph::CreateUniformBuffers(RenderInstance* inst, uint32_t frames)
{
	uniformOffset = inst->GetPageFromUniformBuffer(sizeof(glm::mat4) * 2 * frames, 16);
}

void VKRenderGraph::UpdateUniformBuffer(glm::mat4& proj, glm::mat4& view, uint32_t frameNum)
{
	proj[1][1] *= -1;
	struct {
		glm::mat4 view;
		glm::mat4 proj;
	} what = { view,  proj };
	VKRenderer::gRenderInstance->UpdateDynamicGlobalBuffer(&what, sizeof(glm::mat4) * 2, uniformOffset, frameNum);
}