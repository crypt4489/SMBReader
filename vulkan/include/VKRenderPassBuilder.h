#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"

struct VKRenderPassBuilder
{

	enum VKRenderPassAttachmentType
	{
		COLORATTACH = 0,
		COLORRESOLVEATTACH = 1,
		DEPTHSTENCILATTACH = 2,
		MAXATTACHMENT
	};

	VKRenderPassBuilder(VKDevice* d, uint32_t numberofattachments, uint32_t numberofsubpassdependency, uint32_t numberofsubpassdescriptions);

	void CreateAttachment(VKRenderPassAttachmentType type, VkFormat format,
		VkSampleCountFlagBits sampleCount, VkAttachmentLoadOp loadOp,
		VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp,
		VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout,
		VkImageLayout finalLayout, uint32_t binding);

	void CreateSubPassDependency(uint32_t srcSubpass, uint32_t dstSubpass,
		VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessFlags,
		VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessFlags);

	void CreateSubPassDescription(VkPipelineBindPoint bindPoint, uint32_t firstColorAttachment,
		uint32_t numberOfColorAttachments, uint32_t colorResolveIndex, uint32_t depthAttachmentIndex);

	void CreateInfo();

	uint32_t subpassdesccount, subpassdesccounter;

	uint32_t subpassdepcount, subpassdepcounter;

	uint32_t totalAttachmentCount, attachmentCounter;

	VkAttachmentDescription* attachments;

	VkAttachmentReference* references;

	VkSubpassDescription* subpassDescription;

	VkSubpassDependency* subpassDependencies;

	VkRenderPassCreateInfo createInfo{};
	VKDevice* device;
};

