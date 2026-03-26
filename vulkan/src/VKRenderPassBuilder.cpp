#include "pch.h"

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
	attachments(reinterpret_cast<VkAttachmentDescription*>(d->AllocFromDeviceCache((sizeof(VkAttachmentDescription) * numberofattachments) + (sizeof(VkAttachmentReference) * numberofattachments)))),
	subpassDependencies(reinterpret_cast<VkSubpassDependency*>(d->AllocFromDeviceCache(sizeof(VkSubpassDependency) * numberofsubpassdependency))),
	subpassDescription(reinterpret_cast<VkSubpassDescription*>(d->AllocFromDeviceCache(sizeof(VkSubpassDescription) * numberofsubpassdescriptions)))
{
	references = (VkAttachmentReference*)std::next(attachments, numberofattachments);
}

void VKRenderPassBuilder::SetSampleCount(uint32_t attachmentIndex, VkSampleCountFlagBits sampleFlags)
{
	attachments[attachmentIndex].samples = sampleFlags;
}

void VKRenderPassBuilder::CreateAttachment(VkImageLayout imageReferenceLayout, VkFormat format,
	VkSampleCountFlagBits sampleCount, VkAttachmentLoadOp loadOp,
	VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp,
	VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout,
	VkImageLayout finalLayout, uint32_t binding, uint32_t insertionIndex)
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

	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = binding;
	attachmentRef.layout = imageReferenceLayout;

	attachments[binding] = attachmentDescription;
	references[insertionIndex] = attachmentRef;
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

void VKRenderPassBuilder::CreateSubPassDescription(VkPipelineBindPoint bindPoint, uint32_t numberOfColorAttachments, uint32_t numberResolveAttachments, uint32_t numberDepthStencilAttachments)
{
	VkSubpassDescription subpass{};

	subpass.pipelineBindPoint = bindPoint;
	subpass.colorAttachmentCount = numberOfColorAttachments;
	subpass.pColorAttachments = &references[0];

	uint32_t offset = numberOfColorAttachments;

	if (numberDepthStencilAttachments)
		subpass.pDepthStencilAttachment = &references[offset];

	offset += numberDepthStencilAttachments;

	if (numberResolveAttachments)
		subpass.pResolveAttachments = &references[offset];

	if (0)
		subpass.pInputAttachments = nullptr;

	subpassDescription[subpassdesccounter++] = subpass;
}

void VKRenderPassBuilder::CreateInfo()
{
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = totalAttachmentCount;
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = subpassdesccount;
	createInfo.pSubpasses = subpassDescription;
	createInfo.dependencyCount = subpassdepcount;
	createInfo.pDependencies = subpassDependencies;
}