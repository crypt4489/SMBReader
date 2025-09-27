
#include "pch.h"
#include "VKInstance.h"

#include "VKDevice.h"


#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

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
	gpusAndLogicalDevices(nullptr),
	allocator(new VKDriverAllocator())
{
}

VKInstance::~VKInstance() {

	auto callbacks = (*allocator)();

	vkDestroySurfaceKHR(instance, renderSurface, nullptr);

	VK::Utils::DestroyDebugUtilsMessengerEXT(instance, nullptr);

	vkDestroyInstance(instance, &callbacks);

	if (instanceTempMemory)
	{
		delete[](void*)instanceTempMemory;
	}

	if (allocator->instanceData)
	{
		delete[] allocator->instanceData;
	}
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


VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(uint32_t gpuIndex)
{
	uintptr_t* devices = reinterpret_cast<uintptr_t*>(gpusAndLogicalDevices[gpuIndex]);
	VkPhysicalDevice gpu = (VkPhysicalDevice)devices[0];
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface);
	return ret;
}

VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(VkPhysicalDevice gpu)
{
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpu, renderSurface);
	return ret;
}

void VKInstance::CreateDrawingSurface(GLFWwindow* wind)
{
	VkResult res = glfwCreateWindowSurface(instance, wind, nullptr, &renderSurface);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}


#define TEMPCACHESIZE 512 * 1024
#define PERCAHCESIZE 256 * 1024

void VKInstance::CreateRenderInstance()
{
	char* data = new char[TEMPCACHESIZE + PERCAHCESIZE];
	
	instanceTempMemory = reinterpret_cast<uintptr_t>(data);
	instancePerMemory = reinterpret_cast<uintptr_t>(data + TEMPCACHESIZE);

	instanceTempSize = TEMPCACHESIZE;
	instancePerSize = PERCAHCESIZE;
	
	instancePerOffset = 0;
	instanceTempOffset = 0;

	VkApplicationInfo appInfoStruct{};

	appInfoStruct.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfoStruct.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfoStruct;

	uint32_t glfwReqExtCount;

	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwReqExtCount);

	instanceExtCount = glfwReqExtCount + 1;

	instanceLayerCount = 1;

	instanceLayers = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*)*instanceLayerCount));

	instanceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * instanceExtCount));

	instanceLayers[0] = "VK_LAYER_KHRONOS_validation";

	for (uint32_t i = 0; i < glfwReqExtCount; i++)
	{
		instanceExtensions[i] = glfwExtensions[i];
	}

	instanceExtensions[2] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	
	uint32_t instExtensionRequired = 0;

	vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, nullptr);

	if (!instExtensionRequired)
	{
		throw std::runtime_error("Need extension layers, found none");
	}

	VkExtensionProperties* extensions = reinterpret_cast<VkExtensionProperties*>(AllocFromInstanceCache(sizeof(VkExtensionProperties) * instExtensionRequired));

	if (vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, extensions) != VK_SUCCESS)
	{
		throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed second time");
	}
	

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

	VkLayerProperties* layerProps = reinterpret_cast<VkLayerProperties*>(AllocFromInstanceCache(sizeof(VkLayerProperties) * layersCount));

	if (vkEnumerateInstanceLayerProperties(&layersCount, layerProps) != VK_SUCCESS)
	{
		throw std::runtime_error("vkEnumerateInstanceLayerProperties failed second time");
	}

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
			std::cerr << "Validation layer " << requested << " not available\n";
			throw std::runtime_error("Cannot find validation layer");
		}
	}


	createInfo.enabledExtensionCount = instanceExtCount;
	createInfo.ppEnabledExtensionNames = instanceExtensions;

	createInfo.ppEnabledLayerNames = instanceLayers;
	createInfo.enabledLayerCount = instanceLayerCount;

	VkDebugUtilsMessengerCreateInfoEXT instanceDebugInfo{};
	instanceDebugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	instanceDebugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	instanceDebugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	instanceDebugInfo.pfnUserCallback = VK::Utils::debugCallback;
	instanceDebugInfo.pUserData = nullptr;


	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&instanceDebugInfo;

	auto ret = (*allocator)();

	VkResult result = vkCreateInstance(&createInfo, &ret, &instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}

	VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugInfo.pfnUserCallback = VK::Utils::debugCallback;
	debugInfo.pUserData = nullptr;

	result = VK::Utils::CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot establish debug callback");
	}

	result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to get device pointers");
	}

	gpusAndLogicalDevices = reinterpret_cast<uintptr_t*>(AllocFromInstanceData(sizeof(uintptr_t) * physicalDeviceCount));
}

