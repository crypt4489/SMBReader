
#include "pch.h"
#include "VKInstance.h"
#include "VKDevice.h"
#include <vulkan/vulkan.h>

#ifdef _MSC_VER
#include <vulkan/vulkan_win32.h>
#endif

#include <map>

VKInstance::VKInstance()
	: 
	instanceLayers(nullptr),
	instanceExtensions(nullptr),
	deviceExtensions(nullptr),
	instanceExtCount(0),
	instanceLayerCount(0),
	deviceExtCount(0),
	instanceTempMemory(0),
	instanceTempOffset(0),
	instanceTempSize(0),
	instancePerSize(0),
	instancePerMemory(0),
	handleBumpCounter(0)
{
}

VKInstance::~VKInstance() {

	auto callbacks = (*allocator)();

	for (int32_t i = handleBumpCounter; i >= 0; i--)
	{
		InstanceHandlePoolObject* handle = &handles[i];

		if (!handle->handlePtr)
			continue;

		switch (handle->handleType)
		{
		case RENDER_SURFACE:
			DestroyRenderSurface(EntryHandle(i));
			break;
		case PHYSICAL_DEVICE:

			break;
		case LOGICAL_DEVICE:
			DestroyLogicalDevice(EntryHandle(i));
			break;
		case DEBUG_MESSENGER:
			DestroyDebugUtilsMessengerEXT(EntryHandle(i), nullptr);
			break;
		}
	}

	vkDestroyInstance(instance, &callbacks);
}

void VKInstance::DestroyRenderSurface(EntryHandle index)
{
	VkSurfaceKHR renderSurface = GetRenderSurface(index);

	if (!renderSurface)
		return;

	vkDestroySurfaceKHR(instance, renderSurface, nullptr);
}

void VKInstance::DestroyLogicalDevice(EntryHandle index)
{
	VKDevice* device = GetLogicalDevice(index);

	if (!device)
		return;

	device->DestroyDevice();
}

VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(EntryHandle gpuIndex, EntryHandle renderSurfaceIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkSurfaceKHR renderSurface = GetRenderSurface(renderSurfaceIndex);

	if (!gpu || !renderSurface)
		return { };

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * 30);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * 10);

	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface, formatsDataSpace, presentModesDataSpace);
	return ret;
}

VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(VkPhysicalDevice gpu, EntryHandle renderSurfaceIndex)
{
	VkSurfaceKHR renderSurface = GetRenderSurface(renderSurfaceIndex);

	if ( !renderSurface)
		return { };

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * 30);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * 10);
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface, formatsDataSpace, presentModesDataSpace);
	return ret;
}

bool VKInstance::ValidateSwapChainFormatSupport(EntryHandle gpuIndex, VkFormat requestedFormat, EntryHandle renderSurfaceIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkSurfaceKHR renderSurface = GetRenderSurface(renderSurfaceIndex);

	if (!gpu || !renderSurface)
		return false;

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * 30);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * 10);
	
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface, formatsDataSpace, presentModesDataSpace);

	for (int i = 0; i < ret.formatCount; i++)
	{
		VkSurfaceFormatKHR availableFormat = ret.formats[i];
		if (availableFormat.format == requestedFormat && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
		{
			return true;
		}
	}

	return false;
}

EntryHandle VKInstance::CreateWindowedSurface(HINSTANCE hInst, HWND hWnd)
{
	VkSurfaceKHR renderSurface = VK_NULL_HANDLE;

	VkWin32SurfaceCreateInfoKHR infoStruct{};
	infoStruct.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	infoStruct.hinstance = hInst;
	infoStruct.hwnd = hWnd;

	VkResult vkResult = VK_SUCCESS;

	if ((vkResult = vkCreateWin32SurfaceKHR(instance, &infoStruct, nullptr, &renderSurface)) != VK_SUCCESS)
	{
		//("failed to create window surface!");
		return EntryHandle();
	}

	EntryHandle allocIndex = AddTypedHandleToPool(RENDER_SURFACE, renderSurface);

	return allocIndex;
}

double VKInstance::GetTimeStampPeriod(EntryHandle gpuIndex)
{	
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(gpu, &props);

	double timestampPeriod = props.limits.timestampPeriod;

	return timestampPeriod;
}

