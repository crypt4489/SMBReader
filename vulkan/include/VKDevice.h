#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <forward_list>
#include <map>
#include <numeric>

#include <unordered_map>
#include <set>

#include "vulkan/vulkan.h"
#include "ThreadManager.h"
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


class VKAllocator
{
public:
	VkDeviceSize capacity;
	std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>> freeList; // [staringAddr, endingAddr)
	std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>> occupiedList; //[staringAddr, endingAddr)

	VKAllocator(const VkDeviceSize _c) : capacity(_c)
	{
		auto firstRange = std::make_pair(0U, capacity);
		freeList.emplace_front(firstRange);
	}

	VKAllocator(const VKAllocator&) = delete;

	VKAllocator& operator=(const VKAllocator&) = delete;

	VKAllocator(VKAllocator&&) = default;

	VKAllocator& operator=(VKAllocator&&) = default;

	void InsertSorted(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list, 
		VkDeviceSize first, 
		VkDeviceSize last)
	{
		auto prev = list.before_begin();
		auto traverse = list.begin();
		while (traverse != std::end(list) && first > traverse->first)
		{
			prev = traverse++;
		}
		list.emplace_after(prev, first, last);
	}

	void InsertSortedMerged(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list,
		VkDeviceSize first,
		VkDeviceSize last)
	{
		auto prev = list.before_begin();
		
		auto traverse = list.begin();
		
		while (traverse != std::end(list) && (first) > traverse->second)
		{
			prev = traverse++;
		}

		while (traverse != std::end(list) && traverse->first <= last)
		{
			first = std::min(first, traverse->first);
			last = std::max(last, traverse->second);

			traverse = list.erase_after(prev);
		}

		list.emplace_after(prev, first, last);
	}

	std::pair<VkDeviceSize, VkDeviceSize> GetBestFit(VkDeviceSize size, VkDeviceSize alignment)
	{
		VkDeviceSize maxDiff = UINT64_MAX;
		auto prev = freeList.before_begin();
		auto iter = freeList.begin();
		auto candidate = freeList.end();
		auto candidatePrev = freeList.end();
		VkDeviceSize addressAlignmentMakeUp = 0U;
		while (iter != std::end(freeList))
		{

			VkDeviceSize endingAddress = iter->second;
			VkDeviceSize startingAddress = iter->first;
			VkDeviceSize makeup = (startingAddress & (alignment - 1));

			if (makeup)
			{
				startingAddress += makeup; //make up for any alignment considerations
			}

			VkDeviceSize holesize = endingAddress - startingAddress;
			
			if (holesize >= size)
			{
				VkDeviceSize diff = holesize - size;

				if (diff < maxDiff)
				{
					maxDiff = diff;
					candidate = iter;
					candidatePrev = prev;
					addressAlignmentMakeUp = makeup;
					if (!maxDiff) break; //perfect match
				}
			}

			prev = iter++;
		}

		if (candidate == std::end(freeList))
		{
			return std::make_pair(UINT64_MAX, UINT64_MAX);
		}

		auto ret = *candidate;
		freeList.erase_after(candidatePrev);
		if (addressAlignmentMakeUp)
		{
			InsertSorted(freeList, ret.first, ret.first + addressAlignmentMakeUp);
			ret.first += addressAlignmentMakeUp;
		}
		return ret;
	}

	VkDeviceSize GetMemory(VkDeviceSize size, VkDeviceSize alignment)
	{
		auto iter = GetBestFit(size, alignment);
		
		if (iter.first == UINT64_MAX && iter.second == UINT64_MAX) // too large, no lower bound
		{
			throw std::runtime_error("too large of allocation!");
		}

		VkDeviceSize originaladdr = iter.first;
		VkDeviceSize endaddr = originaladdr + size;
		VkDeviceSize originalSize = iter.second - originaladdr;

		InsertSorted(occupiedList, originaladdr, endaddr); //add new pair to occupied
	
		if (originalSize != size) // if it is not a perfect size, have new hole
		{
			InsertSorted(freeList, endaddr, iter.second);
		}
		
		return originaladdr;
	}

	void FreeMemory(VkDeviceSize addr)
	{
		auto prev = occupiedList.before_begin();
		
		auto iter = occupiedList.begin();

		while (iter != std::end(occupiedList) && iter->first != addr)
		{
			prev = iter++;
		}

		if (iter == std::end(occupiedList))
		{
			throw std::runtime_error("Free memory never allocated");
		}

		VkDeviceSize beg = iter->first, end = iter->second;
		occupiedList.erase_after(prev);
		InsertSortedMerged(freeList, beg, end);
	}
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

			for (auto& hb : hostBuffers)
			{
				vkDestroyBuffer(device, hb.first, nullptr);
				vkFreeMemory(device, hb.second, nullptr);
			}

			for (auto& sc : swapChains)
			{
				sc.DestroySwapChain();
			}

			for (auto& smp : samplers)
			{
				vkDestroySampler(device, smp, nullptr);
			}

			for (auto& iv : imageViews)
			{
				vkDestroyImageView(device, iv, nullptr);
			}

