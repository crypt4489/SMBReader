#include "VKInstance.h"

VKInstance::~VKInstance() {

	for (auto&& ref : logicalDevices)
	{
		ref.second.clear();
	}

	if (renderSurface)
	{
		vkDestroySurfaceKHR(instance, renderSurface, nullptr);
	}

	VK::Utils::DestroyDebugUtilsMessengerEXT(instance, nullptr);

	if (instance)
	{
		vkDestroyInstance(instance, nullptr);
	}
}

VK::Utils::SwapChainSupportDetails VKInstance::GetSwapChainSupport(uint32_t gpuIndex)
{
	VK::Utils::SwapChainSupportDetails ret = VK::Utils::querySwapChainSupport(gpus[gpuIndex], renderSurface);
	return ret;
}

void VKInstance::CreateDrawingSurface(GLFWwindow* wind)
{
	VkResult res = glfwCreateWindowSurface(instance, wind, nullptr, &renderSurface);

	if (res != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void VKInstance::CreateRenderInstance()
{
	VkApplicationInfo appInfoStruct{};

	appInfoStruct.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfoStruct.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfoStruct;


	instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

	uint32_t glfwReqExtCount;

	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwReqExtCount);

	for (uint32_t i = 0; i < glfwReqExtCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	uint32_t instExtensionRequired = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, nullptr);

	if (!instExtensionRequired)
	{
		throw std::runtime_error("Need extension layers, found none");
	}

	std::vector<VkExtensionProperties> extensions(instExtensionRequired);

	if (vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionRequired, extensions.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("vkEnumerateInstanceExtensionProperties failed second time");
	}
	;

	for (auto& requested : instanceExtensions)
	{
		if (std::find_if(extensions.begin(), extensions.end(), [&requested](auto i) {
			return strcmp(requested, i.extensionName) == 0;
			}) == std::end(extensions))
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

	std::vector<VkLayerProperties> layerProps(layersCount);

	if (vkEnumerateInstanceLayerProperties(&layersCount, layerProps.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("vkEnumerateInstanceLayerProperties failed second time");
	}

	for (auto& requested : instanceLayers)
	{
		if (std::find_if(layerProps.begin(), layerProps.end(), [&requested](auto i) {
			return strcmp(requested, i.layerName) == 0;
			}) == std::end(layerProps))
		{
			std::cerr << "Validation layer " << requested << " not available\n";
			throw std::runtime_error("Cannot find validation layer");
		}
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	createInfo.ppEnabledLayerNames = instanceLayers.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());

	VkDebugUtilsMessengerCreateInfoEXT instanceDebugInfo{};
	instanceDebugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	instanceDebugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	instanceDebugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	instanceDebugInfo.pfnUserCallback = VK::Utils::debugCallback;
	instanceDebugInfo.pUserData = nullptr;


	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&instanceDebugInfo;

	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

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

uint32_t VKInstance::CreatePhysicalDevice()
{
	uint32_t physicalDeviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to get device count");
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

	result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to get device pointers");
	}

	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;

	std::multimap<int, VkPhysicalDevice> gpuScores;

	for (const auto& i : physicalDevices)
	{
		GetPhysicalDevicePropertiesandFeatures(i, deviceProperties, deviceFeatures);

		if (!isDeviceSuitable(i)) continue;

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
			}(), i));
	}

	auto bestCase = gpuScores.rbegin();

	if (gpuScores.empty() || bestCase->first <= 0)
	{
		throw std::runtime_error("No suitable gpu found for this");
	}

	VkPhysicalDevice gpu = bestCase->second;

	gpus.push_back(gpu);

	return static_cast<uint32_t>(gpus.size() - 1);
}



VkSampleCountFlagBits VKInstance::GetMaxMSAALevels(uint32_t gpuIndex)
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

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());



	for (auto& requested : deviceExtensions)
	{
		if (std::find_if(availableExtensions.begin(), availableExtensions.end(), [&requested](auto extension) {
			return strcmp(requested, extension.extensionName) == 0;
			}) == std::end(availableExtensions))
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

VKDevice& VKInstance::CreateLogicalDevice(uint32_t gpuIndex, uint32_t& deviceIndex)
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
		deviceIndex = 0;
		std::vector<VKDevice> devices;
		logicalDevices.emplace_back(gpu, std::vector<VKDevice>{});
		auto& ref = logicalDevices.back().second;
		return ref.emplace_back(gpu);
	}

	auto& ref = iter->second;
	ref.emplace_back(gpu);
	return ref.back();
}

VKDevice& VKInstance::GetLogicalDevice(uint32_t gpuIndex, uint32_t deviceIndex)
{
	return logicalDevices[gpuIndex].second[deviceIndex];
}