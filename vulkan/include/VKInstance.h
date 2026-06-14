#pragma once
#include "CommonRenderTypes.h"
#include "IndexTypes.h"
#include "TLSFAllocator.h"
#include "VKTypes.h"
#include "VKUtilities.h"

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

enum InstanceErrorCodeMajor
{
	INSTANCE_HANDLE_CREATION_FAILED = 1,
	INSTANCE_EXTENSION_ENUMERATED_FAILED = 2,
	INSTANCE_REQUESTED_EXTENSION_NOT_AVAILABLE = 3,
	INSTANCE_REQUESTED_LAYER_NOT_AVAILABLE = 4,
	INSTANCE_GPU_ENUMERATION_FAILED = 5,
	INSTANCE_NO_SUITABLE_GPU_FOUND = 6,
	INSTANCE_HANDLE_EXHAUSTION = 7,
	INSTANCE_DEBUG_FUNCTION_POINTER_RETRIEVAL_FAILED = 8,
	INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED = 9,
};

enum InstanceErrorCodeMinor
{
	INSTANCE_HANDLE = 1,
	INSTANCE_RENDER_SURFACE = 2,
	INSTANCE_GPU_ALLOCATION = 3,
	INSTANCE_LOGICAL_DEVICE_ALLOC = 4,
	INSTANCE_DEBUG_MESSENGER = 5,
	INSTANCE_FORMAT_COUNT = 6,
	INSTANCE_PRESENT_MODE_COUNT = 7
};

struct InstanceErrorCodeStruct
{
	int internalErrorCode;
	VkResult vkResult;
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

	void* cacheMemRingBuffer;
	TLSFMain tlsfMain;
	uint32_t cacheMemRingBufferWrite;
	uint32_t cacheMemRingBufferSize;

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

	void* RealAllocCache(size_t size, size_t alignment);

	void RealFree(void* memory);
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

	int CreateRenderInstance(void* dataHead, uint32_t storageSize, uint32_t cacheSize, VKInstanceDebugData* debugData, RenderingInstanceFeatures* featuresRequest);

	EntryHandle CreatePhysicalDevice(EntryHandle renderSurface, GPUFeatureRequest* featureRequest, const char** deviceExtensions, uint32_t deviceExtCount);

	VkSampleCountFlagBits GetMaxMSAALevels(EntryHandle gpuIndex);

	void GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties* deviceProperties, VkPhysicalDeviceFeatures* deviceFeatures);

	bool isDeviceSuitable(VkPhysicalDevice device, const char** deviceExtensions, uint32_t deviceExtCount);

	EntryHandle CreateLogicalDevice(EntryHandle gpuIndex);

	VKDevice* GetLogicalDevice(EntryHandle deviceIndex);

	void SetInstanceDataAndSize(void* dataHead, size_t totalDataSize, size_t cacheSize);

	void* AllocFromInstanceCache(size_t size);

	void* AllocFromInstanceData(size_t size, size_t alignment);

	void FreeFromInstanceData(void* memoryAddress);

	int GetMinimumStorageBufferAlignment(EntryHandle gpuIndex);

	int GetMinimumUniformBufferAlignment(EntryHandle gpuIndex);

	EntryHandle CreateWindowedSurface(HINSTANCE hInst, HWND hWnd);

	double GetTimeStampPeriod(EntryHandle gpuIndex);

	EntryHandle AddTypedHandleToPool(VKInstanceHandleType handleType, void* handlePtr);

	InstanceHandlePoolObject* GetHandle(EntryHandle index);

	void DestroyRenderSurface(EntryHandle index);
	void DestroyLogicalDevice(EntryHandle index);
	void DestroyPhysicalDevice(EntryHandle index);

	EntryHandle CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator);

	void DestroyDebugUtilsMessenger(EntryHandle debugMessengerHandle, const VkAllocationCallbacks* pAllocator);

	void AddInstanceErrorCode(int internalErrorCode, VkResult vulkSpecificResult);

	char* PopErrorOffQueue(int* strLength);

	uint32_t GetLogicalDeviceExtensionsCount(LogicalDeviceFeatures* requestedFeatures);

	void GetLogicalDeviceExtensions(LogicalDeviceFeatures* requestedFeatures, const char** actualHandles);

	VkInstance instance = VK_NULL_HANDLE;
	
	const char** instanceLayers;
	const char** instanceExtensions;

	uintptr_t instanceTempMemory;
	size_t instanceTempOffset = 0;
	size_t instanceTempSize;

	VKAllocationCB *allocator;

	InstanceHandlePoolObject handles[MAX_TYPED_HANDLES];
	
	static const uint32_t errorCodeWrapSize = 16;
	InstanceErrorCodeStruct errorCodes[errorCodeWrapSize];
	uint32_t currentErrorCodePos = 0;
	uint32_t readErrorCodePos = 0;

	uint32_t instanceExtCount;
	uint32_t instanceLayerCount;
	uint32_t handleBumpCounter;

	TLSFMain permanentInstanceMemory;
};

