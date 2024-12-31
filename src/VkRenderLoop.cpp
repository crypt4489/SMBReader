#include "VKRenderLoop.h"
#include "TextManager.h"
VKRenderLoop::VKRenderLoop(RenderInstance& _inst) : inst(_inst) 
{

}

void VKRenderLoop::RenderLoop(std::vector<GenericObject*>& objs)
{
	auto index = inst.BeginFrame();
	if (index == 0xFFFFFFFF) return;
	VkCommandBuffer cb = inst.GetCurrentCommandBuffer();
	auto frameNum = inst.GetCurrentFrame();
	for (auto& obj : objs)
	{
		obj->Draw(cb, frameNum);
	}
	TextManager::DrawTextTM(cb, frameNum);

	inst.SubmitFrame(index);
}