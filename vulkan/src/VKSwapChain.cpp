#include "pch.h"

#include "VKSwapChain.h"
#include "VKDevice.h"
#include "VKInstance.h"

#include <algorithm>

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, size_t formatCount, VkFormat requestedFormat) 
{
	for (uint32_t i = 0; i < formatCount; i++)
	{
		VkSurfaceFormatKHR availableFormat = availableFormats[i];
		if (availableFormat.format == requestedFormat && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
		{
			return availableFormat;
		}
	}

	return { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_MAX_ENUM_KHR };
}

static VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, size_t presentCount) 
{
	for (uint32_t i = 0; i < presentCount; i++)
	{ 
		VkPresentModeKHR availablePresentMode = availablePresentModes[i];
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}


static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) 
{	
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
	{
		return capabilities.currentExtent;
	}
	else 
	{
		VkExtent2D actualExtent = {
			width,
			height
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

static VkExtent2D chooseSwapExtent(uint32_t width, uint32_t height) {

	VkExtent2D actualExtent = {
		width,
		height
	};

	return actualExtent;

}

static PFN_vkReleaseSwapchainImagesEXT vRelease = nullptr;


VKSwapChain::VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, uint32_t requestImages, uint32_t maxFrameInFlight,
	VK::Utils::SwapChainSupportDetails& swapChainSupport, VkFormat requestedFormat)
	:
	device(_d), surface(_surface), maxFrameInFlight(maxFrameInFlight) {

	if (!vRelease)
		vRelease = (PFN_vkReleaseSwapchainImagesEXT)vkGetDeviceProcAddr(device->device, "vkReleaseSwapchainImagesEXT");
	
	SetSwapChainProperties(swapChainSupport, requestImages, requestedFormat);


	images = reinterpret_cast<VkImage*>(device->AllocFromPerDeviceData(sizeof(VkImage) * imageCount));
	presentationFences = reinterpret_cast<EntryHandle*>(device->AllocFromPerDeviceData(sizeof(EntryHandle) * maxFrameInFlight));
	imageViews = reinterpret_cast<EntryHandle*>(device->AllocFromPerDeviceData(sizeof(EntryHandle) * imageCount));
}


void VKSwapChain::SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount, VkFormat requestedFormat)
{
	swapChainImageFormat = chooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount, requestedFormat);
	presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
	
	preTransform = swapChainSupport.capabilities.currentTransform;

	if (!_imageCount)
	{
		imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
	}
	else 
	{
		imageCount = std::min(std::max(_imageCount, swapChainSupport.capabilities.minImageCount), swapChainSupport.capabilities.maxImageCount);
	}
}


int VKSwapChain::CreateSwapChain(uint32_t width, uint32_t height, EntryHandle graphicsTransferQueue, EntryHandle presentQueue)
{
	swapChainExtent = chooseSwapExtent(width, height);
	
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = swapChainImageFormat.format;
	createInfo.imageColorSpace = swapChainImageFormat.colorSpace;
	createInfo.imageExtent = swapChainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	
	QueueManager* graphicsTransferManager = device->GetQueueManager(graphicsTransferQueue);

	if (!graphicsTransferManager)
		return -1;

	queueFamiliesCache[queueFamiliesCacheCount++] = graphicsTransferManager->queueFamilyIndex;

	if (graphicsTransferQueue != presentQueue)
	{
		QueueManager* presentManager = device->GetQueueManager(presentQueue);
		queueFamiliesCache[queueFamiliesCacheCount++] = presentManager->queueFamilyIndex;
	}

	queueSharing = createInfo.imageSharingMode = (queueFamiliesCacheCount > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = queueFamiliesCache;
	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain)) != VK_SUCCESS) 
	{
		device->AddDeviceErrorCode((MINOR_CODE_PACK(DEVICE_VK_TYPE_SWAPCHAIN_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED), vkRes);
		return -1;
	}
	
	VkImage* swcImages = images;

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	CreateSyncObject();

	return 0;
}

