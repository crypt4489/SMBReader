
#include "pch.h"
#include "VKInstance.h"
#include "VKDevice.h"
#include <vulkan/vulkan.h>

#ifdef _MSC_VER
#include <vulkan/vulkan_win32.h>
#endif


static uint32_t ConvertGPUDeviceTypeToVkPhysicalDeviceType(uint32_t type)
{
	uint32_t retType = 0;
	
	if (type & GPUDeviceType::INTEGRATED)	
		retType |= (1<<VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

	if (type & GPUDeviceType::DISCRETE)
		retType |= (1<<VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	
	if (type & GPUDeviceType::VIRTUAL)
		retType |= (1<<VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);

	if (type & GPUDeviceType::CPU)
		retType |= (1<<VK_PHYSICAL_DEVICE_TYPE_CPU);
	
	return retType;
}

VKInstance::VKInstance()
	: 
	instanceLayers(nullptr),
	instanceExtensions(nullptr),
	instanceExtCount(0),
	instanceLayerCount(0),
	instanceTempMemory(0),
	instanceTempOffset(0),
	instanceTempSize(0),
	instancePerSize(0),
	instancePerMemory(0),
	handleBumpCounter(0),
	allocator(nullptr)
{
}

VKInstance::~VKInstance() {

	VkAllocationCallbacks callbacks = (*allocator)();

	for (int32_t i = handleBumpCounter-1; i >= 0; i--)
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
			DestroyDebugUtilsMessenger(EntryHandle(i), nullptr);
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

	uint32_t formatCount = 0, presentModeCount = 0;

	VkResult vkResult[2]{};

	int retCode = VK::Utils::querySwapChainSupportCount(gpu, renderSurface, &formatCount, &presentModeCount, vkResult);

	if (retCode == -1)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_FORMAT_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[0]);
		return {};
	}

	if (retCode == -2)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_PRESENT_MODE_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[1]);
		return {};
	}

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * formatCount);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * presentModeCount);

	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface, formatsDataSpace, presentModesDataSpace);

	return ret;
}

VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(VkPhysicalDevice gpu, EntryHandle renderSurfaceIndex)
{
	VkSurfaceKHR renderSurface = GetRenderSurface(renderSurfaceIndex);

	if ( !renderSurface)
		return { };


	uint32_t formatCount = 0, presentModeCount = 0;

	VkResult vkResult[2]{};

	int retCode = VK::Utils::querySwapChainSupportCount(gpu, renderSurface, &formatCount, &presentModeCount, vkResult);

	if (retCode == -1)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_FORMAT_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[0]);
		return {};
	}

	if (retCode == -2)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_PRESENT_MODE_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[1]);
		return {};
	}

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * formatCount);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * presentModeCount);

	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface, formatsDataSpace, presentModesDataSpace);

	return ret;
}

bool VKInstance::ValidateSwapChainFormatSupport(EntryHandle gpuIndex, VkFormat requestedFormat, EntryHandle renderSurfaceIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkSurfaceKHR renderSurface = GetRenderSurface(renderSurfaceIndex);

	if (!gpu || !renderSurface)
		return false;

	uint32_t formatCount = 0, presentModeCount = 0;

	VkResult vkResult[2]{};

	int retCode = VK::Utils::querySwapChainSupportCount(gpu, renderSurface, &formatCount, &presentModeCount, vkResult);

	if (retCode == -1)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_FORMAT_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[0]);
		return false;
	}

	if (retCode == -2)
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_PRESENT_MODE_COUNT) | INSTANCE_QUERY_SWAPCHAIN_SUPPORT_FAILED, vkResult[1]);
		return false;
	}

	VkSurfaceFormatKHR* formatsDataSpace = (VkSurfaceFormatKHR*)AllocFromInstanceCache(sizeof(VkSurfaceFormatKHR) * formatCount);
	VkPresentModeKHR* presentModesDataSpace = (VkPresentModeKHR*)AllocFromInstanceCache(sizeof(VkPresentModeKHR) * presentModeCount);

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
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_RENDER_SURFACE) | INSTANCE_HANDLE_CREATION_FAILED, vkResult);
		return EntryHandle();
	}

	EntryHandle allocIndex = AddTypedHandleToPool(RENDER_SURFACE, renderSurface);

	if (allocIndex == EntryHandle())
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_RENDER_SURFACE) | INSTANCE_HANDLE_EXHAUSTION, vkResult);
		vkDestroySurfaceKHR(instance, renderSurface, nullptr);
	}

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

