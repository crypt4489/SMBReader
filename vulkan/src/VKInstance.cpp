#include "VKInstance.h"




VKInstance::~VKInstance() {


	for (auto& ref : logicalDevices)
	{
		for (auto& ref2 : ref.second)
		{
			ref2.WaitOnDevice();
		}

		ref.second.clear();
	}

	if (allocator.instanceData)
	{
		delete[] allocator.instanceData;
	}

	if (instanceTempMemory)
	{
		delete[](void*)instanceTempMemory;
	}

}

void* VKInstance::AllocFromInstanceCache(size_t size)
{
	if ((instanceTempOffset + size) >= instanceTempSize)
	{
		instanceTempOffset = instanceTempBase;
	}

	uintptr_t head = instanceTempMemory + instanceTempOffset;

	instanceTempOffset += size;

	return reinterpret_cast<void*>(head);
}


void* VKInstance::AllocFromInstanceData(size_t size)
{
	uintptr_t head = instanceTempMemory + instanceTempBase;

	instanceTempBase += size;

	if (instanceTempBase > instanceTempOffset)
		instanceTempOffset = instanceTempBase;

	return reinterpret_cast<void*>(head);
}


VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(uint32_t gpuIndex)
{
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpus[gpuIndex], renderSurface);
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

void VKInstance::CreateRenderInstance()
{

	instanceTempMemory = reinterpret_cast<uintptr_t>(new char[TEMPCACHESIZE]);
	instanceTempSize = TEMPCACHESIZE;
	instanceTempBase = 0;
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

	auto ret = allocator();

	VkResult result = vkCreateInstance(&createInfo, &ret, &instance);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}

	VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugInfo.pfnUserCallback = VK::Utils::debugCallback;
	debugInfo.pUserData = nullptr;

	result = VK::Utils::CreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot establish debug callback");
	}
}

DeviceIndex VKInstance::CreatePhysicalDevice()
{
	uint32_t physicalDeviceCount = 0;

	deviceExtCount = 1;

	deviceExtensions = reinterpret_cast<const char**>(AllocFromInstanceData(sizeof(char*) * deviceExtCount));

	deviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to get device count");
	}

	VkPhysicalDevice* physicalDevices = reinterpret_cast<VkPhysicalDevice*>(AllocFromInstanceCache(sizeof(VkPhysicalDevice) * physicalDeviceCount));

	result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

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

	gpus.push_back(gpu);

	return DeviceIndex(gpus.size() - 1);
}



VkSampleCountFlagBits VKInstance::GetMaxMSAALevels(DeviceIndex& gpuIndex)
{
	VkPhysicalDevice gpu = gpus[gpuIndex];
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

VKDevice& VKInstance::CreateLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex)
{
	auto& gpu = gpus[gpuIndex];
	auto iter = logicalDevices.begin();
	while (iter != std::end(logicalDevices))
	{
		if (iter->first == gpu) break;
		iter = iter++;
	}

	if (iter == std::end(logicalDevices))
	{
		deviceIndex = std::move(DeviceIndex(0U));
		std::vector<VKDevice> devices;
		logicalDevices.emplace_back(gpu, std::vector<VKDevice>{});
		auto& ref = logicalDevices.back().second;
		return ref.emplace_back(gpu, this);
	}

	auto& ref = iter->second;
	ref.emplace_back(gpu, this);
	return ref.back();
}

VKDevice& VKInstance::GetLogicalDevice(DeviceIndex& gpuIndex, DeviceIndex& deviceIndex)
{
	return logicalDevices[gpuIndex].second[deviceIndex];
}

void VKInstance::SetInstanceDataAndSize(size_t datasize)
{
	allocator.instanceDataSize = datasize;
	
	allocator.instanceData = new uint8_t[datasize];
}


void* VKAPI_CALL VKInstanceAllocator::Allocation(
	void* userData,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKInstanceAllocator* allocator = (VKInstanceAllocator*)userData;
	return allocator->RealAlloc(size, alignment, allocationScope);
}


void* VKAPI_CALL VKInstanceAllocator::Reallocation(
	void* userData,
	void* original,
	size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope
)
{
	VKInstanceAllocator* allocator = (VKInstanceAllocator*)userData;
	return allocator->RealRealloc(original, size, alignment, allocationScope);
}

void VKAPI_CALL VKInstanceAllocator::Free(
	void* userData,
	void* memory
)
{
	VKInstanceAllocator* allocator = (VKInstanceAllocator*)userData;
	allocator->RealFree(memory);
}

void* VKInstanceAllocator::RealAlloc(size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	uintptr_t head = (uintptr_t)instanceData + offset;
	size_t makeup = (head & (alignment - 1));
	head += makeup;
	offset += (size+makeup);
	return (void*)head;
}

void* VKInstanceAllocator::RealRealloc(void* original, size_t size,
	size_t alignment,
	VkSystemAllocationScope allocationScope)
{
	void* newaddr = RealAlloc(size, alignment, allocationScope);
	memcpy(newaddr, original, size);
	return newaddr;
}

void VKInstanceAllocator::RealFree(void* memory)
{

}