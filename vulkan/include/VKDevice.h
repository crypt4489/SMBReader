#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <forward_list>
#include <map>
#include <numeric>

#include <unordered_map>
#include <set>

#include "vulkan/vulkan.h"
#include "IndexTypes.h"
#include "ThreadManager.h"
#include "VKPipelineCache.h"
#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKShaderCache.h"
#include "VKRenderPassBuilder.h"
#include "VKTypes.h"
#include "VKSwapChain.h"
#include "VKUtilities.h"


class VKDevice;

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

class VKCommandBuffer
{
public:
	VKCommandBuffer() :
		buffer(VK_NULL_HANDLE), fenceIdx(~0U), poolIndex(~0U), queueIndex(~0U), queueFamilyIndex(~0U) {};
	VKCommandBuffer(VkCommandBuffer _b, uint32_t i, uint32_t pi, uint32_t qi, uint32_t qfi)
		: buffer(_b), fenceIdx(i), poolIndex(pi), queueIndex(qi), queueFamilyIndex(qfi) {};
	VkCommandBuffer buffer;
	uint32_t fenceIdx = ~0U;
	uint32_t poolIndex;
	uint32_t queueIndex;
	uint32_t queueFamilyIndex;
};

class RecordingBufferObject
{
public:

	RecordingBufferObject(VKDevice& device, VKCommandBuffer& buffer);

	void BindPipeline(uint32_t renderTarget, std::string pipelinename);

	void BindDescriptorSets(std::string descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	void BindVertexBuffer(uint32_t bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets);

	void BindingDrawCmd(uint32_t first, uint32_t drawSize);

	void BindingIndirectDrawCmd(uint32_t indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BeginRenderPassCommand(uint32_t renderTargetIndex, uint32_t imageIndex,
		VkRect2D rect,
		VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 1.0f} },
		VkClearDepthStencilValue depthStencil = { 1.0f, 0 });

	void EndRenderPassCommand();


	void SetViewportCommand(float xs, float ys, float width, float height, float minDepth, float maxDepth);

	void SetScissorCommand(int xo, int yo, uint32_t extentx, uint32_t extenty);

	void BeginRecordingCommand();

	void EndRecordingCommand();

	VKCommandBuffer& cbBufferHandler;
	VKDevice& vkDeviceHandle;
	VkPipelineLayout currLayout;
	VkPipeline currPipeline;
};

class RenderTarget
{
public:
	RenderTarget() = default;
	RenderTarget(uint32_t renderPass, uint32_t imageCount)
	{
		renderPassIndex = renderPass;
		framebufferIndices.resize(imageCount);
		imageViews.resize(imageCount);
	}
	uint32_t renderPassIndex;
	std::vector<uint32_t> framebufferIndices;
	std::vector<ImageIndex> imageViews;
};

enum VKQueueCapabilities : uint32_t
{
	GRAPHICS = 1,
	TRANSFER = 2,
	COMPUTE = 4,
	PRESENT = 8
};

class QueueManager
{
public:

	QueueManager(std::vector<uint32_t> _cqs, int32_t _mqc, uint32_t _qfi, uint32_t _queueCapabilities, bool present, VKDevice& _d);

	uint32_t GetQueue();

	void ReturnQueue(int32_t queueNum);

	uint32_t ConvertQueueProps(uint32_t flags, bool present);

	std::bitset<16> bitmap;
	const int32_t maxQueueCount;
	const uint32_t queueFamilyIndex;
	const uint32_t queueCapabilities;
	std::vector<uint32_t> poolIndices;
	VKDevice& device;
	Semaphore sema;
	std::mutex bitwiseMutex;
};



class VKDevice
{
public:
	VKDevice(VkPhysicalDevice _gpu);
	~VKDevice();

	VKDevice& operator=(const VKDevice& _dev) = delete;

	VKDevice& operator=(VKDevice&& _dev) noexcept;

