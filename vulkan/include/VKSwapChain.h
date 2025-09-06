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

	size_t CalculateSwapChainMemoryUsage();

	EntryHandle* GetDependenciesForImageIndex(uint32_t imageIndex);

	void SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount);

	void RecreateSwapChain(uint32_t width, uint32_t height);

	void CreateSwapChainElements();

	void CreateSwapChain(
		uint32_t width, uint32_t height,
		EntryHandle _renderPassIndex, std::vector<EntryHandle*>& attachmentIndices);

	void CreateSyncObject();

	void DestroySwapChain();

	uint32_t AcquireNextSwapChainImage(uint64_t _timeout, uint32_t imageIndex);

	void CreateSwapChainDependency(uint32_t imageIndex, EntryHandle beforeDrawing, EntryHandle present);

	void AddFramebufferAttachments(std::vector<EntryHandle*>& attachmentIndices);

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
	EntryHandle renderTargetIndex;

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

