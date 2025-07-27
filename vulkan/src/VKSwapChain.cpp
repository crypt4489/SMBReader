

#include "VKSwapChain.h"
#include "VKDevice.h"
#include "VKUtilities.h"


void VKSwapChain::SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport)
{
	swapChainImageFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	
	preTransform = swapChainSupport.capabilities.currentTransform;
	imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}
}


void VKSwapChain::CreateSwapChain( 
	uint32_t width, uint32_t height, 
	uint32_t _renderPassIndex, std::vector<ImageIndex*> &attachmentIndices)
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
	
	
	std::vector<uint32_t> queueFamilyIndices;
	uint32_t ret = device->GetFamiliesOfCapableQueues(queueFamilyIndices, GRAPHICS | TRANSFER | PRESENT);
	uint32_t numberOfQueueFamilies = static_cast<uint32_t>(queueFamilyIndices.size());
	queueSharing = createInfo.imageSharingMode = (numberOfQueueFamilies > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = numberOfQueueFamilies;
	createInfo.pQueueFamilyIndices = queueFamilyIndices.data();

	queueFamiliesCache.resize(numberOfQueueFamilies);

	for (uint32_t i = 0; i < numberOfQueueFamilies; i++) 
		queueFamiliesCache[i] = queueFamilyIndices[i];

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;


	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr);

	swapChainImages.resize(imageCount);
	
	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());

	attachments.resize(imageCount, VKFrameBufferAttachments(attachmentCount));
	swapChainImageViews.resize(imageCount);
	renderTargetIndex = device->CreateRenderTarget(_renderPassIndex, imageCount);

	AddFramebufferAttachments(attachmentIndices);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		auto& fbref = attachments[i];
		fbref.attachments[attachmentCount-1] = &swapChainImageViews[i];
	}

	CreateSwapChainElements();
	CreateSyncObject();
}

void VKSwapChain::CreateSyncObject()
{
	std::vector<uint32_t> imageAvailablesIndices = device->CreateSemaphores(imageCount);
	std::vector<uint32_t> renderFinishedIndices = device->CreateSemaphores(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		CreateSwapChainDependency(i, imageAvailablesIndices[i], renderFinishedIndices[i]);
	}
}

void VKSwapChain::CreateSwapChainDependency(uint32_t imageIndex, uint32_t beforeDrawing, uint32_t present)
{
	std::vector<std::vector<uint32_t>> copy{ {beforeDrawing}, {present} };
	dependencies.AddIndicesForImage(imageIndex, copy);
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
	createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamiliesCache.size());
	createInfo.pQueueFamilyIndices = queueFamiliesCache.data();

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());

	CreateSwapChainElements();
}

void VKSwapChain::CreateSwapChainElements()
{
	for (size_t i = 0; i < imageCount; i++) {
		swapChainImageViews[i] = device->CreateImageView(swapChainImages[i], 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	std::vector<std::vector<uint32_t>> indices(imageCount, std::vector<uint32_t>(attachmentCount));

	auto& renderTarget = device->GetRenderTarget(renderTargetIndex);

	for (size_t i = 0; i < imageCount; i++) {
		auto& ref = indices[i];
		auto& ref2 = attachments[i];

		for (size_t j = 0; j < attachmentCount; j++) {
			ref[j] = *ref2.attachments[j];
		}

		renderTarget.framebufferIndices[i] = device->CreateFrameBuffer(ref, renderTarget.renderPassIndex, swapChainExtent);
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
	
	auto& depends = dependencies.chains[frame];

	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, device->GetSemaphore(depends[0][0]), VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	return imageIndex;
}