void VKSwapChain::CreateSyncObject()
{
	EntryHandle* fences = device->CreateFences(imageCount, VK_FENCE_CREATE_SIGNALED_BIT);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		presentationFences[i] = fences[i];
	}
}


int VKSwapChain::RecreateSwapChain(uint32_t width, uint32_t height)
{
	VkSurfaceCapabilitiesKHR caps;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->gpu, surface, &caps);
	width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
	height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
	swapChainExtent = { width, height };

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = swapChainImageFormat.format;
	createInfo.imageColorSpace = swapChainImageFormat.colorSpace;
	createInfo.imageExtent = swapChainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	createInfo.imageSharingMode = queueSharing;

	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = queueFamiliesCache;

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain)) != VK_SUCCESS) 
	{
		device->AddDeviceErrorCode((MINOR_CODE_PACK(DEVICE_VK_TYPE_SWAPCHAIN_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED), vkRes);
		return -1;
	}

	VkImage* swcImages = images;

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	return 0;
}

EntryHandle* VKSwapChain::CreateSwapchainViews()
{
	for (uint32_t i = 0; i < imageCount; i++) 
	{
		imageViews[i] = device->CreateImageView(images[i], 1, 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
	}

	return imageViews;
}

void VKSwapChain::Wait()
{
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		VkFence fence = device->GetFence(presentationFences[i]);
		vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX);
	}
}

void VKSwapChain::ResetSwapChain()
{
	Wait();

	if (swapChain) 
	{
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < imageCount; i++)
	{
		device->DestroyImageView(imageViews[i]);
	}
}

void VKSwapChain::ReleaseImageMaintenance(uint32_t imageIndex)
{
	VkReleaseSwapchainImagesInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
	info.imageIndexCount = 1;
	info.pImageIndices = &imageIndex;
	info.swapchain = swapChain;

	vRelease(device->device, &info);	
}

void VKSwapChain::DestroySwapChain()
{
	if (swapChain) 
	{
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < imageCount; i++)
	{
		device->DestroyImageView(imageViews[i]);
	}

	

	DestroySyncObject();

	device->FreeFromPerDeviceData(imageViews);
	device->FreeFromPerDeviceData(presentationFences);
	device->FreeFromPerDeviceData(images);
}

void VKSwapChain::DestroySyncObject()
{
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		device->DestroyFence(presentationFences[i]);
	}
}

uint32_t VKSwapChain::AcquireNextSwapChainImage(uint64_t _timeout, VkSemaphore acquireSemaphore)
{
	uint32_t imageIndex;

	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result != VK_SUCCESS) 
	{
		device->AddDeviceErrorCode((MINOR_CODE_PACK(DEVICE_VK_TYPE_SWAPCHAIN_FAILED) | DEVICE_VK_TYPE_ACQUIRE_IMAGE_FAILED), result);
		return ~0U;
	}

	return imageIndex;
}

uint32_t VKSwapChain::AcquireNextSwapChainImage2(uint64_t _timeout, VkSemaphore acquireSemaphore, uint32_t currentFrameInFlight)
{
	uint32_t imageIndex;

	VkFence fence = device->GetFence(presentationFences[currentFrameInFlight]);

	vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX);

	VkAcquireNextImageInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
	info.semaphore = acquireSemaphore;
	info.swapchain = swapChain;
	info.timeout = _timeout;
	info.deviceMask = 1;
	
	VkResult result = vkAcquireNextImage2KHR(device->device, &info, &imageIndex);

	if (result != VK_SUCCESS)
	{
		device->AddDeviceErrorCode((MINOR_CODE_PACK(DEVICE_VK_TYPE_SWAPCHAIN_FAILED) | DEVICE_VK_TYPE_ACQUIRE_IMAGE_FAILED), result);
		return ~0U;
	}

	return imageIndex;
}