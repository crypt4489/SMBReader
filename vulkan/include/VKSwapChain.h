#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

class VKFrameBufferAttachments
{
public:
	VKFrameBufferAttachments() = default;
	VKFrameBufferAttachments(uint32_t size) {
		attachments.resize(size);
	}

	std::vector<ImageIndex*> attachments;
};

class VKSwapChainDependencies
{
public:

	VKSwapChainDependencies() = default;

	void AddIndicesForImage(uint32_t iIndex, std::vector<std::vector<uint32_t>>& depenencies)
	{
		if (chains.count(iIndex))
		{
			throw std::runtime_error(std::format("Trying to recreate the swap chain dependencies for image %d", iIndex));
		}
		chains[iIndex] = depenencies;
	}

	std::unordered_map<uint32_t, std::vector<std::vector<uint32_t>>> chains;
};

class VKSwapChain
{
public:
	VKSwapChain() = default;

	VKSwapChain(VKDevice *_d, VkSurfaceKHR _surface) : device(_d), surface(_surface) {}

	VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, uint32_t _attachmentCount)
		: device(_d), surface(_surface), attachmentCount(_attachmentCount) {}

	void SetDeviceAndSurface(VKDevice *_device, VkSurfaceKHR _surface)
	{
		this->device = _device;
		this->surface = _surface;
	}

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChainElements();

	void CreateSwapChain(
		uint32_t width, uint32_t height,
		uint32_t _renderPassIndex, std::vector<ImageIndex*>& attachmentIndices);

	void DestroySwapChain();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t imageIndex);

	void CreateSwapChainDependency(uint32_t imageIndex, uint32_t beforeDrawing, uint32_t present);

	void AddFramebufferAttachments(std::vector<ImageIndex*>& attachmentIndices)
	{
		uint32_t i = 0;
		uint32_t attachIndicesSize = static_cast<uint32_t>(attachmentIndices.size());
		while (i < imageCount)
		{
			auto& fbref = attachments[i++];
			for (uint32_t j = 0; j < attachIndicesSize; j++) fbref.attachments[j] = attachmentIndices[j];
		}
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}


	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
	
			VkExtent2D actualExtent = {
				width,
				height
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkExtent2D chooseSwapExtent(uint32_t width, uint32_t height) {
		

		VkExtent2D actualExtent = {
			width,
			height
		};



		return actualExtent;
		
	}

	VkFormat GetSwapChainFormat() const
	{
		return swapChainImageFormat.format;
	}


	uint32_t GetSwapChainHeight() const
	{
		return swapChainExtent.height;
	}

	uint32_t GetSwapChainWidth() const
	{
		return swapChainExtent.width;
	}


	VkSwapchainKHR swapChain = VK_NULL_HANDLE;

	std::vector<VkImage> swapChainImages;
	std::vector<ImageIndex> swapChainImageViews{};
	std::vector<VKFrameBufferAttachments> attachments;
	uint32_t renderTargetIndex = ~0ui32;

	VkSurfaceFormatKHR swapChainImageFormat;
	VkPresentModeKHR presentMode;
	VkExtent2D swapChainExtent;

	uint32_t attachmentCount;
	uint32_t imageCount;

	VkSharingMode queueSharing;
	VkSurfaceTransformFlagBitsKHR preTransform;
	std::vector<uint32_t> queueFamiliesCache;
	
	VKSwapChainDependencies dependencies;
	VkSurfaceKHR surface; //need one to draw to
	VKDevice* device; //owner of this swapchain
};

