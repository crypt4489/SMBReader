#pragma once
#include "vulkan/vulkan.h"
#include "VKTypes.h"
#include "VKDevice.h"

#include "VKUtilities.h"
#include "WindowManager.h"


#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

class VKInstance
{
public:

	VKInstance() = default;
	~VKInstance();

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(uint32_t gpuIndex);

	void CreateDrawingSurface(GLFWwindow* wind);

	void CreateRenderInstance();

	uint32_t CreatePhysicalDevice();

	VkSampleCountFlagBits GetMaxMSAALevels(uint32_t gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	VKDevice& CreateLogicalDevice(uint32_t gpuIndex, uint32_t& deviceIndex);

	VKDevice& GetLogicalDevice(uint32_t gpuIndex, uint32_t deviceIndex);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;
	
	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};

	std::vector<VkPhysicalDevice> gpus;
	std::vector<std::pair<VkPhysicalDevice, std::vector<VKDevice>>> logicalDevices;
};

