#pragma once
#include "VKUtilities.h"
#include <unordered_map>
#include <vector>
class VKRenderPassBuilder
{
public:
	enum class VKRenderPassAttachmentType
	{
		COLORATTACH = 0,
		COLORRESOLVEATTACH = 1,
		DEPTHSTENCILATTACH = 2
	};

	using enum VKRenderPassAttachmentType;

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

	std::vector<VkAttachmentDescription> attachments;
	std::unordered_map<VKRenderPassAttachmentType, std::pair<std::vector<VkAttachmentDescription>, std::vector<VkAttachmentReference>>> attachmentsMap;
	uint32_t totalAttachmentCount;
	std::vector<VkSubpassDescription> subpassDescription;
	std::vector<VkSubpassDependency> subpassDependencies;
	VkRenderPassCreateInfo createInfo{};
};