DeviceIndex VKInstance::CreatePhysicalDevice(uint32_t maxNumberOfLogiclDevices)
{
	uintptr_t* devices = reinterpret_cast<uintptr_t*>(AllocFromInstanceData(sizeof(uintptr_t) * (maxNumberOfLogiclDevices + 1)));

	memset(devices, 0, sizeof(uintptr_t) * (maxNumberOfLogiclDevices + 1));

	deviceExtCount = 1;

	deviceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * deviceExtCount));

	deviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkPhysicalDevice* physicalDevices = reinterpret_cast<VkPhysicalDevice*>(AllocFromInstanceCache(sizeof(VkPhysicalDevice) * physicalDeviceCount));

	VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to get device pointers");
	}


	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

	std::multimap<int, VkPhysicalDevice> gpuScores;

	for (uint32_t i = 0; i<physicalDeviceCount; i++)
	{
		VkPhysicalDevice potentGPU = physicalDevices[i];

		GetPhysicalDevicePropertiesandFeatures(potentGPU, deviceProperties, deviceFeatures);

		if (!isDeviceSuitable(potentGPU)) continue;

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
		throw std::runtime_error("No suitable gpu found for this");
	}

	VkPhysicalDevice gpu = bestCase->second;


	
	devices[0] = reinterpret_cast<uintptr_t>(gpu);

	gpusAndLogicalDevices[physicalDeviceCounter] = reinterpret_cast<uintptr_t>(devices);

	return DeviceIndex(physicalDeviceCounter++);
}



VkSampleCountFlagBits VKInstance::GetMaxMSAALevels(DeviceIndex& gpuIndex)
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

	if (renderSurface)
	{
		VK::Utils::SwapChainSupportDetails supportDetails;

		supportDetails = VK::Utils::querySwapChainSupport(device, renderSurface);

		if (supportDetails.formats.empty() || supportDetails.presentModes.empty())
		{
			return false;
		}
	}
	return true;
}

VKDevice* VKInstance::CreateLogicalDevice(DeviceIndex gpuIndex, DeviceIndex* deviceIndex)
{
	uintptr_t* devices = GetDeviceArray(gpuIndex);
	
	VkPhysicalDevice gpu = reinterpret_cast<VkPhysicalDevice>(devices[0]);

	uintptr_t* iter = &devices[1];

	uint32_t i = 0;

	while (iter[0])
	{
		i++;
		iter = &iter[1];
	}
	
	VKDevice* device = reinterpret_cast<VKDevice*>(AllocFromInstanceData(sizeof(VKDevice)));

	device = std::construct_at(device, gpu, this);

	iter[0] = reinterpret_cast<uintptr_t>(device);

	*deviceIndex = DeviceIndex(i);

	return device;
}

VKDevice* VKInstance::GetLogicalDevice(DeviceIndex gpuIndex, DeviceIndex deviceIndex)
{
	uintptr_t* devices = GetDeviceArray(gpuIndex);
	VKDevice* dev = reinterpret_cast<VKDevice*>(devices[1]);
	return dev;
}

VkPhysicalDevice VKInstance::GetPhysicalDevice(DeviceIndex& gpuIndex)
{
	uintptr_t* devices = reinterpret_cast<uintptr_t*>(gpusAndLogicalDevices[gpuIndex()]);
	return reinterpret_cast<VkPhysicalDevice>(devices[0]);
}

uintptr_t* VKInstance::GetDeviceArray(DeviceIndex& gpuIndex)
{
	return reinterpret_cast<uintptr_t*>(gpusAndLogicalDevices[gpuIndex()]);;
}

void VKInstance::SetInstanceDataAndSize(size_t totalDataSize, size_t cacheSize)
{
	allocator->instanceDataSize = totalDataSize-cacheSize;
	allocator->instanceDataOffset = 0;
	allocator->instanceData = new uint8_t[totalDataSize];
	allocator->commandDataSize = cacheSize;
	allocator->commandDataOffset = 0;
	allocator->commandData = allocator->instanceData + allocator->instanceDataSize;
}


void* VKAPI_CALL VKDriverAllocator::Allocation(
	void* userData,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKDriverAllocator* allocator = (VKDriverAllocator*)userData;
	return allocator->RealAlloc(size, alignment, allocationScope);
}


void* VKAPI_CALL VKDriverAllocator::Reallocation(
	void* userData,
	void* original,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKDriverAllocator* allocator = (VKDriverAllocator*)userData;
	return allocator->RealRealloc(original, size, alignment, allocationScope);
}

void VKAPI_CALL VKDriverAllocator::Free(
	void* userData,
	void* memory
)
{
	VKDriverAllocator* allocator = (VKDriverAllocator*)userData;
	allocator->RealFree(memory);
}

void* VKDriverAllocator::RealAlloc(size_t size,
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

void* VKDriverAllocator::RealRealloc(void* original, size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	void* newaddr = RealAlloc(size, alignment, allocationScope);
	memcpy(newaddr, original, size);
	return newaddr;
}

void VKDriverAllocator::RealFree(void* memory)
{

}

void* VKDriverAllocator::RealAlloc(size_t size,
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

void* VKDriverAllocator::RealRealloc(void* original, size_t size,
	size_t alignment,
	bool cache)
{
	void* newaddr = RealAlloc(size, alignment, cache);
	memcpy(newaddr, original, size);
	return newaddr;
}