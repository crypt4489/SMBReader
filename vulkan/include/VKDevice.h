#pragma once

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <set>

#include "vulkan/vulkan.h"
#include "ThreadManager.h"
//
#include "VKTypes.h"
#include "VKSwapChain.h"
#include "VKUtilities.h"

struct DescriptorPoolBuilder
{
	void AddUniformPoolSize(uint32_t size)
	{
		poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, size);
	}

	void AddImageSampler(uint32_t size)
	{
		poolSizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, size);
	}

	std::vector<VkDescriptorPoolSize> poolSizes{};
};



class QueueManager
{
public:

	QueueManager(std::vector<uint32_t> _cqs, int32_t _mqc, uint32_t _qfi, VkCommandPool& _p, VkDevice& _d) :
		bitmap(0U),
		maxQueueCount(_mqc),
		queueFamilyIndex(_qfi),
		pool(_p),
		device(_d),
		sema(Semaphore(_mqc))
	{

		assert(maxQueueCount <= 16);

		for (uint32_t i = 0; i < _cqs.size(); i++)
		{
			bitmap |= (1 << _cqs[i]);
		}
	}

	std::optional<std::tuple<VkQueue, VkCommandPool, int32_t>> GetQueue()
	{
		sema.Wait();
		for (int32_t i = 0; i < maxQueueCount; i++)
		{
			uint32_t mask = (1 << i);
			if ((bitmap & mask) == 0)
			{
				bitmap |= mask;
				VkQueue queue;
				vkGetDeviceQueue(device, queueFamilyIndex, i, &queue);
				return std::tuple<VkQueue, VkCommandPool, int32_t>(queue, pool, i);
			}
		}
		return std::nullopt;
	}

	void ReturnQueue(int32_t queueNum)
	{
		bitmap &= ~(1U << queueNum);
		sema.Notify();
	}
private:
	uint16_t bitmap;
	const int32_t maxQueueCount;
	const uint32_t queueFamilyIndex;
	VkCommandPool& pool;
	VkDevice& device;
	Semaphore sema;
};

class VKDevice
{
public:
	VKDevice(VkPhysicalDevice _gpu) : gpu(_gpu)
	{
		commandPools.resize(2);
		device = VK_NULL_HANDLE;
	}
	~VKDevice()
	{
		if (device)
		{
			vkDeviceWaitIdle(device);

			for (auto& d : descriptorPools)
			{
				vkDestroyDescriptorPool(device, d, nullptr);
			}

			for (auto& c : commandPools)
			{
				vkDestroyCommandPool(device, c, nullptr);
			}

			for (auto& qm : queueManagers)
			{
				delete qm;
			}

			vkDestroyDevice(device, nullptr);
		}
	}

	VKDevice& operator=(const VKDevice& _dev) = default;

	VKDevice& operator=(VKDevice&& _dev) {
		this->device = _dev.device;
		_dev.device = VK_NULL_HANDLE;
		this->gpu = _dev.gpu;
		this->commandPools = std::move(_dev.commandPools);
		this->queueManagers = std::move(_dev.queueManagers);
		return *this;
	};

	VKDevice(const VKDevice& _dev) = default;

	VKDevice(VKDevice&& _dev)
	{
		this->device = _dev.device;
		_dev.device = VK_NULL_HANDLE;
		this->gpu = _dev.gpu;
		this->commandPools = std::move(_dev.commandPools);
		this->queueManagers = std::move(_dev.queueManagers);
	};

	VkPhysicalDevice GetGPU() const
	{
		return gpu;
	}

	void QueueFamilyDetails(std::vector<VkQueueFamilyProperties>& famProps)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