			for (auto& i : images)
			{
				vkDestroyImage(device, std::get<VkImage>(i), nullptr);
			}

			for (auto& m : deviceMemories)
			{
				vkFreeMemory(device, m, nullptr);
			}

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

	VKDevice& operator=(VKDevice&& _dev) noexcept {
		this->device = _dev.device;
		_dev.device = VK_NULL_HANDLE;
		this->gpu = _dev.gpu;
		this->commandPools = std::move(_dev.commandPools);
		this->queueManagers = std::move(_dev.queueManagers);
		this->descriptorPools = std::move(_dev.descriptorPools);
		this->swapChains = std::move(_dev.swapChains);
		this->deviceMemories = std::move(_dev.deviceMemories);
		this->allocators = std::move(_dev.allocators);
		this->deviceBuffers = std::move(_dev.deviceBuffers);
		this->images = std::move(_dev.images);
		this->imageViews = std::move(_dev.imageViews);
		return *this;
	};

	VKDevice(const VKDevice& _dev) = default;

	VKDevice(VKDevice&& _dev) noexcept
	{
		this->device = _dev.device;
		_dev.device = VK_NULL_HANDLE;
		this->gpu = _dev.gpu;
		this->commandPools = std::move(_dev.commandPools);
		this->queueManagers = std::move(_dev.queueManagers);
		this->descriptorPools = std::move(_dev.descriptorPools);
		this->swapChains = std::move(_dev.swapChains);
		this->deviceMemories = std::move(_dev.deviceMemories);
		this->allocators = std::move(_dev.allocators);
		this->deviceBuffers = std::move(_dev.deviceBuffers);
		this->images = std::move(_dev.images);
		this->imageViews = std::move(_dev.imageViews);
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

	std::pair<uint32_t, VkDeviceSize> FindImageMemoryIndexForPool(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps)
	{
		VkImage image;
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = layers;

		imageInfo.format = type;//VK::API::ConvertSMBToVkFormat(type);
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = flags;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
		imageInfo.flags = 0;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);
		uint32_t memoryTypeIndex = VK::Utils::findMemoryType(gpu, memRequirements.memoryTypeBits, memProps);

		vkDestroyImage(device, image, nullptr);

		return std::make_pair(memoryTypeIndex, memRequirements.alignment);

	}

