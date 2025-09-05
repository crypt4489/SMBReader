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




#define SWCATTACHMENTSOFFSET(x) sizeof(VkImage) * x
#define SWCQFCOFFSET(x, y) sizeof(size_t) * x * (1 + y)
#define SWCDEPENDSOFFSET(x) sizeof(uint32_t) * x

class VKSwapChain
{
public:
	VKSwapChain() = default;

	//VKSwapChain(VKDevice *_d, VkSurfaceKHR _surface, void *) : device(_d), surface(_surface) {}

	VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, 
		uint32_t _attachmentCount, uint32_t requestImages, 
		VK::Utils::SwapChainSupportDetails& swapChainSupport, size_t stages, size_t semaphoreperstage)
		: 
		device(_d), surface(_surface), attachmentCount(_attachmentCount), 
		semaphorePerStage(semaphoreperstage), numberOfStages(stages) {
		SetSwapChainProperties(swapChainSupport, requestImages);
	}

	void SetSWCLocalData(void* data)
	{
		headofperdata = reinterpret_cast<uintptr_t>(data);
	}

	size_t* GetDependenciesForImageIndex(uint32_t imageIndex);

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChainElements();

	void CreateSwapChain(
		uint32_t width, uint32_t height,
		uint32_t _renderPassIndex, std::vector<ImageIndex*>& attachmentIndices);

	void CreateSyncObject();

	void DestroySwapChain();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t imageIndex);

	void CreateSwapChainDependency(uint32_t imageIndex, uint32_t beforeDrawing, uint32_t present);

	void AddFramebufferAttachments(std::vector<ImageIndex*>& attachmentIndices)
	{
		uint32_t i = 0;
		uint32_t attachIndicesSize = static_cast<uint32_t>(attachmentIndices.size());


		uintptr_t headofattachments = headofperdata + SWCATTACHMENTSOFFSET(imageCount);

		uintptr_t* attaches = reinterpret_cast<uintptr_t*>(headofattachments);

		uintptr_t outputaddr = headofattachments + sizeof(size_t) * imageCount;

		while (i < imageCount)
		{
			attaches[i] = outputaddr;

			size_t* output = reinterpret_cast<size_t*>(outputaddr);

			for (uint32_t j = 0; j < attachIndicesSize; j++)
			{
				output[j] = reinterpret_cast<size_t>(attachmentIndices[j]);
			}
			outputaddr += sizeof(size_t) * attachmentCount;
			i++;
			
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
	uint32_t renderTargetIndex = ~0ui32;

	VkSurfaceFormatKHR swapChainImageFormat;
	VkPresentModeKHR presentMode;
	VkExtent2D swapChainExtent;

	uint32_t attachmentCount;
	uint32_t imageCount;
	uint32_t queueFamiliesCacheCount;

	size_t semaphorePerStage;
	size_t numberOfStages;

	VkSharingMode queueSharing;
	VkSurfaceTransformFlagBitsKHR preTransform;

	uintptr_t headofperdata;
	
	VkSurfaceKHR surface; //need one to draw to
	VKDevice* device; //owner of this swapchain
};