int VKInstance::CreateRenderInstance(void* dataHead, uint32_t storageSize, uint32_t cacheSize, VKInstanceDebugData* debugData, RenderingInstanceFeatures* featuresRequest)
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
	
	instanceLayerCount = (featuresRequest->useValidation ? 1 : 0);

	instanceLayers = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*)*instanceLayerCount));

	instanceLayers[0] = "VK_LAYER_KHRONOS_validation";

	instanceExtCount = 0;

	if (featuresRequest->useSurface)
	{
		instanceExtCount += 2;
	}

	if (featuresRequest->useSwapChainMaintenance)
	{
		instanceExtCount += 2;
	}

	if (featuresRequest->useDebugExt)
	{
		instanceExtCount += 1;
	}

	instanceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * instanceExtCount));

	uint32_t instIndex = 0;

	if (featuresRequest->useSurface)
	{
		instanceExtensions[instIndex++] = VK_KHR_SURFACE_EXTENSION_NAME;
		switch (featuresRequest->windowManagementType)
		{
		case WindowManagementType::WINDOWS32:
			instanceExtensions[instIndex++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			break;
		default:
			return -1;
		}
	}

	if (featuresRequest->useSwapChainMaintenance)
	{
		instanceExtensions[instIndex++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
		instanceExtensions[instIndex++] = VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
	}

	if (featuresRequest->useDebugExt)
	{
		instanceExtensions[instIndex++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}
	
	uint32_t instExtensionRequired = 0;

	vkResult = vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, nullptr);

	if (vkResult != VK_SUCCESS || !instExtensionRequired)
	{
		AddInstanceErrorCode(INSTANCE_EXTENSION_ENUMERATED_FAILED, vkResult);
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
			AddInstanceErrorCode(MINOR_CODE_PACK(i) | INSTANCE_REQUESTED_EXTENSION_NOT_AVAILABLE, VK_RESULT_MAX_ENUM);
			return -1;
		}
	}

	uint32_t layersCount = 0;

	vkResult = vkEnumerateInstanceLayerProperties(&layersCount, nullptr);

	if (vkResult != VK_SUCCESS || !layersCount)
	{
		AddInstanceErrorCode(INSTANCE_EXTENSION_ENUMERATED_FAILED, vkResult);
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
			AddInstanceErrorCode(MINOR_CODE_PACK(i) | INSTANCE_REQUESTED_LAYER_NOT_AVAILABLE, VK_RESULT_MAX_ENUM);
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
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_HANDLE) | INSTANCE_HANDLE_CREATION_FAILED, vkResult);
		return -1;
	}

	if (debugData)
	{
		EntryHandle debugMessengerHandle = CreateDebugUtilsMessenger(&instanceDebugInfo, nullptr);

		if (debugMessengerHandle == EntryHandle())
		{
			vkDestroyInstance(instance, &allocationCBs);
			return -1;
		}
	}

	return 0;
}

