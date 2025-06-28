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

	DeviceIndex CreatePhysicalDevice();

	VkSampleCountFlagBits GetMaxMSAALevels(DeviceIndex& gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	VKDevice& CreateLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex);

	VKDevice& GetLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;
	
	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};

	std::vector<VkPhysicalDevice> gpus;
	std::vector<std::pair<VkPhysicalDevice, std::vector<VKDevice>>> logicalDevices;
};

