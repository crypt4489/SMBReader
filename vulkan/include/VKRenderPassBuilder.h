#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"

struct VKRenderPassBuilder
{
	VKRenderPassBuilder(VKDevice* d, uint32_t numberofattachments, uint32_t numberofsubpassdependency, uint32_t numberofsubpassdescriptions);

	void CreateAttachment(VkImageLayout imageReferenceLayout, VkFormat format,
		VkSampleCountFlagBits sampleCount, VkAttachmentLoadOp loadOp,
		VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp,
		VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout,
		VkImageLayout finalLayout, uint32_t binding, uint32_t insertionIndex);

	void CreateSubPassDependency(uint32_t srcSubpass, uint32_t dstSubpass,
		VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessFlags,
		VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessFlags);

	void CreateSubPassDescription(VkPipelineBindPoint bindPoint, uint32_t numberOfColorAttachments, uint32_t numberResolveAttachments, uint32_t numberDepthStencilAttachments);

	void CreateInfo();

	void SetSampleCount(uint32_t attachmentIndex, VkSampleCountFlagBits sampleFlags);

	VkAttachmentDescription* attachments;
	VkAttachmentReference* references;
	VkSubpassDescription* subpassDescription;
	VkSubpassDependency* subpassDependencies;
	VKDevice* device;
	VkRenderPassCreateInfo createInfo{};
	uint32_t subpassdesccount;
	uint32_t subpassdesccounter;
	uint32_t subpassdepcount;
	uint32_t subpassdepcounter;
	uint32_t totalAttachmentCount;
	uint32_t attachmentCounter;
};

