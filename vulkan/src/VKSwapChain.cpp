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
	uint32_t* queueFamilyIndices, uint32_t numberOfQueueFamilies)
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

	queueSharing = createInfo.imageSharingMode = (numberOfQueueFamilies > 1) ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = numberOfQueueFamilies;
	createInfo.pQueueFamilyIndices = queueFamilyIndices;

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

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swapChainImages.data());

}

void VKSwapChain::CreateSwapChainImageViews()
{
	swapChainImageViews.resize(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); i++) {
		swapChainImageViews[i] = device->CreateImageView(swapChainImages[i], 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VKSwapChain::CreateFrameBuffers(VkRenderPass& renderPass, std::vector<VkImageView>& attachmentViews)
{
	swapChainFramebuffers.resize(swapChainImageViews.size());
	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			attachmentViews[0],
			attachmentViews[1],
			device->GetImageView(swapChainImageViews[i]),
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device->device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer #" + std::to_string(i) + "!");
		}
	}
}

void VKSwapChain::DestroySwapChain()
{

	for (auto framebuffer : swapChainFramebuffers) {
		vkDestroyFramebuffer(device->device, framebuffer, nullptr);
	}

	if (swapChain) {
		vkDestroySwapchainKHR(device->device, swapChain, nullptr);
	}

}

uint32_t VKSwapChain::AcquireNextSwapChainImage(uint64_t _timeout, VkSemaphore waitSemaphore, VkFence fence)
{
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device->device, swapChain, _timeout, waitSemaphore, fence, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		return 0xFFFFFFFF;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	return imageIndex;
}