	uint32_t CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex)
	{
		VkDeviceMemory deviceMemory;

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = poolSize;
		allocInfo.memoryTypeIndex = memoryTypeIndex;

		if (vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		deviceMemories.push_back(deviceMemory);
		allocators.emplace_back(poolSize);
		return static_cast<uint32_t>(deviceMemories.size()-1);
	}

	uint32_t CreateSwapChain(VkSurfaceKHR surface)
	{
		uint32_t index = static_cast<uint32_t>(swapChains.size());
		auto swapchain = swapChains.emplace_back(this, surface);
		return index;
	}


	uint32_t CreateImage(uint32_t width, 
		uint32_t height, uint32_t mipLevels, 
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps, uint32_t memIndex)
	{
		VkImage image;
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = layers;

		imageInfo.format = type;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = flags;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
		imageInfo.flags = 0;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		auto iter = deviceMemories[memIndex];

		auto &alloc = allocators[memIndex];

		VkDeviceSize addr = alloc.GetMemory(memRequirements.size, memRequirements.alignment);

		vkBindImageMemory(device, image, iter, addr);

		images.push_back(std::tuple(image, addr, memIndex));
		
		return static_cast<uint32_t>(images.size() - 1);
	}

	uint32_t CreateImageView(
		uint32_t imageIndex, uint32_t mipLevels, 
		VkFormat type, VkImageAspectFlags aspectMask)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = std::get<VkImage>(images[imageIndex]);
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = type;
		viewInfo.subresourceRange.aspectMask = aspectMask;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		VkImageView imageView = VK_NULL_HANDLE;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		imageViews.push_back(imageView);

		return static_cast<uint32_t>(imageViews.size() - 1);
	}

	uint32_t CreateImageView(
		VkImage image, uint32_t mipLevels,
		VkFormat type, VkImageAspectFlags aspectMask)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = type;
		viewInfo.subresourceRange.aspectMask = aspectMask;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		VkImageView imageView = VK_NULL_HANDLE;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		imageViews.push_back(imageView);

		return static_cast<uint32_t>(imageViews.size() - 1);
	}

	uint32_t CreateHostBuffer(VkDeviceSize allocSize, bool coherent, bool createAllocator, VkBufferUsageFlags usage)
	{
		hostBuffers.emplace_back(VK_NULL_HANDLE, VK_NULL_HANDLE);
		auto& ref = hostBuffers.back();
		std::tie(ref.first, ref.second) = VK::Utils::createBuffer(device, gpu, allocSize,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), VK_SHARING_MODE_EXCLUSIVE, usage);

		uint32_t ret = static_cast<uint32_t>(hostBuffers.size() - 1);

		if (createAllocator)
		{
			hostAllocators.emplace_back(ret, allocSize);
		}

		return ret;
	}

	uint32_t CreateSampledImage(
		std::vector<std::vector<char>>& imageData, 
		std::vector<uint32_t>& imageSizes,
		uint32_t width, uint32_t height, 
		uint32_t mipLevels, VkFormat type,
		uint32_t queueIndex, uint32_t queueFamilyIndex,
		uint32_t poolIndex, uint32_t memIndex,
		uint32_t hostIndex)
	{
		VkQueue queue;
		vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

		VkDeviceSize imagesSize = static_cast<VkDeviceSize>(std::accumulate(imageSizes.begin(), imageSizes.end(), 0));

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		std::tie(stagingBuffer, stagingMemory) = hostBuffers[hostIndex];

		char* data;
		auto& sizes = imageSizes;
		auto& pixels = imageData;
		vkMapMemory(device, stagingMemory, 0, imagesSize, 0, reinterpret_cast<void**>(&data));
		for (auto i = 0U; i < mipLevels; i++) {
			std::memcpy(data, pixels[i].data(), sizes[i]);
			data += sizes[i];
		}
		vkUnmapMemory(device, stagingMemory);

		VkFormat format = type;

		uint32_t imageIndex = CreateImage(
			width, height, mipLevels, type, 1, 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT, 
			1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memIndex);

		VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, commandPools[poolIndex]);

		VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

		VkDeviceSize offset = 0U;

		for (auto i = 0U; i < mipLevels; i++) {

			VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, std::get<VkImage>(images[imageIndex]), width >> i, height >> i, i, offset, { 0, 0, 0 });

			offset += static_cast<VkDeviceSize>(sizes[i]);
		}

		VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

		VK::Utils::EndOneTimeCommands(device, queue, commandPools[poolIndex], cb);

		return imageIndex;
	}

	void TransitionImageLayout(uint32_t poolIndex, uint32_t queueIdx, uint32_t imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers)
	{
		VkQueue queue;
		vkGetDeviceQueue(device, queueIdx, 0, &queue);
		VK::Utils::TransitionImageLayout(
			device, 
			commandPools[poolIndex], queue,
			std::get<VkImage>(images[imageIndex]), format,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
			1, 1
		);
	}

	uint32_t CreateSampler(uint32_t mipLevels)
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(gpu, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = static_cast<float>(mipLevels);

		uint32_t ret = static_cast<uint32_t>(samplers.size());

		samplers.emplace_back(VK_NULL_HANDLE);

		if (vkCreateSampler(device, &samplerInfo, nullptr, &samplers.back()) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}

		return ret;
	}

	void DestorySampler(uint32_t samplerIndex)
	{
		vkDestroySampler(device, samplers[samplerIndex], nullptr);
		samplers[samplerIndex] = VK_NULL_HANDLE;
	}

	void DestroyImageView(uint32_t imageViewIndex)
	{
		vkDestroyImageView(device, imageViews[imageViewIndex], nullptr);
		imageViews[imageViewIndex] = VK_NULL_HANDLE;
	}

	void DestroyImage(uint32_t imageIndex)
	{
		vkDestroyImage(device, std::get<VkImage>(images[imageIndex]), nullptr);
		auto& alloc = allocators[std::get<uint32_t>(images[imageIndex])];
		alloc.FreeMemory(std::get<VkDeviceSize>(images[imageIndex])); //image, address, and memory type index
		images[imageIndex] = std::tuple(VK_NULL_HANDLE, 0, 0);
	}

	void WriteToHostBuffer(uint32_t hostIndex, void* data, uint32_t size, uint32_t offset)
	{
		auto deviceMem = hostBuffers[hostIndex].second;
		void* datalocal;
		vkMapMemory(device, deviceMem, offset, size, 0, reinterpret_cast<void**>(&datalocal));
		std::memcpy(datalocal , data, size);
		vkUnmapMemory(device, deviceMem);
	}

	uint32_t GetOffsetIntoHostBuffer(uint32_t hostIndex, uint32_t size, uint32_t alignment)
	{
		auto alloc = std::find_if(hostAllocators.begin(), hostAllocators.end(), [&hostIndex](auto& pair)
			{
				return hostIndex == pair.first;
			}
		);

		return static_cast<uint32_t>(alloc->second.GetMemory(size, alignment));
	}


	VkImageView GetImageView(uint32_t index)
	{
		return imageViews[index];
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
	std::vector<VkDeviceMemory> deviceMemories; //memoryIndex, deviceMemory pool
	//std::unordered_map<uint32_t, VkDeviceSize> deviceMemoriesAllocators; //memoryIndex, current stack allocator
	std::vector<VKAllocator> allocators;
	std::vector<std::pair<VkBuffer, VkDeviceMemory>> deviceBuffers;
	std::vector<std::pair<VkBuffer, VkDeviceMemory>> hostBuffers;
	std::vector<char*> hostBufferMappings;
	std::vector<std::tuple<VkImage, VkDeviceSize, uint32_t>> images; 
	std::vector<VkImageView> imageViews;
	std::vector<VkSampler> samplers; 
	std::vector<std::pair<uint32_t, VKAllocator>> hostAllocators;
};