int VKInstance::CreateRenderInstance(OperatingSystem system, void* dataHead, uint32_t storageSize, uint32_t cacheSize, VKInstanceDebugData* debugData)
{

	VkResult vkResult = VK_SUCCESS;

	instanceTempMemory = reinterpret_cast<uintptr_t>(dataHead);

	instancePerMemory = instanceTempMemory + cacheSize;

	instanceTempSize = cacheSize;
	instancePerSize = storageSize;
	
	instancePerOffset = 0;
	instanceTempOffset = 0;

	VkApplicationInfo appInfoStruct{};

	appInfoStruct.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfoStruct.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfoStruct;
	


	instanceExtCount = 5;

	instanceLayerCount = 1;

	instanceLayers = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*)*instanceLayerCount));

	instanceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * instanceExtCount));

	instanceLayers[0] = "VK_LAYER_KHRONOS_validation";


	instanceExtensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
	instanceExtensions[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

	instanceExtensions[2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	instanceExtensions[3] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
	instanceExtensions[4] = VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
//	instanceExtensions[5] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
//	instanceExtensions[5] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
	
	uint32_t instExtensionRequired = 0;

	vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, nullptr);

	if (vkResult != VK_SUCCESS || !instExtensionRequired)
	{
		return -1;
	}

	VkExtensionProperties* extensions = reinterpret_cast<VkExtensionProperties*>(AllocFromInstanceCache(sizeof(VkExtensionProperties) * instExtensionRequired));

	vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, extensions);
	
	for (uint32_t i = 0; i < instanceExtCount; i++)
	{
		const char* requested = instanceExtensions[i];
	
		uint32_t j = 0;

		for (; j < instExtensionRequired; j++)
		{
			VkExtensionProperties props = extensions[j];
			if (strcmp(requested, props.extensionName) == 0) {
				break;
			}
		}

		if (j == instExtensionRequired)
		{
			return -1;
		}
	}

	uint32_t layersCount = 0;

	vkResult = vkEnumerateInstanceLayerProperties(&layersCount, nullptr);

	if (vkResult != VK_SUCCESS || !layersCount)
	{
		return -1;
	}

	VkLayerProperties* layerProps = reinterpret_cast<VkLayerProperties*>(AllocFromInstanceCache(sizeof(VkLayerProperties) * layersCount));

	vkEnumerateInstanceLayerProperties(&layersCount, layerProps);

	for (uint32_t i = 0; i < instanceLayerCount; i++)
	{
		const char* requested = instanceLayers[i];

		uint32_t j = 0;
		for (; j < layersCount; j++)
		{
			VkLayerProperties props = layerProps[j];
			if (strcmp(requested, props.layerName) == 0) {
				break;
			}
		}

		if (j == layersCount)
		{
			return -1;
		}
	}


	createInfo.enabledExtensionCount = instanceExtCount;
	createInfo.ppEnabledExtensionNames = instanceExtensions;

	createInfo.ppEnabledLayerNames = instanceLayers;
	createInfo.enabledLayerCount = instanceLayerCount;

	VkDebugUtilsMessengerCreateInfoEXT instanceDebugInfo{};

	if (debugData)
	{
		VkValidationFeaturesEXT validationFeatures{};
		validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		validationFeatures.enabledValidationFeatureCount = debugData->enablesFeaturesCount;
		validationFeatures.pEnabledValidationFeatures = debugData->enables;

		createInfo.pNext = &validationFeatures;

		instanceDebugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		instanceDebugInfo.messageSeverity = debugData->messageSeverity;
		instanceDebugInfo.messageType = debugData->messageType;
		instanceDebugInfo.flags = debugData->flags;
		instanceDebugInfo.pfnUserCallback = debugData->userCallback;
		instanceDebugInfo.pUserData = debugData->userData;

		validationFeatures.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&instanceDebugInfo;
	}

	VkAllocationCallbacks allocationCBs = (*allocator)();

	vkResult = vkCreateInstance(&createInfo, &allocationCBs, &instance);

	if (vkResult != VK_SUCCESS) 
	{
		//("failed to create instance!");

		return -1;
	}

	if (debugData)
	{
		EntryHandle debugMessengerHandle = CreateDebugUtilsMessengerEXT(&instanceDebugInfo, nullptr);

		if (debugMessengerHandle == EntryHandle())
		{
			vkDestroyInstance(instance, &allocationCBs);
			//("Cannot establish debug callback");
			return -1;
		}
	}

	return 0;
}

