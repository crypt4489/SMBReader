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




struct VKMemoryAllocator
{

	VkDeviceSize capacity;
	std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>> freeList; // [staringAddr, endingAddr)
	std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>> occupiedList; //[staringAddr, endingAddr)

	VKMemoryAllocator(const VkDeviceSize _c);

	VKMemoryAllocator(const VKMemoryAllocator&) = delete;

	VKMemoryAllocator& operator=(const VKMemoryAllocator&) = delete;

	VKMemoryAllocator(VKMemoryAllocator&&) = default;

	VKMemoryAllocator& operator=(VKMemoryAllocator&&) = default;

	void InsertSorted(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list,
		VkDeviceSize first,
		VkDeviceSize last);

	void InsertSortedMerged(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list,
		VkDeviceSize first,
		VkDeviceSize last);

	std::pair<VkDeviceSize, VkDeviceSize> GetBestFit(VkDeviceSize size, VkDeviceSize alignment);

	VkDeviceSize GetMemory(VkDeviceSize size, VkDeviceSize alignment);

	void FreeMemory(VkDeviceSize addr);

	void Reset();
};

struct VKCommandBuffer
{
	VKCommandBuffer() :
		buffer(VK_NULL_HANDLE), fenceIdx(~0U), poolIndex(~0U), queueIndex(~0U), queueFamilyIndex(~0U) {};
	VKCommandBuffer(VkCommandBuffer _b, EntryHandle i, EntryHandle pi, uint32_t qi, uint32_t qfi)
		: buffer(_b), fenceIdx(i), poolIndex(pi), queueIndex(qi), queueFamilyIndex(qfi) {};
	VKCommandBuffer(const VKCommandBuffer& other) = default;
	VKCommandBuffer(VKCommandBuffer&& other) = default;

	VkCommandBuffer buffer;
	EntryHandle fenceIdx;
	EntryHandle poolIndex;
	uint32_t queueIndex;
	uint32_t queueFamilyIndex;
};

struct RBOPipelineBarrierArgs
{
	VkPipelineStageFlags srcStageMask;
	VkPipelineStageFlags dstStageMask;
	VkDependencyFlags dependencyFlags;
	uint32_t memoryBarrierCount;
	VkMemoryBarrier* pMemoryBarriers;
	uint32_t bufferMemoryBarrierCount;
	VkBufferMemoryBarrier* pBufferMemoryBarriers;
	uint32_t imageMemoryBarrierCount;
	VkImageMemoryBarrier* pImageMemoryBarriers;
};

struct RecordingBufferObject
{


	RecordingBufferObject(VKDevice* device, VKCommandBuffer buffer);

	void BindGraphicsPipeline(EntryHandle pipelinename);

	void BindComputePipeline(EntryHandle pipelineId);

	void BindPipelineInternal(EntryHandle id, VkPipelineBindPoint bindPoint);

	void BindDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	void BindComputeDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	void BindVertexBuffer(EntryHandle bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets);

	void BindingDrawCmd(uint32_t first, uint32_t drawSize, uint32_t firstInstance, uint32_t instanceCount);

	void BindingDrawIndexedCmd(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	void BindingIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BindingIndexedIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BeginRenderPassCommand(EntryHandle renderTargetIndex, uint32_t imageIndex,
		VkSubpassContents contents,
		VkRect2D rect,
		VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 1.0f} },
		VkClearDepthStencilValue depthStencil = { 1.0f, 0 });

	void BindIndexBuffer(EntryHandle bufferIndex, uint32_t indexOffset, VkIndexType indexType);

	void BindPipelineBarrierCommand(RBOPipelineBarrierArgs* args);

	void DispatchCommand(uint32_t x, uint32_t y, uint32_t z);

	void EndRenderPassCommand();

	void PushConstantsCommand(uint32_t offset, uint32_t size, VkShaderStageFlags flag, void *data);

	void SetViewportCommand(float xs, float ys, float width, float height, float minDepth, float maxDepth);

	void SetScissorCommand(int xo, int yo, uint32_t extentx, uint32_t extenty);

	void BeginRecordingCommand(VkCommandBufferInheritanceInfo* info, VkCommandBufferUsageFlags flags);

	void EndRecordingCommand();

	void CommandBufferReset();

	void ResetCommandPoolForBuffer();

	void ExecuteSecondaryCommands(EntryHandle* handles, uint32_t count);

	void FillBuffer(EntryHandle bufferHandle, size_t size, size_t offset, uint32_t val);

	void UpdateBuffer(EntryHandle bufferHandle, size_t size, size_t offset, void* val);

	void BindingDrawIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount);
	void BindingDrawIndexedIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount);

	VKCommandBuffer cbBufferHandler;
	VKDevice* vkDeviceHandle;
	VkPipelineLayout currLayout;
	VkPipeline currPipeline;
};

