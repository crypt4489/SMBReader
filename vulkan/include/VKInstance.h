#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"

#ifdef _MSC_VER
#include <Windows.h>
#endif
#undef min
#undef max

#include <mutex>

struct VKAllocationCB
{
	VKAllocationCB() = default;

	uint8_t* instanceData = nullptr;
	size_t instanceDataSize = 0;
	std::atomic<size_t> instanceDataOffset = 0;
	uint8_t* commandData = nullptr;
	size_t commandDataSize = 0;
	std::atomic<size_t> commandDataOffset = 0;

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

	void Delete()
	{
		if (instanceData) delete[] instanceData;
		if (commandData) delete[] commandData;
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

	void* RealAlloc(size_t size,
		size_t alignment, bool cache);

	void* RealRealloc(void* original, size_t size,
		size_t alignment, bool cache);

	//void RealFree(void* memory);


};

enum OperatingSystem
{
	WINDOWS = 0,
	LINUX = 1,
	MAC = 2,
};

struct VKInstance
{


	VKInstance();
	~VKInstance();

	VkPhysicalDevice GetPhysicalDevice(DeviceIndex gpuIndex);

	uintptr_t* GetDeviceArray(DeviceIndex gpuIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(uint32_t gpuIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice gpu);

	bool ValidateSwapChainFormatSupport(uint32_t gpuIndex, VkFormat requestedFormat);

	void CreateRenderInstance(OperatingSystem system, void* dataHead, uint32_t storageSize, uint32_t cacheSize);

	DeviceIndex CreatePhysicalDevice(uint32_t maxNumberOfLogiclDevices);

	VkSampleCountFlagBits GetMaxMSAALevels(DeviceIndex gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	VKDevice* CreateLogicalDevice(DeviceIndex gpuIndex, DeviceIndex* deviceIndex);

	VKDevice* GetLogicalDevice(DeviceIndex gpuIndex, DeviceIndex deviceIndex);

	void SetInstanceDataAndSize(void* dataHead, size_t totalDataSize, size_t cacheSize);

	void* AllocFromInstanceCache(size_t size);

	void* AllocFromInstanceData(size_t size);

	int GetMinimumStorageBufferAlignment(DeviceIndex gpuIndex);

	int GetMinimumUniformBufferAlignment(DeviceIndex gpuIndex);

	void CreateWindowedSurface(HINSTANCE hInst, HWND hWnd);

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

	
	
	VKAllocationCB *allocator;
};