EntryHandle VKInstance::CreatePhysicalDevice(EntryHandle renderSurface, GPUFeatureRequest* featureRequest, const char** deviceExtensions, uint32_t deviceExtCount)
{
	uint32_t physicalDeviceCount = 0;

	VkResult vkResult = VK_SUCCESS; 
	
	if (((vkResult = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE)) != VK_SUCCESS) || !physicalDeviceCount)
	{
		AddInstanceErrorCode(INSTANCE_GPU_ENUMERATION_FAILED, vkResult);
		return EntryHandle();
	}

	VkPhysicalDevice* physicalDevices = reinterpret_cast<VkPhysicalDevice*>(AllocFromInstanceCache(sizeof(VkPhysicalDevice) * physicalDeviceCount));

	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

	VkPhysicalDeviceProperties deviceProperties{};

	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features12;

	int* scores = (int*)AllocFromInstanceCache(sizeof(int) * physicalDeviceCount);

	memset(scores, 0, sizeof(int) * physicalDeviceCount);

	for (uint32_t i = 0; i<physicalDeviceCount; i++)
	{
		VkPhysicalDevice potentGPU = physicalDevices[i];

		vkGetPhysicalDeviceFeatures2(potentGPU, &features2);
		vkGetPhysicalDeviceProperties(potentGPU, &deviceProperties);

		if (!isDeviceSuitable(potentGPU, deviceExtensions, deviceExtCount)) 
			continue;

		if (renderSurface != EntryHandle())
		{
			VK::Utils::SwapChainSupportDetails supportDetails = GetSwapChainSupport(potentGPU, renderSurface);

			if (!supportDetails.formatCount || !supportDetails.presentModeCount)
				continue;
		}

		int score = 0;

		if ((1<<deviceProperties.deviceType) & ConvertGPUDeviceTypeToVkPhysicalDeviceType(featureRequest->deviceType))
			score = std::numeric_limits<short>::max();

		score += deviceProperties.limits.maxImageDimension2D;

		if (deviceProperties.limits.maxImageDimension2D >= featureRequest->desiredMaxImageWidth ||
			deviceProperties.limits.maxImageDimension2D >= featureRequest->desiredMaxImageHeight)
			score += 1000;

		bool meetsRequirements = true;

		if (featureRequest->requireDescriptorBindingPartiallyBound &&
			!features12.descriptorBindingPartiallyBound)
			meetsRequirements = false;

		if (featureRequest->requireDescriptorBindingSampledImageUpdateAfterBind &&
			!features12.descriptorBindingSampledImageUpdateAfterBind)
			meetsRequirements = false;

		if (featureRequest->requireDescriptorBindingUpdateUnusedWhilePending &&
			!features12.descriptorBindingUpdateUnusedWhilePending)
			meetsRequirements = false;

		if (featureRequest->requireDescriptorBindingVariableDescriptorCount &&
			!features12.descriptorBindingVariableDescriptorCount)
			meetsRequirements = false;

		if (featureRequest->requireShaderSampledImageArrayNonUniformIndexing &&
			!features12.shaderSampledImageArrayNonUniformIndexing)
			meetsRequirements = false;

		if (featureRequest->requireStorageBuffer8BitAccess &&
			!features12.storageBuffer8BitAccess)
			meetsRequirements = false;

		if (featureRequest->requireDrawIndirectCount &&
			!features12.drawIndirectCount)
			meetsRequirements = false;

		if (featureRequest->requireRuntimeDescriptorArray &&
			!features12.runtimeDescriptorArray)
			meetsRequirements = false;

		if (featureRequest->requireGeometryShader &&
			!features2.features.geometryShader)
			meetsRequirements = false;

		if (featureRequest->requireTextureCompressionBC &&
			!features2.features.textureCompressionBC)
			meetsRequirements = false;

		if (featureRequest->requireTessellationShader &&
			!features2.features.tessellationShader)
			meetsRequirements = false;

		if (featureRequest->requireSamplerAnisotropy &&
			!features2.features.samplerAnisotropy)
			meetsRequirements = false;

		if (featureRequest->requireMultiDrawIndirect &&
			!features2.features.multiDrawIndirect)
			meetsRequirements = false;

		if (featureRequest->requireWideLines &&
			!features2.features.wideLines)
			meetsRequirements = false;

		if (!meetsRequirements)
			score = std::numeric_limits<int>::min();

		scores[i] = score;
	}

	int bestScore = 0, bestScoreIndex = -1;

	for (uint32_t i = 0; i < physicalDeviceCount; i++)
	{
		if (bestScore < scores[i])
		{
			bestScoreIndex = i;
			bestScore = scores[i];
		}
	}

	if (bestScoreIndex < 0)
	{
		AddInstanceErrorCode(INSTANCE_NO_SUITABLE_GPU_FOUND, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkPhysicalDevice gpu = physicalDevices[bestScoreIndex];

	PhysicalDeviceAllocation* gpuAlloc = (PhysicalDeviceAllocation*)AllocFromInstanceData(sizeof(PhysicalDeviceAllocation));

	gpuAlloc->gpuDeviceHandle = gpu;
	gpuAlloc->logicalDeviceCount = 0;

	EntryHandle allocIndexInStorage = AddTypedHandleToPool(PHYSICAL_DEVICE, gpuAlloc);

	if (allocIndexInStorage == EntryHandle())
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_GPU_ALLOCATION) | INSTANCE_HANDLE_EXHAUSTION, VK_RESULT_MAX_ENUM);

	return allocIndexInStorage;
}

VkSampleCountFlagBits VKInstance::GetMaxMSAALevels(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(gpu, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT)  return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT)  return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT)  return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT; 
	if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT; 
	if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT; 

	return VK_SAMPLE_COUNT_1_BIT;
}

void VKInstance::GetPhysicalDevicePropertiesandFeatures(VkPhysicalDevice device, VkPhysicalDeviceProperties* deviceProperties, VkPhysicalDeviceFeatures* deviceFeatures)
{
	if (deviceProperties)
		vkGetPhysicalDeviceProperties(device, deviceProperties);

	if (deviceFeatures)
		vkGetPhysicalDeviceFeatures(device, deviceFeatures);
}

bool VKInstance::isDeviceSuitable(VkPhysicalDevice device, const char** deviceExtensions, uint32_t deviceExtCount)
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
				break;
		}

		if (j == extensionCount)
			return false;
	}

	
	return true;
}

