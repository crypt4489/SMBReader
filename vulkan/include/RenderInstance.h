#pragma once


#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

#include "FileManager.h"
#include "ThreadManager.h"
#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKShaderCache.h"
#include "VKPipelineCache.h"
#include "WindowManager.h"


class QueueManager
{
public:

	QueueManager(std::vector<uint32_t> _cqs, int32_t _mqc, uint32_t _qfi, VkCommandPool& _p, VkDevice& _d) :
		bitmap(0U),
		maxQueueCount(_mqc),
		queueFamilyIndex(_qfi),
		pool(_p), 
		device(_d),
		sema(Semaphore(_mqc))
	{

		assert(maxQueueCount <= 16);

		for (uint32_t i = 0; i < _cqs.size(); i++)
		{
			bitmap |= (1 << _cqs[i]);
		}
	}

	std::optional<std::tuple<VkQueue, VkCommandPool, int32_t>> GetQueue()
	{
		sema.Wait();
		for (int32_t i = 0; i < maxQueueCount; i++)
		{
			uint32_t mask = (1 << i);
			if ((bitmap & mask) == 0)
			{
				bitmap |= mask;
				VkQueue queue;
				vkGetDeviceQueue(device, queueFamilyIndex, i, &queue);
				return std::tuple<VkQueue, VkCommandPool, int32_t> (queue, pool, i);
			}
		}
		return std::nullopt;
	}

	void ReturnQueue(int32_t queueNum)
	{
		bitmap &= ~(1U << queueNum);
		sema.Notify();
	}
private:
	uint16_t bitmap;
	const int32_t maxQueueCount;
	const uint32_t queueFamilyIndex;
	VkCommandPool& pool;
	VkDevice& device;
	Semaphore sema;
};


class RenderInstance
{
public:

	RenderInstance() : transferSemaphore(Semaphore(1))
	{

	};

	void CreateRenderInstance()
	{

		VkApplicationInfo appInfoStruct{};

		appInfoStruct.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfoStruct.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfoStruct;


		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

		uint32_t glfwReqExtCount;

		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwReqExtCount);

