#include "pch.h"

#include "VKSwapChain.h"
#include "VKDevice.h"
#include "VKInstance.h"
#include "VKUtilities.h"


static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, size_t formatCount) {
	for (uint32_t i = 0; i < formatCount; i++)
	{
		VkSurfaceFormatKHR availableFormat = availableFormats[i];
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, size_t presentCount) {


	for (uint32_t i = 0; i < presentCount; i++)
	{ 
		VkPresentModeKHR availablePresentMode = availablePresentModes[i];
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}


static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
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

static VkExtent2D chooseSwapExtent(uint32_t width, uint32_t height) {

	VkExtent2D actualExtent = {
		width,
		height
	};

	return actualExtent;

}

static PFN_vkReleaseSwapchainImagesEXT vRelease = nullptr;


VKSwapChain::VKSwapChain(VKDevice* _d, VkSurfaceKHR _surface, DeviceOwnedAllocator* allocator, uint32_t requestImages, uint32_t maxFrameInFlight,
	VK::Utils::SwapChainSupportDetails& swapChainSupport)
	:
	device(_d), surface(_surface), maxFrameInFlight(maxFrameInFlight) {

	if (!vRelease)
	{
		vRelease =
			(PFN_vkReleaseSwapchainImagesEXT)
			vkGetDeviceProcAddr(device->device, "vkReleaseSwapchainImagesEXT");
	}
	SetSwapChainProperties(swapChainSupport, requestImages);


	waitSemaphores = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * maxFrameInFlight));
	signalSemaphores = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * maxFrameInFlight));
	images = reinterpret_cast<VkImage*>(allocator->Alloc(sizeof(VkImage) * imageCount));
	presentationFences = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * maxFrameInFlight));
	imageViews = reinterpret_cast<EntryHandle*>(allocator->Alloc(sizeof(EntryHandle) * imageCount));
}


void VKSwapChain::SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount)
{
	swapChainImageFormat = chooseSwapSurfaceFormat(swapChainSupport.formats.data(), swapChainSupport.formats.size());
	presentMode = chooseSwapPresentMode(swapChainSupport.presentModes.data(), swapChainSupport.presentModes.size());
	
	preTransform = swapChainSupport.capabilities.currentTransform;


	if (!_imageCount)
	{
		imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
	}
	else {
		imageCount = std::min(std::max(_imageCount, swapChainSupport.capabilities.minImageCount), swapChainSupport.capabilities.maxImageCount);
	}
}


void VKSwapChain::CreateSwapChain( 
	uint32_t width, uint32_t height)
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
	
	
	uint32_t* qfc = queueFamiliesCache;
	uint32_t ret = device->GetFamiliesOfCapableQueues(&qfc, &queueFamiliesCacheCount, GRAPHICSQUEUE | TRANSFERQUEUE | PRESENTQUEUE);
	queueSharing = createInfo.imageSharingMode = (queueFamiliesCacheCount > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = qfc;


	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	VkResult result = VK_SUCCESS;

	if ((result = vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain)) != VK_SUCCESS) {
		std::cerr <<  result << std::endl;
		throw std::runtime_error("failed to create swap chain!");
	}
	
	VkImage* swcImages = images;

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	CreateSyncObject();
}

void VKSwapChain::CreateSyncObject()
{
	EntryHandle* imageAvailablesIndices = device->CreateSemaphores(imageCount);
	EntryHandle* renderFinishedIndices = device->CreateSemaphores(imageCount);
	EntryHandle* fences = device->CreateFences(imageCount, VK_FENCE_CREATE_SIGNALED_BIT);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		waitSemaphores[i] = imageAvailablesIndices[i];
		signalSemaphores[i] = renderFinishedIndices[i];
		presentationFences[i] = fences[i];
	}
}


void VKSwapChain::RecreateSwapChain(uint32_t width, uint32_t height)
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

	uint32_t* qfc = queueFamiliesCache;

	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = qfc;

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	VkImage* swcImages = images;

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);
}

EntryHandle* VKSwapChain::CreateSwapchainViews()
{
	for (uint32_t i = 0; i < imageCount; i++) {
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

	if (swapChain) {
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
	if (swapChain) {
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < imageCount; i++)
	{
		device->DestroyImageView(imageViews[i]);
	}

	DestroySyncObject();
}

void VKSwapChain::DestroySyncObject()
{
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		device->DestroySemaphore(waitSemaphores[i]);
		device->DestroySemaphore(signalSemaphores[i]);
		device->DestroyFence(presentationFences[i]);
	}
}

uint32_t VKSwapChain::AcquireNextSwapChainImage(uint64_t _timeout, uint32_t acquireSemaphore)
{
	uint32_t imageIndex;

	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, device->GetSemaphore(waitSemaphores[acquireSemaphore]), VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	return imageIndex;
}

uint32_t VKSwapChain::AcquireNextSwapChainImage2(uint64_t _timeout, uint32_t acquireSemaphore)
{

	uint32_t imageIndex;

	VkFence fence = device->GetFence(presentationFences[acquireSemaphore]);

	vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX);

	VkAcquireNextImageInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
	info.semaphore = device->GetSemaphore(waitSemaphores[acquireSemaphore]);
	info.swapchain = swapChain;
	info.timeout = _timeout;
	info.deviceMask = 1;
	
	VkResult result = vkAcquireNextImage2KHR(device->device, &info, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	return imageIndex;
}