EntryHandle VKInstance::CreateLogicalDevice(EntryHandle gpuIndex)
{	
	PhysicalDeviceAllocation* gpu = GetPhysicalDeviceAlloc(gpuIndex);
	
	VKDevice* device = reinterpret_cast<VKDevice*>(AllocFromInstanceData(sizeof(VKDevice)));

	EntryHandle logicalDeviceHandle = AddTypedHandleToPool(LOGICAL_DEVICE, device);

	if (logicalDeviceHandle == EntryHandle())
	{
		AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_LOGICAL_DEVICE_ALLOC) | INSTANCE_HANDLE_EXHAUSTION, VK_RESULT_MAX_ENUM);
		return logicalDeviceHandle;
	}

	std::construct_at(device, gpu->gpuDeviceHandle, this);

	uint32_t gpuOwnerIndex = gpu->logicalDeviceCount++;

	gpu->logicalDevicesIndex[gpuOwnerIndex] = logicalDeviceHandle;

	return logicalDeviceHandle;
}

VkPhysicalDevice VKInstance::GetPhysicalDevice(EntryHandle gpuIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(gpuIndex);

	if (!handlePoolObject || handlePoolObject->handleType != PHYSICAL_DEVICE || !handlePoolObject->handlePtr)
		return VK_NULL_HANDLE;

	PhysicalDeviceAllocation* alloc = (PhysicalDeviceAllocation*)handlePoolObject->handlePtr;

	return alloc->gpuDeviceHandle;
}

PhysicalDeviceAllocation* VKInstance::GetPhysicalDeviceAlloc(EntryHandle gpuIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(gpuIndex);

	if (!handlePoolObject || handlePoolObject->handleType != PHYSICAL_DEVICE || !handlePoolObject->handlePtr)
		return VK_NULL_HANDLE;

	PhysicalDeviceAllocation* alloc = (PhysicalDeviceAllocation*)handlePoolObject->handlePtr;

	return alloc;
}

VkSurfaceKHR VKInstance::GetRenderSurface(EntryHandle renderSurfaceIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(renderSurfaceIndex);

	if (!handlePoolObject || handlePoolObject->handleType != RENDER_SURFACE || !handlePoolObject->handlePtr)
		return VK_NULL_HANDLE;

	VkSurfaceKHR surface = (VkSurfaceKHR)handlePoolObject->handlePtr;

	return surface;
}

VKDevice* VKInstance::GetLogicalDevice(EntryHandle deviceIndex)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(deviceIndex);

	if (!handlePoolObject || handlePoolObject->handleType != LOGICAL_DEVICE || !handlePoolObject->handlePtr)
		return nullptr;

	VKDevice* device = (VKDevice*)handlePoolObject->handlePtr;

	return device;
}

VkDebugUtilsMessengerEXT VKInstance::GetDebugMessenger(EntryHandle debugMessengerHandle)
{
	InstanceHandlePoolObject* handlePoolObject = GetHandle(debugMessengerHandle);

	if (!handlePoolObject || handlePoolObject->handleType != DEBUG_MESSENGER || !handlePoolObject->handlePtr)
	{
		return VK_NULL_HANDLE;
	}

	VkDebugUtilsMessengerEXT debugMessenger = (VkDebugUtilsMessengerEXT)handlePoolObject->handlePtr;

	return debugMessenger;
}

void VKInstance::SetInstanceDataAndSize(void* dataHead, size_t totalDataSize, size_t cacheSize)
{
	uintptr_t tempMemoryHead = (uintptr_t)dataHead;
	uintptr_t memStart = tempMemoryHead;

	allocator = (VKAllocationCB*)(tempMemoryHead);

	tempMemoryHead += sizeof(VKAllocationCB);

	Initialize(&allocator->tlsfMain, (void*)tempMemoryHead, totalDataSize - (tempMemoryHead - memStart), 5);
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
	void* addr = Allocate(&tlsfMain, size, alignment);
	return addr;
}

void* VKAllocationCB::RealRealloc(void* original, size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	void* newaddr = Realloc(&tlsfMain, original, size);
	return newaddr;
}

void VKAllocationCB::RealFree(void* memory)
{
	TLSFFree(&tlsfMain, memory);
}

int VKInstance::GetMinimumStorageBufferAlignment(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkPhysicalDeviceProperties deviceProperties;

	GetPhysicalDevicePropertiesandFeatures(gpu, &deviceProperties, nullptr);

	return static_cast<int>(deviceProperties.limits.minStorageBufferOffsetAlignment);
}