	VKDevice(const VKDevice& _dev) = delete;

	VKDevice(VKDevice&& _dev) noexcept;

	VkPhysicalDevice GetGPU() const;

	void QueueFamilyDetails(std::vector<VkQueueFamilyProperties>& famProps);

	int32_t GetPresentQueue(QueueIndex& queueIdx,
		QueueIndex& maxQueueCount,
		std::vector<VkQueueFamilyProperties>& famProps,
		VkSurfaceKHR& renderSurface);

	int32_t GetQueueByMask(QueueIndex& queueIdx,
		QueueIndex& maxQueueCount,
		std::vector<VkQueueFamilyProperties>& famProps,
		uint32_t queueMask);

	void CreateLogicalDevice(
		std::vector<const char*>& instanceLayers,
		std::vector<const char*>& deviceExtensions,
		uint32_t queueFlags,
		VkPhysicalDeviceFeatures& features,
		VkSurfaceKHR renderSurface
	);

	std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> GetQueueHandle(uint32_t capabilites);

	void CreateQueueManager(uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport);

	uint32_t CreateCommandPool(QueueIndex& queueIndex);

	VkDescriptorPool CreateDesciptorPool(uint32_t& poolIndex, DescriptorPoolBuilder& builder, uint32_t maxSets);

	std::pair<uint32_t, VkDeviceSize> FindImageMemoryIndexForPool(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps);

