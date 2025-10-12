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

static uintptr_t GetDependencies(VKSwapChain* swc)
{
	return 
		swc->headofperdata +
		SWCQFCOFFSET(swc->imageCount) +
		SWCDEPENDSOFFSET(2);
}

static uintptr_t* GetDependenciesPtr(VKSwapChain* swc)
{
	return reinterpret_cast<uintptr_t*>(GetDependencies(swc));
}


static VkImage* GetSwapChainImages(VKSwapChain* swc)
{
	return reinterpret_cast<VkImage*>(swc->headofperdata);
}


static uint32_t* GetQueueFamilyCache(VKSwapChain* swc)
{
	return reinterpret_cast<uint32_t*>(swc->headofperdata + SWCQFCOFFSET(swc->imageCount));
}

static EntryHandle* GetRenderTargetPointer(VKSwapChain* swc)
{
	return reinterpret_cast<EntryHandle*>(swc->headofperdata +
		SWCQFCOFFSET(swc->imageCount) +
		SWCDEPENDSOFFSET(2) +
		SWCRENDERTARGETSOFFSET(swc->imageCount, swc->semaphorePerStage, swc->numberOfStages));
}

void VKSwapChain::SetSwapChainData(void* data)
{
	headofperdata = reinterpret_cast<uintptr_t>(data);
	renderTargetIndex = GetRenderTargetPointer(this);
}

size_t VKSwapChain::CalculateSwapChainMemoryUsage()
{
	return 
		(sizeof(VkImage) * imageCount) + //images first
		(sizeof(uint32_t) * 2) + //queue families
		(sizeof(EntryHandle) * (imageCount * ((semaphorePerStage * numberOfStages) + 1))) +
		(sizeof(EntryHandle) * renderTargetCount);
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
	
	
	uint32_t* qfc = GetQueueFamilyCache(this);
	uint32_t ret = device->GetFamiliesOfCapableQueues(&qfc, &queueFamiliesCacheCount, GRAPHICS | TRANSFER | PRESENT);
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
	
	VkImage *swcImages = GetSwapChainImages(this);

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	CreateSyncObject();
}

void VKSwapChain::CreateRenderTarget(uint32_t index, EntryHandle renderPassIndex)
{
	renderTargetIndex[index] = device->CreateRenderTarget(renderPassIndex, imageCount);
}

void VKSwapChain::CreateSyncObject()
{
	EntryHandle* imageAvailablesIndices = device->CreateSemaphores(imageCount);
	EntryHandle* renderFinishedIndices = device->CreateSemaphores(imageCount);

	uintptr_t dependencies = GetDependencies(this);

	uintptr_t dependenciesdata = dependencies + (sizeof(uintptr_t) * imageCount);

	uintptr_t* ptr1 = reinterpret_cast<uintptr_t*>(dependencies);
	uintptr_t* ptr2 = reinterpret_cast<uintptr_t*>(dependenciesdata);

	size_t stride = semaphorePerStage * numberOfStages;

	for (uint32_t i = 0; i < imageCount; i++)
	{
		ptr1[i] = dependenciesdata;
		ptr2[0] = imageAvailablesIndices[i];
		ptr2[1] = renderFinishedIndices[i];
		
		ptr2 = std::next(ptr2, stride);
		dependenciesdata += sizeof(uintptr_t) * stride;
	}
}

EntryHandle* VKSwapChain::GetDependenciesForImageIndex(uint32_t imageIndex)
{
	uintptr_t* ptr1 = GetDependenciesPtr(this);

	return reinterpret_cast<EntryHandle*>(ptr1[imageIndex]);
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

	uint32_t* qfc = GetQueueFamilyCache(this);

	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = qfc;

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	VkImage* swcImages = GetSwapChainImages(this);

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);
}

EntryHandle* VKSwapChain::CreateSwapchainViews()
{
	EntryHandle* imageViews = reinterpret_cast<EntryHandle*>(device->AllocFromDeviceCache(sizeof(EntryHandle) * imageCount));

	VkImage* swcImages = reinterpret_cast<VkImage*>(headofperdata);

	for (size_t i = 0; i < imageCount; i++) {
		imageViews[i] = device->CreateImageView(swcImages[i], 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	return imageViews;
}

void VKSwapChain::CreateSwapChainElements(uint32_t index, uint32_t aAttachmentCount, EntryHandle *attachments, EntryHandle *imageViews)
{
	auto renderTarget = device->GetRenderTarget(renderTargetIndex[index]);

	for (size_t i = 0; i < imageCount; i++) {

		renderTarget->imageViews[i] = imageViews[i];

		for (size_t j = 0; j < aAttachmentCount; j++) {
			if (attachments[j] == EntryHandle() || (i && attachments[j] == imageViews[i-1]))
				attachments[j] = imageViews[i];
		}

		renderTarget->framebufferIndices[i] = device->CreateFrameBuffer(attachments, aAttachmentCount, renderTarget->renderPassIndex, swapChainExtent);
	}
}

void VKSwapChain::ResetSwapChain()
{
	if (swapChain) {
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < renderTargetCount; i++)
		device->ResetRenderTarget(renderTargetIndex[i]);

}

void VKSwapChain::DestroySwapChain()
{
	if (swapChain) {
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}
	for (uint32_t i = 0; i<renderTargetCount; i++)
		device->DestroyRenderTarget(renderTargetIndex[i]);
}

void VKSwapChain::DestroySyncObject()
{
	size_t totalCount = semaphorePerStage * numberOfStages * imageCount;

	uintptr_t dependencies = GetDependencies(this);

	uintptr_t dependenciesdata = dependencies + (sizeof(uintptr_t) * imageCount);

	EntryHandle* ptr2 = reinterpret_cast<EntryHandle*>(dependenciesdata);
	
	for (size_t i = 0; i < totalCount; i++)
	{
		device->DestroySemaphore(ptr2[i]);
	}
}

uint32_t VKSwapChain::AcquireNextSwapChainImage(uint64_t _timeout, uint32_t frame)
{
	uint32_t imageIndex;
	
	auto depends = GetDependenciesForImageIndex(frame);

	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, device->GetSemaphore(depends[0]), VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	return imageIndex;
}