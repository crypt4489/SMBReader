#pragma once

#include <algorithm>
#include <array>
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


#include "FileManager.h"


class RenderInstance;

namespace {
	namespace VK {

		namespace Renderer {
			RenderInstance* gRenderInstance;
		}

		namespace Utils {

			struct SwapChainSupportDetails {
				VkSurfaceCapabilitiesKHR capabilities{};
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

			uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
				VkPhysicalDeviceMemoryProperties memProperties;
				vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

				for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
					if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
						return i;
					}
				}

				throw std::runtime_error("failed to find suitable memory type!");
			}

			VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
				VkShaderModuleCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.codeSize = code.size();
				createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
				VkShaderModule shaderModule;
				if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
					throw std::runtime_error("failed to create shader module!");
				}
				return shaderModule;
			}

			std::pair<VkBuffer, VkDeviceMemory> createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
				VkDeviceSize bufferSize, 
				VkMemoryPropertyFlags memprops, VkSharingMode sharingMode, 
				VkBufferUsageFlags usage)
			{
				VkBuffer buffer;
				VkDeviceMemory bufferMemory;
				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = bufferSize;
				bufferInfo.sharingMode = sharingMode;
				bufferInfo.usage = usage;
				if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
					throw std::runtime_error("failed to create buffer!");
				}

				VkMemoryRequirements memRequirements;
				vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memprops);

				if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
					throw std::runtime_error("failed to allocate buffer memory!");
				}

				vkBindBufferMemory(device, buffer, bufferMemory, 0);

				return std::make_pair<>(buffer, bufferMemory);
			}

			VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool pool)
			{
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandPool = pool;
				allocInfo.commandBufferCount = 1;

				VkCommandBuffer commandBuffer;
				vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(commandBuffer, &beginInfo);

				return commandBuffer;
			}

			void EndOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer commandBuffer) {
				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(queue);

				vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
			}

			void CopyBuffer(VkDevice device, VkCommandPool pool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
				VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, pool);

				VkBufferCopy copyRegion{};
				copyRegion.size = size;
				vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

				EndOneTimeCommands(device, queue, pool, commandBuffer);
			}

			void CopyBufferToImage(VkDevice device, VkCommandPool pool, VkQueue queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
				VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, pool);

				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;

				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = {
					width,
					height,
					1
				};

				vkCmdCopyBufferToImage(
					commandBuffer,
					buffer,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&region
				);

				EndOneTimeCommands(device, queue, pool, commandBuffer);
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
					!deviceFeatures.samplerAnisotropy)
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
		features.samplerAnisotropy = VK_TRUE;
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

		//::VK::Utils::SwapChainSupportDetails supportDetails = ::VK::Utils::querySwapChainSupport(gpu, renderSurface);
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
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer #" + std::to_string(i) + "!");
			}
		}
	}

	void DestroySwapChain()
	{
		for (auto framebuffer : swapChainFramebuffers) {
			if (framebuffer) vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		}

		for (auto imageView : swapChainImageViews) {
			if (imageView) vkDestroyImageView(logicalDevice, imageView, nullptr);
		}

		if (swapChain) {
			vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
		}
	}

	void RecreateSwapChain() {

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(logicalDevice);

		DestroySwapChain();

		CreateSwapChain();
		CreateVKSWCImageViews();
		CreateFrameBuffers();
	}

	std::vector<char> CreateShader(std::string name)
	{
		auto ret = FileManager::OpenFile(name, std::ios::ate | std::ios::binary | std::ios::in);

		if (!ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

		FileManager::FileHandle handle = ret.value();

		auto stream = handle.second;

		std::streampos fileEnd = stream->tellg();

		std::vector<char> buffer(fileEnd);

		stream->seekg(0);

		stream->read(buffer.data(), fileEnd);

		FileManager::CloseFile(handle.first);

		return buffer;
	}

	void CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = graphicsIdx;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
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

	void RecordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex)
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

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;

		renderPassInfo.pClearValues = &clearColor;

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

		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinesInFlight[0]);

		vkCmdDraw(cb, 4, 1, 0, 0);

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

	void DrawFrame()
	{

		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		RecordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame]};
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

		result = vkQueuePresentKHR(presentQueue, &presentInfo);
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
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void RenderLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			DrawFrame();
		}
		
		vkQueueWaitIdle(graphicsQueue);
		vkQueueWaitIdle(presentQueue);
	}

	void CreateGLFWWindow()
	{
		bool good = glfwInit();
		if (!good) throw std::runtime_error("Cannot create window");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(800, 600, "SMB File Viewer", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, frameResizeCB);

	}

	static void frameResizeCB(GLFWwindow* window, int width, int height)
	{
		auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
		renderInst->SetResizeBool(true);
	}

	void CreateVulkanRenderer()
	{
		CreateGLFWWindow();
		CreateRenderInstance();
		CreateDrawingSurface();
		CreateGPUReferenceAndLogicalDevice();
		CreateDescriptorPool();
		CreateSwapChain();
		CreateVKSWCImageViews();
		CreateRenderPass();
		CreateFrameBuffers();
		CreateCommandPool();
		CreateCommandBuffer();
		CreateSyncObjects();
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

	VkCommandPool GetVulkanCommandPool() const
	{
		return commandPool;
	}

	VkQueue GetGraphicsQueue() const
	{
		return graphicsQueue;
	}

	VkRenderPass GetRenderPass() const
	{
		return renderPass;
	}

	VkDescriptorPool GetDescriptorPool() const
	{
		return descriptorPool;
	}

	void AddPipeline(VkPipeline pipeline)
	{
		if (std::find(pipelinesInFlight.begin(), pipelinesInFlight.end(), pipeline) == std::end(pipelinesInFlight))
			pipelinesInFlight.push_back(pipeline);
	}

	void RemovePipeline(VkPipeline pipeline)
	{
		auto n = std::erase_if(pipelinesInFlight, [pipeline](VkPipeline p) { return p == pipeline; });
		if (!n) std::cout << "Removed pipeline not already added\n";
	}

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

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

	//default shader will expand


	VkRenderPass renderPass = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkCommandPool commandPool = VK_NULL_HANDLE;

	std::vector<VkCommandBuffer> commandBuffers{};

	std::vector<VkSemaphore> imageAvailableSemaphores{};
	std::vector<VkSemaphore> renderFinishedSemaphores{};
	std::vector<VkFence> inFlightFences{};

	GLFWwindow* window = nullptr;

	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};

	VkDescriptorPool descriptorPool;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	std::vector<VkPipeline> pipelinesInFlight;

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