	uint32_t CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex);

	uint32_t CreateSwapChain(VkSurfaceKHR surface);

	uint32_t CreateSwapChain(VkSurfaceKHR surface, uint32_t attachmentCount);


	ImageIndex CreateImage(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps, uint32_t memIndex);

	ImageIndex CreateImageView(
		ImageIndex& imageIndex, uint32_t mipLevels,
		VkFormat type, VkImageAspectFlags aspectMask);

	ImageIndex CreateImageView(
		VkImage image, uint32_t mipLevels,
		VkFormat type, VkImageAspectFlags aspectMask);

	BufferIndex CreateHostBuffer(VkDeviceSize allocSize, bool coherent, bool createAllocator, VkBufferUsageFlags usage);

	ImageIndex CreateSampledImage(
		std::vector<std::vector<char>>& imageData,
		std::vector<uint32_t>& imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, VkFormat type,
		uint32_t memIndex,
		uint32_t hostIndex);

	void TransitionImageLayout(ImageIndex& imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers);

	ImageIndex CreateSampler(uint32_t mipLevels);

	void DestorySampler(ImageIndex& samplerIndex);

	void DestroyImageView(ImageIndex& imageViewIndex);

	void DestroyImage(ImageIndex& imageIndex);

	void WriteToHostBuffer(BufferIndex& hostIndex, void* data, size_t size, size_t offset);

	OffsetIndex GetOffsetIntoHostBuffer(BufferIndex& hostIndex, size_t size, uint32_t alignment);

	uint32_t CreateRenderPasses(VKRenderPassBuilder& builder);

	void CreateRenderGraph(uint32_t renderPass, std::vector<uint32_t>& dynamicOffsets, std::string perGraphDescriptor);

	std::vector<uint32_t> CreateSemaphores(uint32_t count);

	std::vector<uint32_t> CreateFences(uint32_t count, VkFenceCreateFlags flags);

	uint32_t BeginFrameForSwapchain(uint32_t swapChainIndex, uint32_t requestedImage);

	uint32_t SubmitCommandBuffer(
		std::vector<uint32_t>& wait,
		std::vector<VkPipelineStageFlags>& waitStages,
		std::vector<uint32_t>& signal,
		uint32_t cbIndex);

	uint32_t SubmitCommandsForSwapChain(uint32_t swapChainIdx, uint32_t frameIndex,
		uint32_t cbIndex);

	uint32_t PresentSwapChain(uint32_t swapChainIdx, uint32_t frameIdx, uint32_t imageIdx);

	std::vector<uint32_t> CreateReusableCommandBuffers(
		uint32_t numberOfCommandBuffers, VkCommandBufferLevel level, bool createFences, uint32_t capabilites);
	
	uint32_t GetCommandBufferIndex(uint64_t timeout);

	uint32_t RequestAndResetCommandBuffer(uint64_t timeout, uint32_t bufferIndex, bool reset);

	uint32_t CreateFrameBuffer(std::vector<uint32_t>& attachmentIndices, uint32_t renderPassIndex, VkExtent2D& extent);

	VkCommandBuffer GetCommandBufferHandle(uint32_t index);

	uint32_t GetFamiliesOfCapableQueues(std::vector<uint32_t> &queueFamilies, uint32_t capabilities);

	VkImageView GetImageView(uint32_t index);

	VkCommandPool GetCommandPool(uint32_t poolIndex);

	VkDescriptorPool GetDescriptorPool(uint32_t poolIndex);

	VKSwapChain& GetSwapChain(uint32_t index);

	VkRenderPass GetRenderPass(uint32_t index); 

	VkFence GetFence(uint32_t index);

	VkSemaphore GetSemaphore(uint32_t index);
	
	VkFramebuffer GetFrameBuffer(uint32_t index);

	VKShaderCache& GetShaders();

	VKDescriptorSetCache& GetDescriptorSets();

	VKDescriptorLayoutCache& GetDescriptorLayouts();

	void CreatePipelineCache(uint32_t renderPassIndex);

	VKPipelineCache& GetPipelineCache(uint32_t renderPassIndex);

	DescriptorSetBuilder CreateDescriptorSetBuilder(uint32_t poolIndex, std::string layoutname, uint32_t numberofsets);

	DescriptorSetLayoutBuilder CreateDescriptorSetLayoutBuilder();

	RecordingBufferObject GetRecordingBufferObject(uint32_t commandBufferIndex);

	uint32_t CreateRenderTarget(uint32_t renderPassIndex, uint32_t framebufferCount);

	RenderTarget& GetRenderTarget(uint32_t renderTargetIndex);

	void AllocateCommandPools(uint32_t size);

	void WaitOnDevice();

	void GetExclusiveLock();

	void UnlockExclusiveLock();

	void GetSharedLock();

	VkDevice device;
	VkPhysicalDevice gpu;
	std::vector<QueueManager*> queueManagers;
	std::vector<VkCommandPool> commandPools;
	std::vector<VkDescriptorPool> descriptorPools;
	std::vector<VKSwapChain> swapChains;
	std::vector<VkDeviceMemory> deviceMemories; //memoryIndex, deviceMemory pool
	std::vector<VKAllocator> allocators;
	std::vector<std::pair<VkBuffer, VkDeviceMemory>> deviceBuffers;
	std::vector<std::pair<VkBuffer, VkDeviceMemory>> hostBuffers;
	std::vector<std::tuple<VkImage, VkDeviceSize, uint32_t>> images; 
	std::vector<VkImageView> imageViews;
	std::vector<VkSampler> samplers; 
	std::vector<std::pair<uint32_t, VKAllocator>> hostAllocators;
	std::vector<VkRenderPass> renderPasses;
	std::vector<VkSemaphore> semaphores;
	std::vector<VkFence> fences;
	std::vector<VKCommandBuffer> commandBuffers;
	std::vector<VkFramebuffer> frameBuffers;
	std::vector<RenderTarget> renderTargets;
	VKShaderCache shaders;
	std::unordered_map<uint32_t, VKPipelineCache> renderPassPipelineCache;
	VKDescriptorLayoutCache descriptorLayoutCache;
	VKDescriptorSetCache descriptorSetCache;
	SharedExclusiveFlag deviceLock;

	std::unordered_map<uint32_t, uint32_t> graphMapping;
	std::vector<VKRenderGraph*> graphs;
};

