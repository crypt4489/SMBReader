#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/include/GLFW/glfw3.h"
#include <mutex>

struct VKInstanceAllocator
{
	VKInstanceAllocator() = default;

	uint8_t* instanceData;
	size_t instanceDataSize;
	std::atomic<size_t> instanceDataOffset;
	uint8_t* commandData;
	size_t commandDataSize;
	std::atomic<size_t> commandDataOffset;

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

	VKInstance();
	~VKInstance();

	VkPhysicalDevice GetPhysicalDevice(DeviceIndex& gpuIndex);

	uintptr_t* GetDeviceArray(DeviceIndex& gpuIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(uint32_t gpuIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice gpu);

	void CreateDrawingSurface(GLFWwindow* wind);

	void CreateRenderInstance();

	DeviceIndex CreatePhysicalDevice(uint32_t maxNumberOfLogiclDevices);

	VkSampleCountFlagBits GetMaxMSAALevels(DeviceIndex& gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	VKDevice* CreateLogicalDevice(DeviceIndex gpuIndex, DeviceIndex* deviceIndex);

	VKDevice* GetLogicalDevice(DeviceIndex gpuIndex, DeviceIndex deviceIndex);

	void SetInstanceDataAndSize(size_t totalDataSize, size_t cacheSize);

	void* AllocFromInstanceCache(size_t size);

	void* AllocFromInstanceData(size_t size);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;
	
	const char** instanceLayers;
	const char** instanceExtensions;
	const char** deviceExtensions;

	uint32_t instanceExtCount;
	uint32_t instanceLayerCount;
	uint32_t deviceExtCount;

	uintptr_t instanceTempMemory;
	uintptr_t instancePerMemory;
	std::atomic<size_t> instanceTempOffset = 0;
	std::atomic<size_t> instancePerOffset = 0;
	size_t instancePerSize;
	size_t instanceTempSize;

	uintptr_t *gpusAndLogicalDevices;

	uint32_t physicalDeviceCount = 0;
	uint32_t physicalDeviceCounter = 0;

	
	
	VKInstanceAllocator *allocator;
};

