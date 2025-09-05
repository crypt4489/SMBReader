

#include "VKSwapChain.h"
#include "VKDevice.h"
#include "VKUtilities.h"


void VKSwapChain::SetSwapChainProperties(VK::Utils::SwapChainSupportDetails& swapChainSupport, uint32_t _imageCount)
{
	swapChainImageFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	
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
		imageCount = std::max(_imageCount, swapChainSupport.capabilities.minImageCount);
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


	uint32_t* qfc = reinterpret_cast<uint32_t*>(headofperdata + SWCATTACHMENTSOFFSET(imageCount) + SWCQFCOFFSET(imageCount, attachmentCount));
	queueFamiliesCacheCount = numberOfQueueFamilies;

	for (uint32_t i = 0; i < numberOfQueueFamilies; i++) 
		qfc[i] = queueFamilyIndices[i];

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;


	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}
	
	VkImage *swcImages = reinterpret_cast<VkImage*>(headofperdata);

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	renderTargetIndex = device->CreateRenderTarget(_renderPassIndex, imageCount);

	auto &ref = device->GetRenderTarget(renderTargetIndex);

	AddFramebufferAttachments(attachmentIndices);

	uintptr_t* attaches = reinterpret_cast<uintptr_t*>(headofperdata + SWCATTACHMENTSOFFSET(imageCount));

	for (uint32_t i = 0; i < imageCount; i++)
	{
		auto fbref = reinterpret_cast<size_t*>(attaches[i]);
		fbref[attachmentCount-1] = reinterpret_cast<size_t>(&ref.imageViews[i]);
	}

	CreateSwapChainElements();
	CreateSyncObject();
}

void VKSwapChain::CreateSyncObject()
{
	std::vector<uint32_t> imageAvailablesIndices = device->CreateSemaphores(imageCount);
	std::vector<uint32_t> renderFinishedIndices = device->CreateSemaphores(imageCount);

	uintptr_t dependencies = headofperdata +
		SWCATTACHMENTSOFFSET(imageCount) +
		SWCQFCOFFSET(imageCount, attachmentCount) +
		SWCDEPENDSOFFSET(queueFamiliesCacheCount);

	uintptr_t dependenciesdata = dependencies + (sizeof(uintptr_t) * imageCount);

	uintptr_t* ptr1 = reinterpret_cast<uintptr_t*>(dependencies);
	uintptr_t* ptr2 = reinterpret_cast<uintptr_t*>(dependenciesdata);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		ptr1[i] = dependenciesdata;
		ptr2[0] = imageAvailablesIndices[i];
		ptr2[1] = renderFinishedIndices[i];
		
		ptr2 = std::next(ptr2, 2);
		dependenciesdata += sizeof(uintptr_t) * 2;
	}
}

size_t* VKSwapChain::GetDependenciesForImageIndex(uint32_t imageIndex)
{
	uintptr_t dependencies = headofperdata +
		SWCATTACHMENTSOFFSET(imageCount) +
		SWCQFCOFFSET(imageCount, attachmentCount) +
		SWCDEPENDSOFFSET(queueFamiliesCacheCount);

	uintptr_t* ptr1 = reinterpret_cast<uintptr_t*>(dependencies);

	return reinterpret_cast<size_t*>(ptr1[imageIndex]);
}

void VKSwapChain::CreateSwapChainDependency(uint32_t imageIndex, uint32_t beforeDrawing, uint32_t present)
{
	//std::vector<std::vector<uint32_t>> copy{ {beforeDrawing}, {present} };
	//dependencies.AddIndicesForImage(imageIndex, copy);
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

	uint32_t* qfc = reinterpret_cast<uint32_t*>(headofperdata + SWCATTACHMENTSOFFSET(imageCount) + SWCQFCOFFSET(imageCount, attachmentCount));

	createInfo.queueFamilyIndexCount = queueFamiliesCacheCount;
	createInfo.pQueueFamilyIndices = qfc;

	createInfo.preTransform = preTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	VkImage* swcImages = reinterpret_cast<VkImage*>(headofperdata);

	vkGetSwapchainImagesKHR(device->device, swapChain, &imageCount, swcImages);

	CreateSwapChainElements();
}

void VKSwapChain::CreateSwapChainElements()
{
	std::vector<size_t> indices(attachmentCount);

	auto& renderTarget = device->GetRenderTarget(renderTargetIndex);

	uintptr_t* attaches = reinterpret_cast<uintptr_t*>(headofperdata + SWCATTACHMENTSOFFSET(imageCount));

	VkImage* swcImages = reinterpret_cast<VkImage*>(headofperdata);

	for (size_t i = 0; i < imageCount; i++) {
		auto ref2 = reinterpret_cast<size_t*>(attaches[i]);

		renderTarget.imageViews[i] = device->CreateImageView(swcImages[i], 1, swapChainImageFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);

		for (size_t j = 0; j < attachmentCount; j++) {
			indices[j] = *reinterpret_cast<ImageIndex*>(ref2[j]);
		}

		renderTarget.framebufferIndices[i] = device->CreateFrameBuffer(indices, renderTarget.renderPassIndex, swapChainExtent);
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