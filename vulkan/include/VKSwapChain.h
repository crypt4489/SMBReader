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

struct VKSwapChain
{

	VKSwapChain() = default;

	VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, DeviceOwnedAllocator* allocator,
		uint32_t requestImages, uint32_t maxFramesInFlight,
		VK::Utils::SwapChainSupportDetails& swapChainSupport);

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChain(
		uint32_t width, uint32_t height);

	void CreateSyncObject();

	EntryHandle* CreateSwapchainViews();

	void ResetSwapChain();

	void DestroySwapChain();

	void DestroySyncObject();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t acquireSemaphore);

	uint32_t AcquireNextSwapChainImage2(uint64_t _timeout, uint32_t acquireSemaphore);

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
	VkPresentModeKHR presentMode;
	VkExtent2D swapChainExtent;

	uint32_t imageCount;
	uint32_t queueFamiliesCacheCount;

	VkSharingMode queueSharing;
	VkSurfaceTransformFlagBitsKHR preTransform;

	VkImage* images;
	uint32_t queueFamiliesCache[2];
	EntryHandle* waitSemaphores;
	EntryHandle* signalSemaphores;
	EntryHandle* imageViews;
	EntryHandle* presentationFences;

	VkSurfaceKHR surface; //need one to draw to
	VKDevice* device; //owner of this swapchain

	uint32_t maxFrameInFlight;
	
};

