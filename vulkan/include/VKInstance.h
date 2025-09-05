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


struct VKInstanceAllocator
{

	uint8_t* instanceData;
	size_t instanceDataSize;
	size_t offset;

	VkAllocationCallbacks operator()() const {
		VkAllocationCallbacks res;

		res.pUserData = (void*)this;

		res.pfnAllocation = &Allocation;
		res.pfnReallocation = &Reallocation;
		res.pfnFree = &Free;

		res.pfnInternalAllocation = nullptr;
		res.pfnInternalFree = nullptr;

		return res;
	}

	static void* VKAPI_CALL Allocation(
		void* userData,
		size_t size,
		size_t alignment,
		VkSystemAllocationScope allocationScope
	);


	static void* VKAPI_CALL Reallocation(
		void* userData,
		void *original,
		size_t size,
		size_t alignment,
		VkSystemAllocationScope allocationScope
	);

	static void VKAPI_CALL Free(
		void* userData,
		void* memory
	);

	void* RealAlloc(size_t size,
		size_t alignment, VkSystemAllocationScope allocationScope);

	void* RealRealloc(void* original, size_t size,
		size_t alignment, VkSystemAllocationScope allocationScope);

	void RealFree(void* memory);


};

class VKInstance
{
public:

	VKInstance() = default;
	~VKInstance();

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(uint32_t gpuIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice gpu);

	void CreateDrawingSurface(GLFWwindow* wind);

	void CreateRenderInstance();

	DeviceIndex CreatePhysicalDevice();

	VkSampleCountFlagBits GetMaxMSAALevels(DeviceIndex& gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	VKDevice& CreateLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex);

	VKDevice& GetLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex);

	void SetInstanceDataAndSize(size_t datasize);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;
	
	std::vector<const char*> instanceExtensions{};
	std::vector<const char*> instanceLayers{};
	std::vector<const char*> deviceExtensions{};

	std::vector<VkPhysicalDevice> gpus;
	std::vector<std::pair<VkPhysicalDevice, std::vector<VKDevice>>> logicalDevices;
	
	VKInstanceAllocator allocator;
};

