

#include "VKSwapChain.h"
#include "VKDevice.h"
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
		SWCATTACHMENTSOFFSET(swc->imageCount) +
		SWCQFCOFFSET(swc->imageCount, swc->attachmentCount) +
		SWCDEPENDSOFFSET(swc->queueFamiliesCacheCount);
}

static uintptr_t* GetDependenciesPtr(VKSwapChain* swc)
{
	return reinterpret_cast<uintptr_t*>(GetDependencies(swc));
}


static VkImage* GetSwapChainImages(VKSwapChain* swc)
{
	return reinterpret_cast<VkImage*>(swc->headofperdata);
}


static uintptr_t GetHeadOfAttachments(VKSwapChain* swc)
{
	uintptr_t headofattachments = swc->headofperdata + SWCATTACHMENTSOFFSET(swc->imageCount);

	return headofattachments;
}

static uintptr_t* GetHeadOfAttachmentsPtr(VKSwapChain* swc)
{
	return reinterpret_cast<uintptr_t*>(GetHeadOfAttachments(swc));
}

static uint32_t* GetQueueFamilyCache(VKSwapChain* swc)
{
	return reinterpret_cast<uint32_t*>(swc->headofperdata + SWCATTACHMENTSOFFSET(swc->imageCount) + SWCQFCOFFSET(swc->imageCount, swc->attachmentCount));
}

size_t VKSwapChain::CalculateSwapChainMemoryUsage()
{
	return 
		sizeof(uintptr_t)* imageCount * (1 + attachmentCount) +
		sizeof(VkImage) * imageCount +
		sizeof(uint32_t) * 2 +
		sizeof(uintptr_t) * (imageCount * ((semaphorePerStage * numberOfStages) + 1));
}

void VKSwapChain::AddFramebufferAttachments(EntryHandle** attachmentIndices, uint32_t attachIndicesSize)
{
	uint32_t i = 0;

	uintptr_t headofattachments = GetHeadOfAttachments(this);

	uintptr_t* attaches = reinterpret_cast<uintptr_t*>(headofattachments);

	uintptr_t outputaddr = headofattachments + sizeof(size_t) * imageCount;

	while (i < imageCount)
	{
		attaches[i] = outputaddr;

		EntryHandle** output = reinterpret_cast<EntryHandle**>(outputaddr);

		for (uint32_t j = 0; j < attachIndicesSize; j++)
		{
			output[j] = attachmentIndices[j];
		}
		outputaddr += sizeof(size_t) * attachmentCount;
		i++;

	}
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
	uint32_t width, uint32_t height, 
	EntryHandle _renderPassIndex, EntryHandle** attachmentIndices, uint32_t numattaches)
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


	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}
	
	VkImage *swcImages = GetSwapChainImages(this);

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	renderTargetIndex = device->CreateRenderTarget(_renderPassIndex, imageCount);

	auto ref = device->GetRenderTarget(renderTargetIndex);

	AddFramebufferAttachments(attachmentIndices, numattaches);

	uintptr_t* attaches = GetHeadOfAttachmentsPtr(this);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		auto fbref = reinterpret_cast<EntryHandle**>(attaches[i]);
		fbref[attachmentCount-1] = &ref->imageViews[i];
	}

	CreateSwapChainElements();
	CreateSyncObject();
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

	CreateSwapChainElements();
}

void VKSwapChain::CreateSwapChainElements()
{

	EntryHandle* indices = reinterpret_cast<EntryHandle*>(device->AllocFromDeviceCache(sizeof(EntryHandle) * attachmentCount));

	auto renderTarget = device->GetRenderTarget(renderTargetIndex);

	uintptr_t* attaches = GetHeadOfAttachmentsPtr(this);

	VkImage* swcImages = reinterpret_cast<VkImage*>(headofperdata);

	for (size_t i = 0; i < imageCount; i++) {
		auto ref2 = reinterpret_cast<EntryHandle**>(attaches[i]);

		renderTarget->imageViews[i] = device->CreateImageView(swcImages[i], 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);

		for (size_t j = 0; j < attachmentCount; j++) {
			indices[j] = *ref2[j];
		}

		renderTarget->framebufferIndices[i] = device->CreateFrameBuffer(indices, attachmentCount, renderTarget->renderPassIndex, swapChainExtent);
	}
}


void VKSwapChain::DestroySwapChain()
{
	if (swapChain) {
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
	}
}

uint32_t VKSwapChain::AcquireNextSwapChainImage(uint64_t _timeout, uint32_t frame)
{
	uint32_t imageIndex;
	
	auto depends = GetDependenciesForImageIndex(frame);

	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, device->GetSemaphore(depends[0]), VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	return imageIndex;
}