		famProps.resize(queueFamilyCount);

		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, famProps.data());
	}

	int32_t GetPresentQueue(uint32_t &queueIdx, 
		uint32_t &maxQueueCount, 
		std::vector<VkQueueFamilyProperties>& famProps, 
		VkSurfaceKHR &renderSurface)
	{
		uint32_t i = 0;
		for (const auto& props : famProps)
		{
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, renderSurface, &presentSupport);

			if (presentSupport)
			{
				maxQueueCount = props.queueCount;
				queueIdx = i;
				return 0;
			}
			i++;
		}
		return -1;
	}

	int32_t GetQueueByMask(uint32_t& queueIdx,
		uint32_t& maxQueueCount,
		std::vector<VkQueueFamilyProperties>& famProps,
		uint32_t queueMask)
	{
		uint32_t i = 0;
		for (const auto& props : famProps)
		{

			if ((props.queueFlags & queueMask) == queueMask) {
				queueIdx = i;
				maxQueueCount = props.queueCount;
				return 0;
			}
			i++;
		}
		return -1;
	}

	void CreateLogicalDevice(
		std::vector<const char*>& instanceLayers,
		std::vector<const char*>& deviceExtensions,
		std::set<uint32_t>& queueIndices,
		std::vector<uint32_t>& maxQueueCount,
		VkPhysicalDeviceFeatures &features)
	{

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		std::vector<float> queuePriorties(*std::max_element(maxQueueCount.begin(), maxQueueCount.end()), 1.0f);

		uint32_t i = 0;

		for (uint32_t queueFamily : queueIndices) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = maxQueueCount[i++];
			queueCreateInfo.pQueuePriorities = queuePriorties.data();
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkDeviceCreateInfo logDeviceInfo{};
		logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		logDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		logDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
		logDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		logDeviceInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
		logDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
		logDeviceInfo.ppEnabledLayerNames = instanceLayers.data();
		logDeviceInfo.pEnabledFeatures = &features;

		VkResult res = vkCreateDevice(gpu, &logDeviceInfo, nullptr, &device);

		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical GPU, mate!");
		}
	}

	VkQueue GetQueueHandle(uint32_t queueFamily, uint32_t queueIdx)
	{
		VkQueue queue;
		vkGetDeviceQueue(device, queueFamily, queueIdx, &queue);
		return queue;
	}

	QueueManager* CreateQueueManager(uint32_t queueIndex, uint32_t poolIndex, uint32_t maxCount)
	{
		queueManagers.push_back(new QueueManager(std::vector<uint32_t>{0}, maxCount, queueIndex, commandPools[poolIndex], device));
		return queueManagers.back();
	}

	VkCommandPool CreateCommandPool(uint32_t queueIndex, uint32_t& poolIndex)
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueIndex;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPools[poolIndex]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}

		return commandPools[poolIndex];
	}

	VkDescriptorPool CreateDesciptorPool(uint32_t& poolIndex, DescriptorPoolBuilder &builder, uint32_t maxSets)
	{
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		uint32_t poolSizeCount = static_cast<uint32_t>(builder.poolSizes.size());

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizeCount;
		poolInfo.pPoolSizes = builder.poolSizes.data();
		poolInfo.maxSets = maxSets;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}

		descriptorPools.push_back(descriptorPool);
		poolIndex = static_cast<uint32_t>(descriptorPools.size()-1);
		return descriptorPool;
	}

	uint32_t CreateMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
	{
		uint32_t memorytype = ::VK::Utils::findMemoryType(gpu, memoryTypeBits, properties);
		
		auto iter = deviceMemories.find(memorytype);
		if (iter != std::end(deviceMemories))
		{
			return static_cast<uint32_t>(std::distance(deviceMemories.begin(), iter));
		}

		uint32_t ret = static_cast<uint32_t>(deviceMemories.size());
		
		VkDeviceMemory deviceMemory;

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = poolSize;
		allocInfo.memoryTypeIndex = memorytype;

		if (vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		deviceMemories[memorytype] = deviceMemory;

		return ret;
	}

	uint32_t CreateSwapChain(VkSurfaceKHR surface)
	{
		uint32_t index = static_cast<uint32_t>(swapChains.size());
		auto swapchain = swapChains.emplace_back(this, surface);
		return index;
	}

	VkCommandPool GetCommandPool(uint32_t poolIndex)
	{
		return commandPools[poolIndex];
	}

	VkDescriptorPool GetDescriptorPool(uint32_t poolIndex)
	{
		return descriptorPools[poolIndex];
	}

	VKSwapChain& GetSwapChain(uint32_t index)
	{
		return swapChains[index];
	}

	VkDevice device;
	VkPhysicalDevice gpu;
	std::vector<QueueManager*> queueManagers;
	std::vector<VkCommandPool> commandPools;
	std::vector<VkDescriptorPool> descriptorPools;
	std::vector<VKSwapChain> swapChains;
	std::unordered_map<uint32_t, VkDeviceMemory> deviceMemories; //memoryIndex, deviceMemory pool
	std::vector<VkBuffer> deviceBuffers;
};