EntryHandle VKInstance::CreatePhysicalDevice(EntryHandle renderSurface) 
{
	deviceExtCount = 4;

	deviceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * deviceExtCount));

	deviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	
	deviceExtensions[1] = VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;

	deviceExtensions[2] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;

	deviceExtensions[3] = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME;

	uint32_t physicalDeviceCount = 0;

	VkResult result = VK_SUCCESS; 
	
	if (((result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE)) != VK_SUCCESS) || !physicalDeviceCount)
	{
		//("failed to get device pointers");
		return EntryHandle();
	}

	VkPhysicalDevice* physicalDevices = reinterpret_cast<VkPhysicalDevice*>(AllocFromInstanceCache(sizeof(VkPhysicalDevice) * physicalDeviceCount));

	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

	std::multimap<int, VkPhysicalDevice> gpuScores;

	for (uint32_t i = 0; i<physicalDeviceCount; i++)
	{
		VkPhysicalDevice potentGPU = physicalDevices[i];

		GetPhysicalDevicePropertiesandFeatures(potentGPU, deviceProperties, deviceFeatures);

		if (!isDeviceSuitable(potentGPU)) continue;

		if (renderSurface != EntryHandle())
		{
			VK::Utils::SwapChainSupportDetails supportDetails = GetSwapChainSupport(potentGPU, renderSurface);

			if (!supportDetails.formatCount || !supportDetails.presentModeCount)
				continue;
		}

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
				!deviceFeatures.samplerAnisotropy ||
				!deviceFeatures.multiDrawIndirect)
			{
				score = std::numeric_limits<int>::min();
			}

			return score;
			}(), potentGPU));
	}

	auto bestCase = gpuScores.rbegin();

	if (gpuScores.empty() || bestCase->first <= 0)
	{
		//("No suitable gpu found for this");
		return EntryHandle();
	}

	VkPhysicalDevice gpu = bestCase->second;

	PhysicalDeviceAllocation* gpuAlloc = (PhysicalDeviceAllocation*)AllocFromInstanceData(sizeof(PhysicalDeviceAllocation));

	gpuAlloc->gpuDeviceHandle = gpu;
	gpuAlloc->logicalDeviceCount = 0;

	EntryHandle allocIndexInStorage = AddTypedHandleToPool(PHYSICAL_DEVICE, gpuAlloc);

	return allocIndexInStorage;
}

VkSampleCountFlagBits VKInstance::GetMaxMSAALevels(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(gpu, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	return VK_SAMPLE_COUNT_1_BIT;
}

void VKInstance::GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties& deviceProperties, VkPhysicalDeviceFeatures& deviceFeatures)
{
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
}

bool VKInstance::isDeviceSuitable(VkPhysicalDevice device)
{
	uint32_t extensionCount;

	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	VkExtensionProperties* availableExtensions = reinterpret_cast<VkExtensionProperties*>(AllocFromInstanceCache(sizeof(VkExtensionProperties) * extensionCount));
	
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions);

	for (uint32_t i = 0; i<deviceExtCount; i++)
	{
		const char* ext = deviceExtensions[i];

		uint32_t j = 0;
		for (; j < extensionCount; j++)
		{
			VkExtensionProperties prop2 = availableExtensions[j];
	
			if (strcmp(ext, prop2.extensionName) == 0)
			{
				break;
			}
		}

		if (j == extensionCount)
		{
			return false;
		}
	}

	
	return true;
}

EntryHandle VKInstance::CreateLogicalDevice(EntryHandle gpuIndex)
{	
	PhysicalDeviceAllocation* gpu = GetPhysicalDeviceAlloc(gpuIndex);
	
	VKDevice* device = reinterpret_cast<VKDevice*>(AllocFromInstanceData(sizeof(VKDevice)));

	device = std::construct_at(device, gpu->gpuDeviceHandle, this);

	uint32_t gpuOwnerIndex = gpu->logicalDeviceCount++;

	EntryHandle logicalDeviceHandle = AddTypedHandleToPool(LOGICAL_DEVICE, device);

	gpu->logicalDevicesIndex[gpuOwnerIndex] = logicalDeviceHandle;

	return logicalDeviceHandle;
}