int VKInstance::GetMinimumUniformBufferAlignment(EntryHandle gpuIndex)
{
	VkPhysicalDevice gpu = GetPhysicalDevice(gpuIndex);

	VkPhysicalDeviceProperties deviceProperties;

	GetPhysicalDevicePropertiesandFeatures(gpu, &deviceProperties, nullptr);

	return static_cast<int>(deviceProperties.limits.minUniformBufferOffsetAlignment);
}

void* VKInstance::AllocFromInstanceCache(size_t size)
{
	size_t newHead = instanceTempOffset + size, out = instanceTempOffset;

	if (newHead >= instanceTempSize)
	{
		out = 0;
		newHead = size;
	}

	uintptr_t head = instanceTempMemory + out;

	instanceTempOffset = newHead;

	return reinterpret_cast<void*>(head);
}

void* VKInstance::AllocFromInstanceData(size_t size)
{
	size_t newHead = instancePerOffset+size, out = instancePerOffset;

	if (newHead >= instancePerSize)
		return nullptr;

	uintptr_t head = instancePerMemory + out;

	instancePerOffset = newHead;

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

EntryHandle VKInstance::CreateDebugUtilsMessenger(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{	
	PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

	VkDebugUtilsMessengerEXT debugMessenger;

	EntryHandle debugHandle = EntryHandle();

	VkResult vkResult = VK_SUCCESS;

	if (func)
	{
		 vkResult = func(instance, pCreateInfo, pAllocator, &debugMessenger);

		 if (vkResult == VK_SUCCESS)
		 {
			 debugHandle = AddTypedHandleToPool(DEBUG_MESSENGER, debugMessenger);
			 
			 if (debugHandle == EntryHandle())
			 {
				 AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_DEBUG_MESSENGER) | INSTANCE_HANDLE_EXHAUSTION, VK_RESULT_MAX_ENUM);
			 }
		 }
		 else
		 {
			 AddInstanceErrorCode(MINOR_CODE_PACK(INSTANCE_DEBUG_MESSENGER) | INSTANCE_HANDLE_CREATION_FAILED, vkResult);
		 }
	}
	else
	{
		AddInstanceErrorCode(INSTANCE_DEBUG_FUNCTION_POINTER_RETRIEVAL_FAILED, VK_RESULT_MAX_ENUM);
	}
	
	return debugHandle;
}

void VKInstance::DestroyDebugUtilsMessenger(EntryHandle debugMessengerHandle, const VkAllocationCallbacks* pAllocator)
{
	PFN_vkDestroyDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

	VkDebugUtilsMessengerEXT debugMessenger = GetDebugMessenger(debugMessengerHandle);

	if (!debugMessenger)
		return;

	if (func) 
	{
		func(instance, debugMessenger, pAllocator);
		return;
	}

	AddInstanceErrorCode(INSTANCE_DEBUG_FUNCTION_POINTER_RETRIEVAL_FAILED, VK_RESULT_MAX_ENUM);
}

void VKInstance::AddInstanceErrorCode(int internalErrorCode, VkResult vulkSpecificResult)
{
	int instErrorCodePos = currentErrorCodePos;

	currentErrorCodePos = (currentErrorCodePos + 1) & (errorCodeWrapSize - 1);

	InstanceErrorCodeStruct* errorCode = &errorCodes[instErrorCodePos];

	errorCode->internalErrorCode = internalErrorCode;
	errorCode->vkResult = vulkSpecificResult;
}

uint32_t VKInstance::GetLogicalDeviceExtensionsCount(LogicalDeviceFeatures* requestedFeatures)
{
	uint32_t ldeviceExtCount = 0;

	if (requestedFeatures->useSwapChain)
	{
		ldeviceExtCount++;
	}

	if (requestedFeatures->useSwapChainMaintenance)
	{
		ldeviceExtCount++;
	}

	if (requestedFeatures->useSPVDrawParameters)
	{
		ldeviceExtCount++;
	}

	if (requestedFeatures->useSPVDebugInfo)
	{
		ldeviceExtCount++;
	}

	return ldeviceExtCount;
}

void VKInstance::GetLogicalDeviceExtensions(LogicalDeviceFeatures* requestedFeatures, const char** actualHandles)
{
	uint32_t lDeviceIndex = 0;

	if (requestedFeatures->useSwapChain)
	{
		actualHandles[lDeviceIndex++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	}

	if (requestedFeatures->useSwapChainMaintenance)
	{
		actualHandles[lDeviceIndex++] = VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
	}

	if (requestedFeatures->useSPVDrawParameters)
	{
		actualHandles[lDeviceIndex++] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;
	}

	if (requestedFeatures->useSPVDebugInfo)
	{
		actualHandles[lDeviceIndex++] = VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME;
	}
}