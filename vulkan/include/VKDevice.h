#pragma once

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <forward_list>
#include <numeric>
#include <set>
#include <mutex>
#include <shared_mutex>

#include "vulkan/vulkan.h"

#include "IndexTypes.h"
#include "VKTypes.h"


class VKDevice;

struct DescriptorPoolBuilder
{
	DescriptorPoolBuilder(void* data, size_t _pslSize) 
		: 
		poolSizesSize(_pslSize),
		counter(0),
		poolSizes(reinterpret_cast<VkDescriptorPoolSize*>(data))
	{
	}
	void AddUniformPoolSize(uint32_t size)
	{
		poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, size };
	}

	void AddStoragePoolSize(uint32_t size)
	{
		poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, size };
	}

	void AddImageSampler(uint32_t size)
	{
		poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, size };
	}

	VkDescriptorPoolSize* poolSizes;
	size_t poolSizesSize;
	size_t counter; 
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
			startingAddress += makeup; //make up for any alignment considerations
			

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
	VKCommandBuffer(VkCommandBuffer _b, EntryHandle i, uint32_t pi, uint32_t qi, uint32_t qfi)
		: buffer(_b), fenceIdx(i), poolIndex(pi), queueIndex(qi), queueFamilyIndex(qfi) {};
	VKCommandBuffer(const VKCommandBuffer& other) = default;
	VKCommandBuffer(VKCommandBuffer&& other) = default;

	VkCommandBuffer buffer;
	EntryHandle fenceIdx;
	uint32_t poolIndex;
	uint32_t queueIndex;
	uint32_t queueFamilyIndex;
};

class RecordingBufferObject
{
public:

	RecordingBufferObject(VKDevice& device, VKCommandBuffer& buffer);

	void BindGraphicsPipeline(EntryHandle pipelinename);

	void BindComputePipeline(EntryHandle pipelineId);

	void BindPipelineInternal(EntryHandle id, VkPipelineBindPoint bindPoint);

	void BindDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	void BindVertexBuffer(EntryHandle bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets);

	void BindingDrawCmd(uint32_t first, uint32_t drawSize);

	void BindingDrawIndexedCmd(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	void BindingIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BeginRenderPassCommand(EntryHandle renderTargetIndex, uint32_t imageIndex,
		VkRect2D rect,
		VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 1.0f} },
		VkClearDepthStencilValue depthStencil = { 1.0f, 0 });

	void BindIndexBuffer(EntryHandle bufferIndex, uint32_t indexOffset);

	void DispatchCommand(uint32_t x, uint32_t y, uint32_t z);

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
	
	RenderTarget(EntryHandle renderPass, uint32_t imageCount, void *data)
	{
		renderPassIndex = renderPass;
		count = imageCount;

		EntryHandle* head = reinterpret_cast<EntryHandle*>(data);

		framebufferIndices = head;
		imageViews = std::next(head, imageCount);
	}

	EntryHandle renderPassIndex;
	uint32_t count;
	EntryHandle* framebufferIndices;
	EntryHandle* imageViews;
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

	QueueManager(uint32_t* _cqs, uint32_t _cqss,
		int32_t _mqc, uint32_t _qfi,
		uint32_t _queueCapabilities, bool present,
		VKDevice& _d, void* data);

	uint32_t GetQueue();

	void ReturnQueue(size_t queueNum);

	uint32_t ConvertQueueProps(uint32_t flags, bool present);

	std::bitset<16> bitmap;
	const int32_t maxQueueCount;
	const uint32_t queueFamilyIndex;
	const uint32_t queueCapabilities;
	EntryHandle* poolIndices;
	VKDevice& device;
	std::mutex queueSema;
	std::condition_variable queueCV;
	int queueCountCV;
	std::mutex bitwiseMutex;
};



class VKDevice
{
public:
	
	VKDevice(VkPhysicalDevice _gpu, VKInstance* _inst);

	//CREATORS

	EntryHandle CompileShader(char* data, VkShaderStageFlags flags);

	EntryHandle CreateCommandPool(QueueIndex& queueIndex);

	EntryHandle CreateDesciptorPool(DescriptorPoolBuilder& builder, uint32_t maxSets);

	DescriptorPoolBuilder CreateDescriptorPoolBuilder(size_t poolSize);

	DescriptorSetBuilder* CreateDescriptorSetBuilder(EntryHandle poolIndex, EntryHandle layout, uint32_t numberofsets);

	DescriptorSetLayoutBuilder* CreateDescriptorSetLayoutBuilder(uint32_t bindingCount);