		for (uint32_t i = 0; i < glfwReqExtCount; i++)
		{
			instanceExtensions.push_back(glfwExtensions[i]);
		}

		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		uint32_t instExtensionRequired = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, nullptr);

		if (!instExtensionRequired)
		{
			throw std::runtime_error("Need extension layers, found none");
		}

		std::vector<VkExtensionProperties> extensions(instExtensionRequired);

		if (vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, extensions.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed second time");
		}
		;

		for (auto& requested : instanceExtensions)
		{
			if (std::find_if(extensions.begin(), extensions.end(), [&requested](auto i) {
				return strcmp(requested, i.extensionName) == 0;
				}) == std::end(extensions))
			{
				std::cerr << "Extension " << requested << " not available\n";
				throw std::runtime_error("Cannot find extension");
			}
		}

		uint32_t layersCount = 0;
		vkEnumerateInstanceLayerProperties(&layersCount, nullptr);

		if (!layersCount)
		{
			throw std::runtime_error("Need validation layers, found none");
		}

		std::vector<VkLayerProperties> layerProps(layersCount);

		if (vkEnumerateInstanceLayerProperties(&layersCount, layerProps.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("vkEnumerateInstanceLayerProperties failed second time");
		}

		for (auto& requested : instanceLayers)
		{
			if (std::find_if(layerProps.begin(), layerProps.end(), [&requested](auto i) {
				return strcmp(requested, i.layerName) == 0;
				}) == std::end(layerProps))
			{
				std::cerr << "Validation layer " << requested << " not available\n";
				throw std::runtime_error("Cannot find validation layer");
			}
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
		createInfo.ppEnabledExtensionNames = instanceExtensions.data();

		createInfo.ppEnabledLayerNames = instanceLayers.data();
		createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());

		VkDebugUtilsMessengerCreateInfoEXT instanceDebugInfo{};
		instanceDebugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		instanceDebugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		instanceDebugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		instanceDebugInfo.pfnUserCallback = ::VK::Utils::debugCallback;
		instanceDebugInfo.pUserData = nullptr; 


		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&instanceDebugInfo;

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

		VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugInfo.pfnUserCallback = ::VK::Utils::debugCallback;
		debugInfo.pUserData = nullptr; 

		result = ::VK::Utils::CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Cannot establish debug callback");
		}
	}

	void CreateGPUReferenceAndLogicalDevice()
	{
		uint32_t physicalDeviceCount = 0;
		VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to get device count");
		}

		std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

		result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to get device pointers");
		}

		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;

		std::multimap<int, VkPhysicalDevice> gpuScores;

		for (const auto& i : physicalDevices)
		{
			GetPhysicalDevicePropertiesandFeatures(i, deviceProperties, deviceFeatures);

			if (!isDeviceSuitable(i)) continue;

			gpuScores.insert(std::pair<int, VkPhysicalDevice>([&deviceProperties, &deviceFeatures]() {
				int score = 0;

				if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					score = std::numeric_limits<short>::max();
				}

				score += deviceProperties.limits.maxImageDimension2D;

				if (!deviceFeatures.geometryShader || 
					!deviceFeatures.tessellationShader || 
					!deviceFeatures.textureCompressionBC ||
					!deviceFeatures.samplerAnisotropy ||
					!deviceFeatures.multiDrawIndirect)
				{
					score = std::numeric_limits<int>::min();
				}

				return score;
				}(), i));

			::VK::Utils::operator<<(std::cout, deviceProperties);
		}

		auto bestCase = gpuScores.rbegin();

		if (gpuScores.empty() || bestCase->first <= 0)
		{
			throw std::runtime_error("No suitable gpu found for this");
		}

		gpu = bestCase->second;

		msaaSamples = GetMaxMSAALevels();

		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);
		::VK::Utils::operator<<(std::cout, memProperties);

		uint32_t queueFamilyCount;

		std::vector<VkQueueFamilyProperties> famProps;

		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

		famProps.resize(queueFamilyCount);

		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, famProps.data());

		int counter = 0;

		for (const auto& props : famProps)
		{
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(gpu, counter, renderSurface, &presentSupport);

			::VK::Utils::operator<<(std::cout, props);

			if (presentSupport) presentIdx = counter;

			if (props.queueFlags & VK_QUEUE_TRANSFER_BIT &&
				props.queueFlags & VK_QUEUE_COMPUTE_BIT &&
				props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphicsIdx = counter;
				graphicsMaxQueueCount = props.queueCount;
			}


			if (graphicsIdx >= 0 && presentIdx >= 0)
			{
				break;
			}

			counter++;
		}

		if (graphicsIdx == -1 || presentIdx == -1) 
		{
			throw std::runtime_error("Cannot find a device with a queue that has COMPUTE,"
				"TRANSFER and GRAPHICS bits set");
		}

		std::set queueIndices = { graphicsIdx, presentIdx };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		std::vector<float> queuePriorties(graphicsMaxQueueCount, 1.0f);
		for (int queueFamily : queueIndices) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			if (queueFamily == graphicsIdx)
				queueCreateInfo.queueCount = graphicsMaxQueueCount;
			else
				queueCreateInfo.queueCount = 1;
			
			queueCreateInfo.pQueuePriorities = queuePriorties.data();
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures features{};
		features.geometryShader = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.tessellationShader = VK_TRUE;
		features.samplerAnisotropy = VK_TRUE;
		features.multiDrawIndirect = VK_TRUE;

		VkDeviceCreateInfo logDeviceInfo{};
		logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		logDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		logDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
		logDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		logDeviceInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
		logDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
		logDeviceInfo.ppEnabledLayerNames = instanceLayers.data();
		logDeviceInfo.pEnabledFeatures = &features;

		VkResult res = vkCreateDevice(gpu, &logDeviceInfo, nullptr, &logicalDevice);

		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical GPU, mate!");
		}

		vkGetDeviceQueue(logicalDevice, graphicsIdx, 0, &graphicsQueue);
		vkGetDeviceQueue(logicalDevice, presentIdx, 0, &presentQueue);


		//::VK::Utils::SwapChainSupportDetails supportDetails = ::VK::Utils::querySwapChainSupport(gpu, renderSurface);
	}

	void CreateDrawingSurface()
	{
		VkResult res = glfwCreateWindowSurface(instance, windowMan->GetWindow(), nullptr, &renderSurface);

		if (res != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void CreateSwapChain()
	{
		::VK::Utils::SwapChainSupportDetails swapChainSupport = ::VK::Utils::querySwapChainSupport(gpu, renderSurface);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = renderSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		
		uint32_t queueFamilyIndices[2] = { static_cast<uint32_t>(graphicsIdx), static_cast<uint32_t>(presentIdx) };

		if (graphicsIdx != presentIdx)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} 
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

	}

	void CreateVKSWCImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	void CreateFrameBuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			std::array<VkImageView, 3> attachments = {
				colorImageView,
				depthImageView,
				swapChainImageViews[i],
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer #" + std::to_string(i) + "!");
			}
		}
	}

	void CreateDepthImage()
	{
		
		VkImageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = depthFormat;
		info.extent = { swapChainExtent.width, swapChainExtent.height, 1 };
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.samples = msaaSamples;
		info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		std::tie(depthImage, depthMemory) = ::VK::Utils::CreateImage(logicalDevice, gpu, info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.format = depthFormat;
		viewInfo.image = depthImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

		depthImageView = ::VK::Utils::CreateImageView(logicalDevice, viewInfo);

		::VK::Utils::TransitionImageLayout(logicalDevice, commandPool, graphicsQueue, depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);
	}

	void DestroySwapChain()
	{

		if (colorImageView) vkDestroyImageView(logicalDevice, colorImageView, nullptr);
		if (colorImage) vkDestroyImage(logicalDevice, colorImage, nullptr);
		if (colorImageMemory) vkFreeMemory(logicalDevice, colorImageMemory, nullptr);

		for (auto framebuffer : swapChainFramebuffers) {
			if (framebuffer) vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		}

		for (auto imageView : swapChainImageViews) {
			if (imageView) vkDestroyImageView(logicalDevice, imageView, nullptr);
		}

		vkDestroyImageView(logicalDevice, depthImageView, nullptr);
		vkDestroyImage(logicalDevice, depthImage, nullptr);
		vkFreeMemory(logicalDevice, depthMemory, nullptr);

		if (swapChain) {
			vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
		}
	}

	void RecreateSwapChain() {

		int width = 0, height = 0;
		glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(logicalDevice);

		DestroySwapChain();

		CreateSwapChain();
		CreateVKSWCImageViews();
		CreateMSAAColorResources();
		CreateDepthImage();
		CreateFrameBuffers();
	}

	std::pair<VkShaderModule, VkShaderStageFlagBits> GetShaderFromCache(const std::string& name)
	{	  
		return shaderCache.GetShader(name);
	}


	void CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = msaaSamples;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void CreateGraphicsCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = graphicsIdx;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void CreateTransferCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = graphicsIdx;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &transferPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}

		gptManager = new QueueManager({ 0 }, graphicsMaxQueueCount, graphicsIdx, transferPool, logicalDevice);
	}

	void CreateCommandBuffer()
	{
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void BeginCommandBufferRecording(VkCommandBuffer cb, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; 
		beginInfo.pInheritanceInfo = nullptr; 

		if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cb, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(cb, 0, 1, &scissor);

	}

	void EndCommandBufferRecording(VkCommandBuffer cb)
	{
		vkCmdEndRenderPass(cb);

		if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void CreateSyncObjects()
	{
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create semaphores!");
			}
		}
	}

	uint32_t BeginFrame()
	{
		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
			return 0xFFFFFFFF;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);

		BeginCommandBufferRecording(commandBuffers[currentFrame], imageIndex);

		return imageIndex;
	}

	void SubmitFrame(uint32_t imageIndex)
	{

		EndCommandBufferRecording(commandBuffers[currentFrame]);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resizeWindow) {
			resizeWindow = false;
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT * 100;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * 100;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 100;

		if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void CreateMSAAColorResources() {


		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = swapChainExtent.width;
		imageInfo.extent.height = swapChainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;

		imageInfo.format = swapChainImageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage =
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = msaaSamples;
		imageInfo.flags = 0;

		std::tie(colorImage, colorImageMemory) = ::VK::Utils::CreateImage(logicalDevice, gpu, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = colorImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = swapChainImageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;

		colorImageView = ::VK::Utils::CreateImageView(logicalDevice, viewInfo);
	}

	void WaitOnQueues()
	{
		vkQueueWaitIdle(graphicsQueue);
		vkQueueWaitIdle(presentQueue);
	}

	static void frameResizeCB(GLFWwindow* window, int width, int height)
	{
		auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
		renderInst->SetResizeBool(true);
	}

	void CreatePipelines()
	{
		// Create Shaders

		auto shader1 = shaderCache.GetShader("3dtextured.vert.spv");
		auto shader2 = shaderCache.GetShader("3dtextured.frag.spv");

		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders = { shader1, shader2 };

		auto shader3 = shaderCache.GetShader("text.vert.spv");
		auto shader4 = shaderCache.GetShader("text.frag.spv");

		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders2 = { shader3, shader4 };

		DescriptorSetLayoutBuilder textDescriptor{}, globalBufferBuilder{}, genericObjectBuilder{};
		std::vector<VkDescriptorSetLayout> textDescriptorContainers(1);
		std::vector<VkDescriptorSetLayout> regularMeshConatiners(2);

		textDescriptor.AddPixelImageSamplerLayout(0);

		textDescriptorContainers[0] = textDescriptor.CreateDescriptorSetLayout(logicalDevice);

		descriptorLayoutCache.AddLayout("oneimage", textDescriptorContainers[0]);

		globalBufferBuilder.AddBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

		regularMeshConatiners[0] = globalBufferBuilder.CreateDescriptorSetLayout(logicalDevice);
		
		descriptorLayoutCache.AddLayout("mainrenderpass", regularMeshConatiners[0]);

		genericObjectBuilder.AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

		genericObjectBuilder.AddPixelImageSamplerLayout(1);

		regularMeshConatiners[1] = genericObjectBuilder.CreateDescriptorSetLayout(logicalDevice);

		descriptorLayoutCache.AddLayout("genericobject", regularMeshConatiners[1]);

		//mainRenderPassCache.CreatePipeline(textDescriptorContainers, std::nullopt, std::nullopt, shaders, VK_COMPARE_OP_LESS, GetMSAASamples(), "2dimage");

		mainRenderPassCache.CreatePipeline(regularMeshConatiners, std::nullopt, std::nullopt, shaders, VK_COMPARE_OP_LESS, GetMSAASamples(), "genericpipeline");

		mainRenderPassCache.CreatePipeline(textDescriptorContainers, TextVertex::getBindingDescription(), TextVertex::getAttributeDescriptions(), shaders2, VK_COMPARE_OP_ALWAYS, GetMSAASamples(), "text");

		
	}

	PipelineCacheObject GetPipeline(std::string ptype)
	{
		return mainRenderPassCache.GetPipelineFromCache(ptype);
	}

	void CreateGlobalBuffer()
	{
		std::tie(globalBuffer, globalBufferMemory) = ::VK::Utils::createBuffer(logicalDevice,
			gpu, 128'000'000, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			VK_SHARING_MODE_EXCLUSIVE, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		);

		vkMapMemory(logicalDevice, globalBufferMemory, 0, 128'000'000, 0, &globalmemorymapped);
	}

	void CreateDyanmicGlobalMeshBuffers()
	{
		std::tie(std::ignore, dynamicMemoryMapped) = GetPageFromUniformBuffer(sizeof(glm::mat4) * 256 * MAX_FRAMES_IN_FLIGHT, dynamicGlobalOffset);
	}

	std::tuple<VkBuffer, void*, size_t, size_t> GetDynamicBuffer()
	{
		return { globalBuffer, dynamicMemoryMapped, dynamicGlobalOffset, sizeof(glm::mat4) * 256 };
	}

	void UpdateDynamicGlobalBuffer(void* data, size_t dataSize, size_t offset, uint32_t frame)
	{
		void* to = ((char*)dynamicMemoryMapped) + (sizeof(glm::mat4) * 256 * frame) + offset;
		std::memcpy(to, data, dataSize);
	}


	std::pair<VkBuffer, void*> GetPageFromUniformBuffer(size_t size, size_t &offset)
	{
		offset = dumbAllocator;
		void* ret = ((char*)globalmemorymapped) + offset;
		dumbAllocator += size;
		return { globalBuffer, ret };
	}

	void CreateVulkanRenderer(WindowManager *window)
	{
		this->windowMan = window;
		windowMan->SetWindowResizeCallback(frameResizeCB);
		glfwSetWindowUserPointer(windowMan->GetWindow(), this);

		

		CreateRenderInstance();
		CreateDrawingSurface();
		CreateGPUReferenceAndLogicalDevice();

		depthFormat = ::VK::Utils::findSupportedFormat(gpu,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		CreateDescriptorPool();

		CreateSwapChain();
		CreateVKSWCImageViews();

		
		CreateMSAAColorResources();
		
		

		CreateGraphicsCommandPool();
		CreateTransferCommandPool();
		
		CreateDepthImage();

		CreateRenderPass();

		CreateFrameBuffers();
		
		CreateCommandBuffer();
		CreateSyncObjects();

		descriptorLayoutCache.device = logicalDevice;
		mainRenderPassCache.renderPass = renderPass;
		mainRenderPassCache.device = logicalDevice;
		shaderCache.SetLogicalDevice(logicalDevice);

		CreateGlobalBuffer();

		CreateDyanmicGlobalMeshBuffers();

		CreatePipelines();
	}

	
	void DestroyVulkanRenderer()
	{
		DestroyRenderInstance();
	}

	void DestroyRenderInstance()
	{
		if (gptManager) delete gptManager;

		vkDestroyBuffer(logicalDevice, globalBuffer, nullptr);
		
		vkFreeMemory(logicalDevice, globalBufferMemory, nullptr);

		shaderCache.DestroyShaderCache();

		mainRenderPassCache.DestroyPipelineCache();

		descriptorLayoutCache.DestroyLayoutCache();

		DestroySwapChain();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (inFlightFences[i])
			{
				vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
			}

			if (renderFinishedSemaphores[i])
			{
				vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
			}

			if (imageAvailableSemaphores[i])
			{
				vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
			}
		}

		if (transferPool)
		{
			vkDestroyCommandPool(logicalDevice, transferPool, nullptr);
		}

		if (commandPool)
		{
			vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
		}


		if (renderPass)
		{
			vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
		}

		if (descriptorPool)
		{
			vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
		}

		
		if (logicalDevice)
		{
			vkDeviceWaitIdle(logicalDevice);
			vkDestroyDevice(logicalDevice, nullptr);
		}

		if (renderSurface)
		{
			vkDestroySurfaceKHR(instance, renderSurface, nullptr);
		}

		::VK::Utils::DestroyDebugUtilsMessengerEXT(instance, nullptr);

		if (instance)
		{
			vkDestroyInstance(instance, nullptr);
		}
	}

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures)
	{
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	}

	void SetResizeBool(bool set)
	{
		resizeWindow = set;
	}

	VkDevice GetVulkanDevice() const
	{
		return logicalDevice;
	}

	VkPhysicalDevice GetVulkanPhysicalDevice() const
	{
		return gpu;
	}

	VkCommandPool GetVulkanGraphincsCommandPool() const
	{
		return commandPool;
	}

	VkQueue GetGraphicsQueue() const
	{
		return graphicsQueue;
	}

	VkCommandPool GetVulkanTransferCommandPool() const
	{
		return transferPool;
	}

	auto GetTransferQueue()
	{
		return gptManager->GetQueue();
	}

	void ReturnTranferQueue(int32_t i)
	{
		gptManager->ReturnQueue(i);
	}

	VkRenderPass GetRenderPass() const
	{
		return renderPass;
	}

	VkDescriptorPool GetDescriptorPool() const
	{
		return descriptorPool;
	}

	VkCommandBuffer GetCurrentCommandBuffer() const
	{
		return commandBuffers[currentFrame];
	}

	uint32_t GetCurrentFrame() const
	{
		return currentFrame;
	}

	VkSampleCountFlagBits GetMSAASamples() const
	{
		return msaaSamples;
	}

	Semaphore& GetTransferSemaphore()
	{
		return transferSemaphore;
	}

	VKPipelineCache* GetMainRenderPassCache()
	{
		return &mainRenderPassCache;
	}

	VKDescriptorLayoutCache* GetDescriptorLayoutCache()
	{
		return &descriptorLayoutCache;
	}

	 
	VKDescriptorSetCache* GetDescriptorSetCache()
	{
		return &descriptorSetCache;;
	}

	uint32_t GetSwapChainHeight() const
	{
		return swapChainExtent.height;
	}

	uint32_t GetSwapChainWidth() const
	{
		return swapChainExtent.width;
	}

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	

private:
	VkInstance instance = VK_NULL_HANDLE;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice gpu = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;
	int graphicsIdx = -1, presentIdx = -1;
	uint32_t graphicsMaxQueueCount = 0U;
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	//default shader will expand


	VkRenderPass renderPass = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandPool transferPool = VK_NULL_HANDLE;

	Semaphore transferSemaphore;

	QueueManager *gptManager;

	std::vector<VkCommandBuffer> commandBuffers{};

	std::vector<VkSemaphore> imageAvailableSemaphores{};
	std::vector<VkSemaphore> renderFinishedSemaphores{};
	std::vector<VkFence> inFlightFences{};

	WindowManager *windowMan = nullptr;

	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};

	VkDescriptorPool descriptorPool;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkImage colorImage = VK_NULL_HANDLE;
	VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
	VkImageView colorImageView = VK_NULL_HANDLE;

	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VkFormat depthFormat;

	VKShaderCache shaderCache;
	VKPipelineCache mainRenderPassCache;
	VKDescriptorLayoutCache descriptorLayoutCache;
	VKDescriptorSetCache descriptorSetCache;

	VkBuffer globalBuffer;
	VkDeviceMemory globalBufferMemory;
	VkDeviceSize dumbAllocator;
	void* globalmemorymapped;

	void* dynamicMemoryMapped;
	VkDeviceSize dynamicGlobalOffset;

	VkSampleCountFlagBits GetMaxMSAALevels()
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(gpu, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		::VK::Utils::SwapChainSupportDetails supportDetails;

		for (auto& requested : deviceExtensions)
		{
			if (std::find_if(availableExtensions.begin(), availableExtensions.end(), [&requested](auto extension) {
				return strcmp(requested, extension.extensionName) == 0;
				}) == std::end(availableExtensions))
			{
				return false;
			}
		}

		supportDetails = ::VK::Utils::querySwapChainSupport(device, renderSurface);

		if (supportDetails.formats.empty() || supportDetails.presentModes.empty())
		{
			return false;
		}

		return true;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

};