VkPhysicalDevice VKInstance::GetPhysicalDevice(EntryHandle gpuIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(gpuIndex);

	if (handlePoolObject->handleType != PHYSICAL_DEVICE || !handlePoolObject->handlePtr)
	{
		return VK_NULL_HANDLE;
	}

	PhysicalDeviceAllocation* alloc = (PhysicalDeviceAllocation*)handlePoolObject->handlePtr;

	return alloc->gpuDeviceHandle;
}

PhysicalDeviceAllocation* VKInstance::GetPhysicalDeviceAlloc(EntryHandle gpuIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(gpuIndex);

	if (handlePoolObject->handleType != PHYSICAL_DEVICE || !handlePoolObject->handlePtr)
	{
		return VK_NULL_HANDLE;
	}

	PhysicalDeviceAllocation* alloc = (PhysicalDeviceAllocation*)handlePoolObject->handlePtr;

	return alloc;
}

VkSurfaceKHR VKInstance::GetRenderSurface(EntryHandle renderSurfaceIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(renderSurfaceIndex);

	if (handlePoolObject->handleType != RENDER_SURFACE || !handlePoolObject->handlePtr)
	{
		return VK_NULL_HANDLE;
	}

	VkSurfaceKHR surface = (VkSurfaceKHR)handlePoolObject->handlePtr;

	return surface;
}

VKDevice* VKInstance::GetLogicalDevice(EntryHandle deviceIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(deviceIndex);

	if (handlePoolObject->handleType != LOGICAL_DEVICE || !handlePoolObject->handlePtr)
	{
		return nullptr;
	}

	VKDevice* device = (VKDevice*)handlePoolObject->handlePtr;

	return device;
}

VkDebugUtilsMessengerEXT VKInstance::GetDebugMessenger(EntryHandle debugMessengerHandle)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(debugMessengerHandle);

	if (handlePoolObject->handleType != DEBUG_MESSENGER || !handlePoolObject->handlePtr)
	{
		return VK_NULL_HANDLE;
	}

	VkDebugUtilsMessengerEXT debugMessenger = (VkDebugUtilsMessengerEXT)handlePoolObject->handlePtr;

	return debugMessenger;
}

void VKInstance::SetInstanceDataAndSize(void* dataHead, size_t totalDataSize, size_t cacheSize)
{
	uintptr_t tempMemoryHead = (uintptr_t)dataHead;

	allocator = (VKAllocationCB*)(tempMemoryHead);

	tempMemoryHead += sizeof(VKAllocationCB);

	allocator->instanceDataSize = totalDataSize-cacheSize;
	allocator->commandDataSize = cacheSize;
	allocator->instanceDataOffset = 0;
	allocator->commandDataOffset = 0;
	allocator->instanceData = (uint8_t*)tempMemoryHead;
	allocator->commandData = allocator->instanceData + allocator->instanceDataSize;
}


void* VKAPI_CALL VKAllocationCB::Allocation(
	void* userData,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKAllocationCB* allocator = (VKAllocationCB*)userData;
	return allocator->RealAlloc(size, alignment, allocationScope);
}


void* VKAPI_CALL VKAllocationCB::Reallocation(
	void* userData,
	void* original,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKAllocationCB* allocator = (VKAllocationCB*)userData;
	return allocator->RealRealloc(original, size, alignment, allocationScope);
}

void VKAPI_CALL VKAllocationCB::Free(
	void* userData,
	void* memory
)
{
	VKAllocationCB* allocator = (VKAllocationCB*)userData;
	allocator->RealFree(memory);
}

void* VKAllocationCB::RealAlloc(size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	uintptr_t head = 0;
	size_t val, desired, out;
	size = (size + alignment - 1) & ~(alignment - 1);
	if (allocationScope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND)
	{
		head = (uintptr_t)commandData;

		val = commandDataOffset.load(std::memory_order_relaxed);

		do {

			desired = val + size;
			out = val;
			if (desired >= commandDataSize)
			{
				out = 0;
				desired = out + size;
			}
		} while (!commandDataOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));
		
		head += out;
	}
	else 
	{
		head = (uintptr_t)instanceData;
		
		val = instanceDataOffset.load(std::memory_order_relaxed);

		do {
			desired = val + size;
			out = val;
		} while (!instanceDataOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));

		head += out;
	}
	
	return (void*)head;
}

