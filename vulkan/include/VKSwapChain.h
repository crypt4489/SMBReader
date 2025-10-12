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




#define SWCQFCOFFSET(x) (sizeof(VkImage) * x)
#define SWCDEPENDSOFFSET(x) (sizeof(uint32_t) * x)
#define SWCRENDERTARGETSOFFSET(x, y, z) (sizeof(EntryHandle) * (x * ((y * z) + 1)))

class VKSwapChain
{
public:
	VKSwapChain() = default;

	VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, 
		uint32_t _attachmentCount, uint32_t requestImages, 
		VK::Utils::SwapChainSupportDetails& swapChainSupport, size_t stages, size_t semaphoreperstage, uint32_t _renderTargetCount)
		: 
		device(_d), surface(_surface), attachmentCount(_attachmentCount), 
		semaphorePerStage(semaphoreperstage), numberOfStages(stages), renderTargetCount(_renderTargetCount) {
		SetSwapChainProperties(swapChainSupport, requestImages);
	}

	void SetSwapChainData(void* data);

	size_t CalculateSwapChainMemoryUsage();

	EntryHandle* GetDependenciesForImageIndex(uint32_t imageIndex);

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChainElements(uint32_t index, uint32_t aAttachmentCount, EntryHandle* attachments, EntryHandle* imageViews);

	void CreateRenderTarget(uint32_t index, EntryHandle renderPassIndex);

	

	void CreateSwapChain(
		uint32_t width, uint32_t height);

	void CreateSyncObject();

	EntryHandle* CreateSwapchainViews();

	void ResetSwapChain();

	void DestroySwapChain();

	void DestroySyncObject();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t imageIndex);

	void AddFramebufferAttachments(EntryHandle** attachmentIndices, uint32_t attachIndicesSize);

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
	EntryHandle *renderTargetIndex;
	uint32_t renderTargetCount;

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