	EntryHandle CreateDescriptorSet(VkDescriptorSet* set, uint32_t numberOfSets);

	EntryHandle CreateDescriptorSetLayout(DescriptorSetLayoutBuilder* builder);

	EntryHandle* CreateFences(uint32_t count, VkFenceCreateFlags flags);

	EntryHandle CreateFrameBuffer(EntryHandle* attachmentIndices, uint32_t attachmentsCount, EntryHandle renderPassIndex, VkExtent2D& extent);

	EntryHandle CreateHostBuffer(VkDeviceSize allocSize, bool coherent, VkBufferUsageFlags usage);

	EntryHandle CreateImage(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps, EntryHandle memIndex);

	EntryHandle CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex);

	EntryHandle CreateImageView(
		EntryHandle imageIndex, uint32_t mipLevels,
		VkFormat type, VkImageAspectFlags aspectMask);

	EntryHandle CreateImageView(
		VkImage image, uint32_t mipLevels,
		VkFormat type, VkImageAspectFlags aspectMask);

	void CreateLogicalDevice(
		const char** instanceLayers,
		uint32_t layerCount,
		const char** deviceExtensions,
		uint32_t deviceExtCount,
		uint32_t queueFlags,
		VkPhysicalDeviceFeatures& features,
		VkSurfaceKHR renderSurface,
		size_t perDeviceDataSize,
		size_t perEntriesSize,
		size_t perCacheSize,
		size_t driverPerSize,
		size_t driverPerCache
	);

	VKGraphicsPipelineBuilder* CreateGraphicsPipelineBuilder(EntryHandle renderPassIndex, uint32_t colorCount, uint32_t descLayoutCount, uint32_t dynamicStateCount);
	
	VKComputePipelineBuilder* CreateComputePipelineBuilder(size_t numDesc);

	EntryHandle CreatePipelineCacheObject(PipelineCacheObject* obj);

	EntryHandle CreatePipelineObject(VKGraphicsPipelineObjectCreateInfo* info);

	void CreateQueueManager(QueueManager* manager, uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport);

	void CreateRenderGraph(EntryHandle renderPass);

	EntryHandle CreateRenderPasses(VKRenderPassBuilder& builder);

	VKRenderPassBuilder CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs);

	EntryHandle CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount);

	EntryHandle* CreateReusableCommandBuffers(
		uint32_t numberOfCommandBuffers,
		VkCommandBufferLevel level, bool createFences, 
		uint32_t capabilites
	);

	EntryHandle CreateSampledImage(
		char* imageData,
		uint32_t* imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, VkFormat type,
		EntryHandle memIndex,
		EntryHandle hostIndex, VkImageAspectFlags flags);

	EntryHandle CreateSampler(uint32_t mipLevels);

	EntryHandle* CreateSemaphores(uint32_t count);

	EntryHandle CreateShader(char* data, size_t dataSize, VkShaderStageFlags flags);

	EntryHandle CreateSwapChain(uint32_t attachmentCount, uint32_t requestedImageCount, uint32_t maxSemaphorePerStage, uint32_t stages);

	//GETTERS

	VKCommandBuffer* GetCommandBuffer(EntryHandle index);

	VkCommandBuffer GetCommandBufferHandle(EntryHandle index);

	VkCommandPool GetCommandPool(EntryHandle poolIndex);

	VkDescriptorPool GetDescriptorPool(EntryHandle poolIndex);

	VkDescriptorSet GetDescriptorSet(EntryHandle handle, uint32_t index);

	VkDescriptorSetLayout GetDescriptorSetLayout(EntryHandle index);

	uint32_t GetFamiliesOfCapableQueues(uint32_t** queueFamilies, uint32_t* size, uint32_t capabilities);

	VkFence GetFence(EntryHandle index);

	VkFramebuffer GetFrameBuffer(EntryHandle index);
	
	VkBuffer GetHostBuffer(EntryHandle index);

	VkImage GetImageByIndex(EntryHandle index);

	VkImage GetImageByTexture(EntryHandle index);

	VkImageView GetImageViewByIndex(EntryHandle index);

	VkImageView GetImageViewByTexture(EntryHandle index);

	PipelineCacheObject* GetPipelineCacheObject(EntryHandle index);

	VKPipelineObject* GetPipelineObject(EntryHandle index);

	int32_t GetPresentQueue(QueueIndex& queueIdx,
		QueueIndex& maxQueueCount,
		VkQueueFamilyProperties* famProps,
		uint32_t famPropsCount,
		VkSurfaceKHR& renderSurface);

	int32_t GetQueueByMask(QueueIndex& queueIdx,
		QueueIndex& maxQueueCount,
		VkQueueFamilyProperties* famProps,
		uint32_t famPropsCount,
		uint32_t queueMask);

	std::tuple<uint32_t, uint32_t, uint32_t, EntryHandle> GetQueueHandle(uint32_t capabilites);

	RecordingBufferObject GetRecordingBufferObject(EntryHandle commandBufferIndex);

	VKRenderGraph* GetRenderGraph(EntryHandle renderPassIndex);

	VkRenderPass GetRenderPass(EntryHandle index);

	RenderTarget* GetRenderTarget(EntryHandle renderTargetIndex);

	VkSampler GetSamplerByIndex(EntryHandle index);

	VkSampler GetSamplerByTexture(EntryHandle index);

	VkSemaphore GetSemaphore(EntryHandle index);

	std::pair<VkShaderModule, VkShaderStageFlagBits> GetShader(EntryHandle shaderHandle);

	VKSwapChain* GetSwapChain(EntryHandle index);


	VKTexture* GetTexture(EntryHandle handle);

	//Destructors

	void DestroyBuffer(EntryHandle handle);

	void DestroyCommandBuffer(EntryHandle handle);

	void DestroyDescriptorPool(EntryHandle handle);

	void DestroyDescriptorLayout(EntryHandle handle);

	void DestroyDevice();

	void DestroyImage(EntryHandle imageIndex);

	void DestroyImagePool(EntryHandle handle);

	void DestroyImageView(EntryHandle imageViewIndex);

	void DestroyPipelineCacheObject(EntryHandle handle);

	void DestroyRenderPass(EntryHandle handle);

	void DestroyRenderTarget(EntryHandle handle);

	void DestorySampler(EntryHandle samplerIndex);

	void DestroySemaphore(EntryHandle handle);

	void DestroyShader(EntryHandle shaderHandle);

	void DestroyTexture(EntryHandle textureHandle);

	//Allocation

	void* GetVkTypeFromEntry(EntryHandle index);

	EntryHandle AddVkTypeToEntry(void* handle);

	void* AllocFromPerDeviceData(size_t size);

	void* AllocFromDeviceCache(size_t size);


	//ACTIONS/HELPERS


	uint32_t BeginFrameForSwapchain(EntryHandle swapChainIndex, uint32_t requestedImage);

	VkShaderStageFlagBits ConvertShaderFlags(const std::string& filename);

	std::pair<uint32_t, VkDeviceSize> FindImageMemoryIndexForPool(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps);

	size_t GetOffsetIntoHostBuffer(EntryHandle hostIndex, size_t size, uint32_t alignment);

	uint32_t PresentSwapChain(EntryHandle swapChainIdx, uint32_t frameIdx, EntryHandle commandBufferIndex);

	VkQueueFamilyProperties* QueueFamilyDetails(uint32_t *size);

	EntryHandle RequestWithPossibleBufferResetAndFenceReset(uint64_t timeout, EntryHandle bufferIndex, bool reset, bool fenceReset);

	void ReturnQueueToManager(size_t queueManagerIndex, size_t queueIndex);

	uint32_t SubmitCommandBuffer(
		EntryHandle* wait,
		VkPipelineStageFlags* waitStages,
		EntryHandle* signal,
		uint32_t waitCount,
		uint32_t signalCount,
		EntryHandle cbIndex);

	uint32_t SubmitCommandsForSwapChain(EntryHandle swapChainIdx, uint32_t frameIndex,
		EntryHandle cbIndex);

	void TransitionImageLayout(EntryHandle imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers);

	void UpdateRenderGraph(EntryHandle renderPass, uint32_t* dynamicOffsets, uint32_t dos, EntryHandle perGraphDescriptor);

	int32_t WaitOnCommandBufferAndPossibleResetFence(uint64_t timeout, EntryHandle bufferIndex, bool resetfence);
	
	void WaitOnDevice();

	void WriteToHostBuffer(EntryHandle hostIndex, void* data, size_t size, size_t offset);

	VkDevice device;
	VkPhysicalDevice gpu;
	VKInstance* parentInstance;
	EntryHandle queueManagers;
	size_t queueManagersSize;

	mutable std::shared_mutex deviceLock;

	uintptr_t* entries;
	std::atomic<size_t> indexForEntries = 0;
	size_t numberOfEntries = 0;

	void* perDeviceData;
	std::atomic<size_t> perDeviceOffset;
	size_t perDeviceSize;

	void* deviceCache;
	std::atomic<size_t> deviceCacheWrite;
	size_t deviceCacheSize;

	VKDriverAllocator *deviceAllocator;
};