void* VKAllocationCB::RealRealloc(void* original, size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	void* newaddr = RealAlloc(size, alignment, allocationScope);
	memcpy(newaddr, original, size);
	return newaddr;
}

void VKAllocationCB::RealFree(void* memory)
{

}

void* VKAllocationCB::RealAlloc(size_t size,
	size_t alignment,
	bool cache)
{
	uintptr_t head = 0;
	size_t val, desired, out;
	size = (size + alignment - 1) & ~(alignment - 1);
	if (cache)
	{
		head = (uintptr_t)commandData;

		val = commandDataOffset.load(std::memory_order_relaxed);

		do {
			desired = val + size;
			out = val;
			if (desired >= commandDataSize)
			{
				out = 0;
				desired = out + size;
			}
		} while (!commandDataOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));

		head += out;
	}
	else
	{
		head = (uintptr_t)instanceData;

		val = instanceDataOffset.load(std::memory_order_relaxed);

		do {
			desired = val + size;
			out = val;
		} while (!instanceDataOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));

		head += out;
	}

	return (void*)head;
}

void* VKAllocationCB::RealRealloc(void* original, size_t size,
	size_t alignment,
	bool cache)
{
	void* newaddr = RealAlloc(size, alignment, cache);
	memcpy(newaddr, original, size);
	return newaddr;
}

int VKInstance::GetMinimumStorageBufferAlignment(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

	GetPhysicalDevicePropertiesandFeatures(gpu, deviceProperties, deviceFeatures);

	return static_cast<int>(deviceProperties.limits.minStorageBufferOffsetAlignment);
}

int VKInstance::GetMinimumUniformBufferAlignment(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

	GetPhysicalDevicePropertiesandFeatures(gpu, deviceProperties, deviceFeatures);

	return static_cast<int>(deviceProperties.limits.minUniformBufferOffsetAlignment);
}

void* VKInstance::AllocFromInstanceCache(size_t size)
{
	size_t val, desired, out;
	val = instanceTempOffset.load(std::memory_order_relaxed);
	do {
		desired = val + size;
		out = val;
		if ((val + size) >= instanceTempSize)
		{
			out = 0;
			desired = out + size;
		}
	} while (!instanceTempOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));


	uintptr_t head = instanceTempMemory + out;

	return reinterpret_cast<void*>(head);
}

void* VKInstance::AllocFromInstanceData(size_t size)
{
	size_t val, desired, out;
	val = instancePerOffset.load(std::memory_order_relaxed);
	do {

		desired = val + size;
		out = val;
	} while (!instancePerOffset.compare_exchange_weak(val, desired, std::memory_order_relaxed, std::memory_order_relaxed));

	uintptr_t head = instancePerMemory + out;

	return reinterpret_cast<void*>(head);
}

EntryHandle VKInstance::AddTypedHandleToPool(VKInstanceHandleType handleType, void* handlePtr)
{
	if (handleBumpCounter == MAX_TYPED_HANDLES)
		return EntryHandle();

	uint32_t currHandleIndex = handleBumpCounter++;

	InstanceHandlePoolObject* handlePoolObject = &handles[currHandleIndex];

	handlePoolObject->handleType = handleType;
	handlePoolObject->handlePtr = (uintptr_t)handlePtr;

	return EntryHandle(currHandleIndex);
}

InstanceHandlePoolObject* VKInstance::GetHandle(EntryHandle index)
{
	if (index() >= handleBumpCounter)
		return nullptr;

	return &handles[index()];
}



EntryHandle VKInstance::CreateDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{	
	PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

	VkDebugUtilsMessengerEXT debugMessenger;

	EntryHandle debugHandle = EntryHandle();

	if (func)
	{
		 VkResult vkResult = func(instance, pCreateInfo, pAllocator, &debugMessenger);

		 if (vkResult == VK_SUCCESS)
		 {
			 debugHandle = AddTypedHandleToPool(DEBUG_MESSENGER, debugMessenger);
		 }
		 else
		 {

		 }
	}
	else
	{

	}
	
	return debugHandle;
}

void VKInstance::DestroyDebugUtilsMessengerEXT(EntryHandle debugMessengerHandle, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

	VkDebugUtilsMessengerEXT debugMessenger = GetDebugMessenger(debugMessengerHandle);

	if (func) 
	{
		func(instance, debugMessenger, pAllocator);
	}
}