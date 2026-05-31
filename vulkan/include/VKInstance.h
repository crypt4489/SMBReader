#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"

#include <atomic>

#ifdef _MSC_VER
#include <Windows.h>
#endif

#undef min
#undef max

#define MAX_TYPED_HANDLES 15

#define MAX_LOGICAL_DEVICES_PER_PHYSICAL_DEVICE 8

enum VKInstanceHandleType : uint64_t
{
	RENDER_SURFACE = 1,
	PHYSICAL_DEVICE = 2,
	LOGICAL_DEVICE = 3,
	DEBUG_MESSENGER = 4,
	MAX_INST_HANDLE
};

struct InstanceHandlePoolObject
{
	VKInstanceHandleType handleType;
	uintptr_t handlePtr;
};

struct PhysicalDeviceAllocation
{
	VkPhysicalDevice gpuDeviceHandle;
	EntryHandle logicalDevicesIndex[MAX_LOGICAL_DEVICES_PER_PHYSICAL_DEVICE];
	uint32_t logicalDeviceCount;
	uint32_t pad;
};

#define MAX_VALID_FEATURES_ENABLE 8

struct VKInstanceDebugData
{
	PFN_vkDebugUtilsMessengerCallbackEXT userCallback;
	void* userData;
	VkValidationFeatureEnableEXT enables[MAX_VALID_FEATURES_ENABLE];
	uint32_t enablesFeaturesCount;
	VkDebugUtilsMessengerCreateFlagsEXT flags;
	VkDebugUtilsMessageSeverityFlagsEXT messageSeverity;
	VkDebugUtilsMessageTypeFlagsEXT messageType;
};


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

	VkPhysicalDevice GetPhysicalDevice(EntryHandle gpuIndex);

	PhysicalDeviceAllocation* GetPhysicalDeviceAlloc(EntryHandle gpuIndex);

	VkSurfaceKHR GetRenderSurface(EntryHandle renderSurfaceIndex);

	VkDebugUtilsMessengerEXT GetDebugMessenger(EntryHandle debugMessengerHandle);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(EntryHandle gpuIndex, EntryHandle renderSurfaceIndex);

	VK::Utils::SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice gpu, EntryHandle renderSurfaceIndex);

	bool ValidateSwapChainFormatSupport(EntryHandle gpuIndex, VkFormat requestedFormat, EntryHandle renderSurfaceIndex);

	int CreateRenderInstance(OperatingSystem system, void* dataHead, uint32_t storageSize, uint32_t cacheSize, VKInstanceDebugData* debugData);

	EntryHandle CreatePhysicalDevice(EntryHandle renderSurfaceIndex);

	VkSampleCountFlagBits GetMaxMSAALevels(EntryHandle gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device);

	EntryHandle CreateLogicalDevice(EntryHandle gpuIndex);

	VKDevice* GetLogicalDevice(EntryHandle deviceIndex);

	void SetInstanceDataAndSize(void* dataHead, size_t totalDataSize, size_t cacheSize);

	void* AllocFromInstanceCache(size_t size);

	void* AllocFromInstanceData(size_t size);

	int GetMinimumStorageBufferAlignment(EntryHandle gpuIndex);

	int GetMinimumUniformBufferAlignment(EntryHandle gpuIndex);

	EntryHandle CreateWindowedSurface(HINSTANCE hInst, HWND hWnd);

	double GetTimeStampPeriod(EntryHandle gpuIndex);

	EntryHandle AddTypedHandleToPool(VKInstanceHandleType handleType, void* handlePtr);

	InstanceHandlePoolObject* GetHandle(EntryHandle index);

	void DestroyRenderSurface(EntryHandle index);
	void DestroyLogicalDevice(EntryHandle index);

	EntryHandle CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator);

	void DestroyDebugUtilsMessengerEXT(EntryHandle debugMessengerHandle, const VkAllocationCallbacks* pAllocator);

	VkInstance instance = VK_NULL_HANDLE;
	
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

	VKAllocationCB *allocator;

	InstanceHandlePoolObject handles[MAX_TYPED_HANDLES];
	uint32_t handleBumpCounter;
};

