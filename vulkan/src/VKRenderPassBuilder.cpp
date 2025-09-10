#include "VKRenderPassBuilder.h"
#include "VKDevice.h"
VKRenderPassBuilder::VKRenderPassBuilder(VKDevice* d, uint32_t numberofattachments, uint32_t numberofsubpassdependency, uint32_t numberofsubpassdescriptions)
	:
	device(d),
	totalAttachmentCount(numberofattachments),
	subpassdepcount(numberofsubpassdependency),
	subpassdesccount(numberofsubpassdescriptions),
	subpassdepcounter(0),
	subpassdesccounter(0),
	attachmentCounter(0),
	attachments(reinterpret_cast<VkAttachmentDescription*>(d->AllocFromDeviceCache(sizeof(VkAttachmentDescription) * numberofattachments))),
	subpassDependencies(reinterpret_cast<VkSubpassDependency*>(d->AllocFromDeviceCache(sizeof(VkSubpassDependency) * numberofsubpassdependency))),
	subpassDescription(reinterpret_cast<VkSubpassDescription*>(d->AllocFromDeviceCache(sizeof(VkSubpassDescription) * numberofsubpassdescriptions)))
{

}

void VKRenderPassBuilder::CreateAttachment(VKRenderPassAttachmentType type, VkFormat format,
	VkSampleCountFlagBits sampleCount, VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp,
	VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout,
	VkImageLayout finalLayout, uint32_t binding)
{
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = format;
	attachmentDescription.samples = sampleCount;
	attachmentDescription.loadOp = loadOp;
	attachmentDescription.storeOp = storeOp;
	attachmentDescription.stencilLoadOp = stencilLoadOp;
	attachmentDescription.stencilStoreOp = stencilStoreOp;
	attachmentDescription.initialLayout = initialLayout;
	attachmentDescription.finalLayout = finalLayout;

	VkImageLayout refLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	switch (type)
	{
	case COLORATTACH:
	case COLORRESOLVEATTACH:
		refLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case DEPTHSTENCILATTACH:
		refLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		break;
	}

	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = binding;
	attachmentRef.layout = refLayout;

	auto iter = attachmentsMap.find(type);
	if (iter == std::end(attachmentsMap))
	{
		attachmentsMap[type] = { {attachmentDescription}, {attachmentRef} };
		return;
	}
	iter->second.first.push_back(attachmentDescription);
	iter->second.second.push_back(attachmentRef);
}

void VKRenderPassBuilder::CreateSubPassDependency(uint32_t srcSubpass, uint32_t dstSubpass,
	VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessFlags,
	VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessFlags)
{

	VkSubpassDependency dependency{};
	dependency.srcSubpass = srcSubpass;
	dependency.dstSubpass = dstSubpass;
	dependency.srcStageMask = srcStageMask;
	dependency.srcAccessMask = srcAccessFlags;
	dependency.dstStageMask = dstStageMask;
	dependency.dstAccessMask = dstAccessFlags;
	subpassDependencies[subpassdepcounter++] = dependency;
}

void VKRenderPassBuilder::CreateSubPassDescription(VkPipelineBindPoint bindPoint, uint32_t firstColorAttachment,
	uint32_t numberOfColorAttachments, uint32_t colorResolveIndex, uint32_t depthAttachmentIndex)
{
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = bindPoint;
	subpass.colorAttachmentCount = numberOfColorAttachments;
	subpass.pColorAttachments = &attachmentsMap[COLORATTACH].second[firstColorAttachment];
	subpass.pResolveAttachments = &attachmentsMap[COLORRESOLVEATTACH].second[colorResolveIndex];
	subpass.pDepthStencilAttachment = &attachmentsMap[DEPTHSTENCILATTACH].second[depthAttachmentIndex];
	subpassDescription[subpassdesccounter++] = subpass;
}

void VKRenderPassBuilder::CreateInfo()
{
	for (auto attach : attachmentsMap)
	{
		auto size = attach.second.first.size();
		for (auto i = 0; i < size; i++)
		{
			attachments[attach.second.second[i].attachment] = attach.second.first[i];
		}
	}

	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = totalAttachmentCount;
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = subpassdesccount;
	createInfo.pSubpasses = subpassDescription;
	createInfo.dependencyCount = subpassdepcount;
	createInfo.pDependencies = subpassDependencies;
}