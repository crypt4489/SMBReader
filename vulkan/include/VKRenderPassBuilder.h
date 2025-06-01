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
		VkImageLayout finalLayout, uint32_t binding)
	{
		totalAttachmentCount++;
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

	void CreateSubPassDependency(uint32_t srcSubpass, uint32_t dstSubpass,
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
		subpassDependencies.push_back(dependency);
	}

	void CreateSubPassDescription(VkPipelineBindPoint bindPoint, uint32_t firstColorAttachment, 
		uint32_t numberOfColorAttachments, uint32_t colorResolveIndex, uint32_t depthAttachmentIndex)
	{
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = bindPoint;
		subpass.colorAttachmentCount = numberOfColorAttachments;
		subpass.pColorAttachments = &attachmentsMap[COLORATTACH].second[firstColorAttachment];
		subpass.pResolveAttachments = &attachmentsMap[COLORRESOLVEATTACH].second[colorResolveIndex];
		subpass.pDepthStencilAttachment = &attachmentsMap[DEPTHSTENCILATTACH].second[depthAttachmentIndex];
		subpassDescription.push_back(subpass);
	}

	void CreateInfo()
	{
		attachments.resize(totalAttachmentCount);
		for (auto attach : attachmentsMap)
		{
			auto size = attach.second.first.size();
			for (auto i = 0; i<size; i++)
			{
				attachments[attach.second.second[i].attachment] = attach.second.first[i];
			}
		}
		
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.subpassCount = static_cast<uint32_t>(subpassDescription.size());
		createInfo.pSubpasses = subpassDescription.data();
		createInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
		createInfo.pDependencies = subpassDependencies.data();
	}
	std::vector<VkAttachmentDescription> attachments;
	std::unordered_map<VKRenderPassAttachmentType, std::pair<std::vector<VkAttachmentDescription>, std::vector<VkAttachmentReference>>> attachmentsMap;
	uint32_t totalAttachmentCount;
	std::vector<VkSubpassDescription> subpassDescription;
	std::vector<VkSubpassDependency> subpassDependencies;
	VkRenderPassCreateInfo createInfo{};
};

