#pragma once

#include "GLFW/include/GLFW/glfw3.h"


#include <iostream>
#include <vector>
#include <limits>
#include <map>

#include <vulkan/vulkan.h>


namespace {
	namespace VKUtils {
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
			int i;

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
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func) {
				return func(instance, pCreateInfo, pAllocator, &debugMessenger);
			}
			else {
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
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

		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwReqExtCount);
		
		for (int i = 0; i < glfwReqExtCount; i++)
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

		
		createInfo.enabledExtensionCount = instanceExtensions.size();
		createInfo.ppEnabledExtensionNames = instanceExtensions.data();

		createInfo.ppEnabledLayerNames = instanceLayers.data();
		createInfo.enabledLayerCount = instanceLayers.size();

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

		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;

		std::multimap<int, VkPhysicalDevice> gpuScores;


		for (const auto& i : physicalDevices)
		{
			GetPhysicalDevicePropertiesandFeatures(i, deviceProperties, deviceFeatures);

			gpuScores.insert(std::pair<int, VkPhysicalDevice>([&deviceProperties, &deviceFeatures]() {
				int score = 0;

				if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					score += std::numeric_limits<short>::max();
				}

				score += deviceProperties.limits.maxImageDimension2D;

				if (!deviceFeatures.geometryShader || !deviceFeatures.tessellationShader || !deviceFeatures.textureCompressionBC)
				{
					score = std::numeric_limits<int>::min();
				}

				return score;
				}(), i));

			::VKUtils::operator<<(std::cout, deviceProperties);
		}

		auto bestCase = gpuScores.rbegin();

		if (bestCase->first <= 0)
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

		int famIdx = 0;

		auto selectQueue = std::find_if(famProps.begin(), famProps.end(), [&famIdx](auto& props)
			{

				if (props.queueFlags & VK_QUEUE_TRANSFER_BIT && props.queueFlags & VK_QUEUE_COMPUTE_BIT && props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					return true;
				famIdx++;
				return false;
			});

		if (selectQueue == famProps.end())
		{
			throw std::runtime_error("Cannot find a device with a queue that has COMPUTE, TRANSFER and GRAPHICS bits set");
		}

		::VKUtils::operator<<(std::cout, *selectQueue);

		VkDeviceQueueCreateInfo deviceQueueInfo{};
		deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueInfo.queueFamilyIndex = famIdx;
		deviceQueueInfo.queueCount = 1;

		float queuePriority = 1.0f;
		deviceQueueInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures features{};
		features.geometryShader = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.tessellationShader = VK_TRUE;
		//features.multiDrawIndirect = VK_TRUE;

		VkDeviceCreateInfo logDeviceInfo{};
		logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		logDeviceInfo.queueCreateInfoCount = 1;
		logDeviceInfo.pQueueCreateInfos = &deviceQueueInfo;
		logDeviceInfo.enabledExtensionCount = 0;
		logDeviceInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
		logDeviceInfo.ppEnabledExtensionNames = nullptr;
		logDeviceInfo.ppEnabledLayerNames = instanceLayers.data();
		logDeviceInfo.pEnabledFeatures = &features;

		VkResult res = vkCreateDevice(gpu, &logDeviceInfo, nullptr, &logicalDevice);

		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical GPU, mate!");
		}

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

	void DestroyGLFWWindow()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void DestroyRenderInstance()
	{

		if (logicalDevice)
		{
			vkDestroyDevice(logicalDevice, nullptr);
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
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	GLFWwindow* window = nullptr;
	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
};

