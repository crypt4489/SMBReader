#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

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

	void SetDeviceAndSurface(VKDevice *_device, VkSurfaceKHR _surface)
	{
		this->device = _device;
		this->surface = _surface;
	}

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport);

	void CreateSwapChain(uint32_t width, uint32_t height, uint32_t* queueFamilyIndices, uint32_t numberOfQueueFamilies);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChainImageViews();

	void CreateFrameBuffers(VkRenderPass renderPass, std::vector<VkImageView>& attachmentViews);

	void DestroySwapChain();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t imageIndex);

	void CreateSwapChainDependency(uint32_t imageIndex, uint32_t beforeDrawing, uint32_t present);

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

		//actualExtent.width = std::clamp(actualExtent.width, minMaxImageDimensions[0].width, minMaxImageDimensions[1].width);
		//actualExtent.height = std::clamp(actualExtent.height, minMaxImageDimensions[0].height, minMaxImageDimensions[1].height);

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
	std::vector<uint32_t> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkSurfaceFormatKHR swapChainImageFormat;
	VkExtent2D swapChainExtent;

	VkPresentModeKHR presentMode;
	uint32_t imageCount;
	VkSharingMode queueSharing;
	VkSurfaceTransformFlagBitsKHR preTransform;
	std::vector<uint32_t> queueFamiliesCache;
	VKSwapChainDependencies dependencies;
 
	VkSurfaceKHR surface; //need one to draw to
	VKDevice* device; //owner of this swapchain
};