struct RenderTarget
{

	
	RenderTarget() = default;
	
	RenderTarget(EntryHandle renderPass, uint32_t imageCount, void* data);

	EntryHandle renderPassIndex;
	uint32_t count;
	EntryHandle* framebufferIndices;
	EntryHandle* imageViews;
};

enum VKQueueCapabilities
{
	GRAPHICSQUEUE = 1,
	TRANSFERQUEUE = 2,
	COMPUTEQUEUE = 4,
	PRESENTQUEUE = 8
};

struct QueueManager
{


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

struct ShaderHandle
{
	VkShaderModule sMod;
	VkShaderStageFlags flags;
};

struct DeviceOwnedAllocator
{
	uintptr_t memHead;
	std::atomic<size_t> writeHead;
	size_t size;

	DeviceOwnedAllocator();

	void* Alloc(size_t inSize);

	void* AllocWrapAround(size_t inSize);
};


struct DescriptorPoolBuilder
{
	DescriptorPoolBuilder(DeviceOwnedAllocator* alloc, size_t _numPoolSizes, VkDescriptorPoolCreateFlags _flags);

	void AddUniformPoolSize(uint32_t size);

	void AddStoragePoolSize(uint32_t size);

	void AddImageSampler(uint32_t size);
	VkDescriptorPoolSize* poolSizes;
	VkDescriptorPoolCreateFlags flags;
	size_t numPoolSizes;
	size_t counter;
};


enum HandleType
{
	VulkBuffer = 0,
	VulkImageHandle = 1,
	VulkImageView = 2,
	VulkImageSampler = 3,
	VulkTextureHandle = 4,
	VulkBufferView = 5,
	VulkImageBufferView = 6,
	VulkCommandPool = 7,
	VulkComputeGraph = 8,
	VulkDescriptorPool = 9,
	VulkDescriptorSet = 10,
	VulkDescriptorLayout = 11,
	VulkFence = 12,
	VulkFrameBuffer = 13,
	VulkImageMemoryPool = 14,
	VulkPipelineCacheObject = 15,
	VulkComputePipeline = 16,
	VulkGraphicsPipeline = 17,
	VulkMemoryBarrier = 18,
	VulkBufferMemoryBarrier = 19,
	VulkImageMemoryBarrier = 20,
	VulkRenderTargetData = 21,
	VulkRenderPassHandle = 22,
	VulkRenderTarget = 23,
	VulkVKCommandBuffer = 24,
	VulkSemaphores = 25,
	VulkShaderHandle = 26,
	VulkSwapChain = 27,
	VulkQueueManager = 28,
	VulkGraphicsOTQ = 29,
	VulkComputeOTQ = 30,
	VulkIndirectPipeline = 31,
	VulkMaxEnum
};

struct HandlePoolObject
{
	HandleType type;
	uintptr_t memoryLocation;
};


struct VKDevice
{

	
	VKDevice(VkPhysicalDevice _gpu, VKInstance* _inst);

	//CREATORS

	EntryHandle CompileShader(char* data, VkShaderStageFlags flags);

	EntryHandle CreateBufferView(EntryHandle bufferHandle, VkFormat format, size_t rangeSize, size_t offset, uint32_t numberOfAllocs);

	EntryHandle CreateBufferViewFromImagePool(EntryHandle imagePoolIndex, VkFormat format, size_t rangeSize, size_t offset);

	EntryHandle CreateCommandPool(QueueIndex queueIndex);

	EntryHandle CreateComputeGraph(uint32_t dynamicCount, uint32_t maxPipelineCount, uint32_t maxFramesInFlight);

	EntryHandle CreateComputeOneTimeQueue(uint32_t maxObjectCount);

	EntryHandle CreateDesciptorPool(DescriptorPoolBuilder* builder, uint32_t maxSets);

	DescriptorPoolBuilder CreateDescriptorPoolBuilder(size_t poolSize, VkDescriptorPoolCreateFlags flags);

