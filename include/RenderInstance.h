#pragma once

#include <algorithm>
#include <iostream>
#include <vector>
#include <limits>
#include <map>
#include <set>


#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/include/GLFW/glfw3native.h"
#undef min
#undef max


namespace {
	namespace VKUtils {

		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
			SwapChainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount) {
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

			if (presentModeCount) {
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
			}

			return details;
		}

		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceProperties& props)
		{
			// Extract and display API version
			uint32_t apiVersionMajor = VK_VERSION_MAJOR(props.apiVersion);
			uint32_t apiVersionMinor = VK_VERSION_MINOR(props.apiVersion);
			uint32_t apiVersionPatch = VK_VERSION_PATCH(props.apiVersion);
			os << "API Version: " << apiVersionMajor << "." << apiVersionMinor << "." << apiVersionPatch << "\n";

			// Device properties
			os << "Device ID: " << props.deviceID << "\n";
			os << "Vendor ID: " << props.vendorID << "\n";
			os << "Device Name: " << props.deviceName << "\n";

			// Device type (as string)
			os << "Device Type: ";
			switch (props.deviceType)
			{
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: os << "Integrated GPU"; break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   os << "Discrete GPU"; break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    os << "Virtual GPU"; break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:            os << "CPU"; break;
			default:                                     os << "Unknown"; break;
			}
			os << "\n";

			// Other properties
			os << "Driver Version: " << props.driverVersion << "\n";
			os << "Limits: Max Image Dimension 2D: " << props.limits.maxImageDimension2D << "\n";

			return os;
		}


		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceMemoryProperties& props)
		{
			uint32_t i;

			for (i = 0; i < props.memoryHeapCount; i++)
			{
				os << "Heap Size : ";
				os << ((double)props.memoryHeaps[i].size / 1'000'000'000) << " GBs\n";
				os << "Heap Flags : ";
				os << props.memoryHeaps[i].flags << "\n";
			}

			for (i = 0; i < props.memoryTypeCount; i++)
			{
				os << "Memory Type Heap Index : ";
				os << props.memoryTypes[i].heapIndex << "\n";
				os << "Memory Type Property Flags : ";
				os << props.memoryTypes[i].propertyFlags << "\n";
			}

			return os;
		}

		std::ostream& operator<<(std::ostream& os, const VkQueueFamilyProperties& props)
		{

			os << "Queue Flags : \n";
			if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) os << "VK_QUEUE_TRANSFER_BIT\n";
			if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) os << "VK_QUEUE_COMPUTE_BIT\n";
			if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) os << "VK_QUEUE_GRAPHICS_BIT\n";
			if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) os << "VK_QUEUE_SPARSE_BINDING_BIT\n";

			os << "Queue Count : " << props.queueCount << "\n";
			os << "Time Stamp Valid Bits : " << props.timestampValidBits << "\n";
			os << "Min Image Transfer Granularity (depth, height, width) : \n";
			os << props.minImageTransferGranularity.depth << " " << props.minImageTransferGranularity.height << " " << props.minImageTransferGranularity.width << "\n";

			return os;
		}




		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData) {

			if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				return VK_FALSE;
			}

			std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

			return VK_FALSE;
		}


		static VkDebugUtilsMessengerEXT debugMessenger;

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator) {
			auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
			if (func) {
				return func(instance, pCreateInfo, pAllocator, &debugMessenger);
			}
			else {
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator) {

			if (!debugMessenger) return;

			auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (func) {
				func(instance, debugMessenger, pAllocator);
			}
		}
	}
}


class RenderInstance
{
public:

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
		instanceDebugInfo.pfnUserCallback = ::VKUtils::debugCallback;
		instanceDebugInfo.pUserData = nullptr; // Optional


		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&instanceDebugInfo;

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

		VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugInfo.pfnUserCallback = ::VKUtils::debugCallback;
		debugInfo.pUserData = nullptr; // Optional

		result = ::VKUtils::CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr);

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
					!deviceFeatures.textureCompressionBC)
				{
					score = std::numeric_limits<int>::min();
				}

				return score;
				}(), i));

			::VKUtils::operator<<(std::cout, deviceProperties);
		}

		auto bestCase = gpuScores.rbegin();

		if (gpuScores.empty() || bestCase->first <= 0)
		{
			throw std::runtime_error("No suitable gpu found for this");
		}

		gpu = bestCase->second;

		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);
		::VKUtils::operator<<(std::cout, memProperties);

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
			if (presentSupport) presentIdx = counter;

			if (props.queueFlags & VK_QUEUE_TRANSFER_BIT && 
				props.queueFlags & VK_QUEUE_COMPUTE_BIT && 
				props.queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsIdx = counter;

			if (graphicsIdx >= 0 && presentIdx >= 0) break;

			counter++;
		}

		if (graphicsIdx == -1 || presentIdx == -1) 
		{
			throw std::runtime_error("Cannot find a device with a queue that has COMPUTE, TRANSFER and GRAPHICS bits set and/or a presentation queue");
		}

		std::set queueIndices = { graphicsIdx, presentIdx };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		float queuePriority = 1.0f;
		for (int queueFamily : queueIndices) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures features{};
		features.geometryShader = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.tessellationShader = VK_TRUE;
		//features.multiDrawIndirect = VK_TRUE;

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

		//::VKUtils::SwapChainSupportDetails supportDetails = ::VKUtils::querySwapChainSupport(gpu, renderSurface);
	}

	void CreateDrawingSurface()
	{
		VkResult res = glfwCreateWindowSurface(instance, window, nullptr, &renderSurface);

		if (res != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void CreateSwapChain()
	{
		::VKUtils::SwapChainSupportDetails swapChainSupport = ::VKUtils::querySwapChainSupport(gpu, renderSurface);

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

	void CreateGraphicsPipelineTemp()
	{

	}

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures)
	{
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	}

	void CreateGLFWWindow()
	{
		bool good = glfwInit();
		if (!good) throw std::runtime_error("Cannot create window");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(800, 600, "SMB File Viewer", nullptr, nullptr);

	}

	void CreateVulkanRenderer()
	{
		CreateGLFWWindow();
		CreateRenderInstance();
		CreateDrawingSurface();
		CreateGPUReferenceAndLogicalDevice();
		CreateVKSWCImageViews();
	}

	

	void DestroyVulkanRenderer()
	{
		DestroyRenderInstance();
		DestroyGLFWWindow();
	}

	void DestroyGLFWWindow()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void DestroyRenderInstance()
	{

		for (auto& imageView : swapChainImageViews) {
			vkDestroyImageView(logicalDevice, imageView, nullptr);
		}


		if (swapChain)
		{
			vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
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

		::VKUtils::DestroyDebugUtilsMessengerEXT(instance, nullptr);

		if (instance)
		{
			vkDestroyInstance(instance, nullptr);
		}
	}
private:
	VkInstance instance = VK_NULL_HANDLE;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	VkPhysicalDevice gpu = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;
	int graphicsIdx = -1, presentIdx = -1;

	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	GLFWwindow* window = nullptr;

	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};


	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		::VKUtils::SwapChainSupportDetails supportDetails;

		for (auto& requested : deviceExtensions)
		{
			if (std::find_if(availableExtensions.begin(), availableExtensions.end(), [&requested](auto extension) {
				return strcmp(requested, extension.extensionName) == 0;
				}) == std::end(availableExtensions))
			{
				return false;
			}
		}

		supportDetails = ::VKUtils::querySwapChainSupport(device, renderSurface);

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
			glfwGetFramebufferSize(window, &width, &height);

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

