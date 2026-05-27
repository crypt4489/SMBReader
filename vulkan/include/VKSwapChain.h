#pragma once

#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"

struct VKSwapChain
{
	VKSwapChain() = default;

	VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, DeviceOwnedAllocator* allocator,
		uint32_t requestImages, uint32_t maxFramesInFlight,
		VK::Utils::SwapChainSupportDetails& swapChainSupport, VkFormat requestedFormat);

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount, VkFormat requestedFormat);

	int RecreateSwapChain(uint32_t width, uint32_t height);

	int CreateSwapChain(uint32_t width, uint32_t height, EntryHandle graphicsTransferQueue, EntryHandle presentQueue);

	void CreateSyncObject();

	EntryHandle* CreateSwapchainViews();

	void ResetSwapChain();

	void DestroySwapChain();

	void DestroySyncObject();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, VkSemaphore acquireSemaphore);

	uint32_t AcquireNextSwapChainImage2(uint64_t _timeout, VkSemaphore acquireSemaphore, uint32_t currentFrameInFlight);

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

	void ReleaseImageMaintenance(uint32_t imageIndex);

	void Wait();

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR swapChainImageFormat;
	VkExtent2D swapChainExtent;
	VkImage* images;
	EntryHandle* imageViews;
	EntryHandle* presentationFences;
	VkSurfaceKHR surface;
	VKDevice* device; 

	uint32_t imageCount;
	uint32_t maxFrameInFlight;
	uint32_t queueFamiliesCacheCount;
	uint32_t queueFamiliesCache[2];
	VkSharingMode queueSharing;
	VkPresentModeKHR presentMode;
	VkSurfaceTransformFlagBitsKHR preTransform;
};