	DescriptorSetBuilder* CreateDescriptorSetBuilder(EntryHandle poolIndex, EntryHandle descriptorLayout, uint32_t numberofsets);

	DescriptorSetBuilder* UpdateDescriptorSet(EntryHandle descriptorHandle);

	DescriptorSetLayoutBuilder* CreateDescriptorSetLayoutBuilder(uint32_t bindingCount);

	EntryHandle CreateDescriptorSet(VkDescriptorSet* set, uint32_t numberOfSets);

	EntryHandle CreateDescriptorSetLayout(DescriptorSetLayoutBuilder* builder);

	EntryHandle* CreateFences(uint32_t count, VkFenceCreateFlags flags);

	EntryHandle CreateFrameBuffer(EntryHandle* attachmentIndices, uint32_t attachmentsCount, EntryHandle renderPassIndex, VkExtent2D extent);

	EntryHandle CreateHostBuffer(VkDeviceSize allocSize, bool coherent, VkBufferUsageFlags usage);

	EntryHandle CreateDeviceBuffer(VkDeviceSize allocSize, VkBufferUsageFlags usage);

	EntryHandle CreateImage(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps, VkImageLayout layout, VkImageTiling tiling, VkImageCreateFlags cflags, EntryHandle memIndex);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, VkFormat type,
		EntryHandle memIndex,
		VkImageAspectFlags flags, VkImageLayout layout, bool createSampler);

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
		VkPhysicalDeviceFeatures2* features,
		VkSurfaceKHR renderSurface,
		size_t perDeviceDataSize,
		size_t perEntriesSize,
		size_t perCacheSize,
		size_t driverPerSize,
		size_t driverPerCache
	);

	EntryHandle CreateGraphicsOneTimeQueue(uint32_t maxObjectCount);

	VKGraphicsPipelineBuilder* CreateGraphicsPipelineBuilder(EntryHandle renderPassIndex, uint32_t colorCount, uint32_t descLayoutCount, uint32_t dynamicStateCount, uint32_t pushConstantRangeCount);
	
	VKComputePipelineBuilder* CreateComputePipelineBuilder(size_t numberOfDescriptors, uint32_t pushConstantRangeCount);

	EntryHandle CreatePipelineCacheObject(PipelineCacheObject* obj);

	EntryHandle CreateComputePipelineObject(VKComputePipelineObjectCreateInfo* info);

	EntryHandle CreateGraphicsPipelineObject(VKGraphicsPipelineObjectCreateInfo* info);



	EntryHandle CreateMemoryBarrier(VkAccessFlags src, VkAccessFlags dst);

	EntryHandle CreateBufferMemoryBarrier(VkAccessFlags src, VkAccessFlags dst, uint32_t srcQFI, uint32_t dstQFI, EntryHandle bufferIndex, size_t offset, size_t size);

	EntryHandle CreateImageMemoryBarrier(VkAccessFlags src, VkAccessFlags dst, uint32_t srcQFI, uint32_t dstQFI, VkImageLayout oldLayout, VkImageLayout newLayout, EntryHandle imageIndex, VkImageSubresourceRange subresourceRange);

	void CreateQueueManager(QueueManager* manager, uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport);

	

	EntryHandle CreateRenderTargetData(EntryHandle renderTarget, uint32_t descriptorCount);

	EntryHandle CreateRenderPasses(VKRenderPassBuilder& builder);

	VKRenderPassBuilder CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs);

	EntryHandle CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount);

	EntryHandle* CreateReusableCommandBuffers(
		uint32_t numberOfCommandBuffers,
		bool createFences, 
		uint32_t capabilites,
		VkCommandBufferLevel level
	);

	EntryHandle CreateSampledImage(
		char* imageData,
		uint32_t* imageSizes,
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, VkFormat type,
		EntryHandle memIndex,
		EntryHandle hostIndex, VkImageAspectFlags flags);


	EntryHandle CreateSampledImageHandle(
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, VkFormat type,
		EntryHandle memIndex,
		VkImageAspectFlags flags
	);


	EntryHandle CreateSampler(uint32_t mipLevels);

	EntryHandle* CreateSemaphores(uint32_t count);

	EntryHandle CreateShader(char* data, size_t dataSize, VkShaderStageFlags flags);

	EntryHandle CreateSwapChain(uint32_t attachmentCount, uint32_t requestedImageCount, uint32_t maxFramesInFlight, uint32_t renderTargetCount);

	

	//GETTERS

	VkBufferView GetBufferView(EntryHandle handle, uint32_t index);

	VKCommandBuffer* GetCommandBuffer(EntryHandle handle);

	VkCommandBuffer GetCommandBufferHandle(EntryHandle handle);

	VkCommandPool GetCommandPool(EntryHandle handle);

	VKComputeGraph* GetComputeGraph(EntryHandle handle);

	VKComputeOneTimeQueue* GetComputeOTQ(EntryHandle handle);

	VKGraphicsOneTimeQueue* GetGraphicsOTQ(EntryHandle handle);

	VkDescriptorPool GetDescriptorPool(EntryHandle handle);

	VkDescriptorSet GetDescriptorSet(EntryHandle handle, uint32_t index);

	VkDescriptorSet* GetDescriptorSets(EntryHandle handle);

	VkDescriptorSetLayout GetDescriptorSetLayout(EntryHandle handle);

	uint32_t GetFamiliesOfCapableQueues(uint32_t** queueFamilies, uint32_t* size, uint32_t capabilities);

	VkFence GetFence(EntryHandle handle);

	VkFramebuffer GetFrameBuffer(EntryHandle handle);
	
	VkBuffer GetBufferHandle(EntryHandle handle);

	VkImage GetImageByHandle(EntryHandle handle);

	VkImage GetImageByTexture(EntryHandle handle);

	VkImageView GetImageViewByHandle(EntryHandle handle);

	VkImageView GetImageViewByTexture(EntryHandle handle, int imageViewIndex);

	VkMemoryBarrier* GetMemoryBarrier(EntryHandle handle);

	VkBufferMemoryBarrier* GetBufferMemoryBarrier(EntryHandle handle);

	VkImageMemoryBarrier* GetImageMemoryBarrier(EntryHandle handle);

	PipelineCacheObject* GetPipelineCacheObject(EntryHandle handle);

	VKPipelineObject* GetPipelineObject(EntryHandle handle);

	VKComputePipelineObject* GetComputePipelineObject(EntryHandle handle);

	int32_t GetPresentQueue(QueueIndex* queueIdx,
		QueueIndex* maxQueueCount,
		VkQueueFamilyProperties* famProps,
		uint32_t famPropsCount,
		VkSurfaceKHR renderSurface);

	int32_t GetQueueByMask(QueueIndex* queueIdx,
		QueueIndex* maxQueueCount,
		VkQueueFamilyProperties* famProps,
		uint32_t famPropsCount,
		uint32_t queueMask);

	struct QueueDetails
	{
		uint32_t managerIndex;
		uint32_t queueIndex;
		uint32_t queueFamilyIndex;
		EntryHandle poolIndex;
	};

	QueueDetails GetQueueHandle(uint32_t capabilites);

	RecordingBufferObject GetRecordingBufferObject(EntryHandle handle);

	VKRenderGraph* GetRenderGraph(EntryHandle handle);

	VkRenderPass GetRenderPass(EntryHandle handle);

	RenderTarget* GetRenderTarget(EntryHandle handle);

	RenderTarget* GetRenderTargetByGraph(EntryHandle handle);

	VkSampler GetSamplerByHandle(EntryHandle handle);

	VkSampler GetSamplerByTexture(EntryHandle handle, int samplerIndex);

	VkSemaphore GetSemaphore(EntryHandle handle);

	ShaderHandle* GetShader(EntryHandle handle);

	VKSwapChain* GetSwapChain(EntryHandle handle);

	VKTexture* GetTexture(EntryHandle handle);

	//Destructors

	void DestroyBuffer(EntryHandle handle);

	void DestroyBufferView(EntryHandle handle);

	

	void DestroyCommandBuffer(EntryHandle handle);

	void DestroyCommandPool(EntryHandle handle);

	void DestroyDescriptorPool(EntryHandle handle);

	void DestroyDescriptorLayout(EntryHandle handle);

	void DestroyDevice();

	void DestroyFence(EntryHandle handle);

	void DestroyFrameBuffer(EntryHandle handle);

	void DestroyImage(EntryHandle handle);

	void DestroyImagePool(EntryHandle handle);

	void DestroyImageView(EntryHandle handle);

	void DestroyPipelineCacheObject(EntryHandle handle);

	void DestroyRenderPass(EntryHandle handle);

	void DestroyRenderTarget(EntryHandle handle);

	void DestroySampler(EntryHandle handle);

	void DestroySemaphore(EntryHandle handle);

	void DestroyShader(EntryHandle handle);

	void DestroySwapChain(EntryHandle handle);

	void DestroyTexture(EntryHandle handle);

	void DestroyTexelBufferView(EntryHandle handle);

	

	//Allocation

	HandlePoolObject GetVkTypeFromEntry(EntryHandle index);

	EntryHandle AddVkTypeToEntry(void* handle, HandleType type);

	void* AllocFromPerDeviceData(size_t size);

	void* AllocFromDeviceCache(size_t size);


	//ACTIONS/HELPERS


	uint32_t BeginFrameForSwapchain(EntryHandle swapChainIndex, uint32_t requestedImage);

	void CommandBufferResetFence(EntryHandle bufferIndex);

	VkShaderStageFlagBits ConvertShaderFlags(const std::string& filename);

	std::pair<uint32_t, VkDeviceSize> FindImageMemoryIndexForPool(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps);

	size_t GetMemoryFromBuffer(EntryHandle hostIndex, size_t size, uint32_t alignment);

	uint32_t PresentSwapChain(EntryHandle swapChainIdx, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle commandBufferIndex);

	VkQueueFamilyProperties* QueueFamilyDetails(uint32_t *size);

	EntryHandle RequestWithPossibleBufferResetAndFenceReset(uint64_t timeout, EntryHandle bufferIndex, bool commnadBufferReset, bool fenceReset);

	void ResetRenderTarget(EntryHandle handle);

	void ReturnQueueToManager(size_t queueManagerIndex, size_t queueIndex);

	uint32_t SubmitCommandBuffer(
		EntryHandle* wait,
		VkPipelineStageFlags* waitStages,
		EntryHandle* signal,
		uint32_t waitCount,
		uint32_t signalCount,
		EntryHandle cbIndex);

	uint32_t SubmitCommandsForSwapChain(EntryHandle swapChainIdx, uint32_t frameIndex, uint32_t imageIndex, EntryHandle cbIndex);

	void TransitionImageLayout(EntryHandle imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers);

	void UpdateRenderGraph(EntryHandle renderPass, uint32_t* dynamicOffsets, uint32_t dos, EntryHandle* perGraphDescriptor, uint32_t descriptorCount, uint32_t* dynamicPerSet);

	int32_t CommandBufferWaitOn(uint64_t timeout, EntryHandle bufferIndex);
	
	void WaitOnDevice();

	void WriteToHostBuffer(EntryHandle hostIndex, void* data, size_t size, size_t offset, int copies, size_t stride);

	void WriteToHostBufferBatch(EntryHandle hostIndex, void** dataPoints, size_t* sizes, size_t* offsets, size_t range, size_t minOffset, size_t numCopies);

	void WriteToDeviceBuffer(EntryHandle deviceIndex, EntryHandle stagingBufferIndex, void* data, size_t size, size_t offset, int copies, size_t stride);

	void ReadHostBuffer(void* dest, EntryHandle hostIndex, size_t size, size_t offset);

	void UploadImageData(EntryHandle textureIndex,
		char* imageData, size_t totalImageDataSize,
		uint32_t* indivdualImageSizes, EntryHandle stagingBufferIndex,
		int width, int height,
		int mipLevels, VkFormat format
	);

	void UploadImageData(EntryHandle textureIndex,
		char* imageData, size_t totalImageDataSize,
		uint32_t* indivdualImageSizes, EntryHandle stagingBufferIndex,
		int width, int height,
		int mipLevels, VkFormat format, RecordingBufferObject* rbo
	);


	void WriteToDeviceBufferBatch(EntryHandle deviceIndex, EntryHandle stagingBufferIndex, void** data, size_t* sizes, size_t* offsets, size_t cumulativesize, int entries, RecordingBufferObject* rbo);

	void ResetBufferAllocator(EntryHandle bufferIndex);

	//mutable std::shared_mutex deviceLock;

	VkDevice device;
	VkPhysicalDevice gpu;
	VKInstance* parentInstance;

	EntryHandle queueManagers;
	size_t queueManagersSize;

	uint32_t deviceMask = 0;

	DeviceOwnedAllocator deviceDataAlloc;
	DeviceOwnedAllocator deviceCacheAlloc;

	HandlePoolObject* entries;
	std::atomic<size_t> indexForEntries = 0;
	size_t numberOfEntries = 0;

	VKAllocationCB *deviceDriverAllocator;
};

