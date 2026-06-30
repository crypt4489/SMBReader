#pragma once

#include <bitset>
#include <condition_variable>
#include <mutex>


#include "vulkan/vulkan.h"

#include "IndexTypes.h"
#include "VKTypes.h"

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

enum RecordingBufferObjectErrorCodes
{
	RBO_FAILED_TO_BEGIN_RECORD = 1,
	RBO_FAILED_TO_END_RECORD = 2,
	RBO_FAILED_TO_RESET_BUFFER = 3,
	RBO_FAILED_TO_RESET_POOL = 4
};

struct RecordingBufferObject
{
	RecordingBufferObject(VKDevice* device, VKCommandBuffer buffer);

	int BindGraphicsPipeline(EntryHandle pipelinename);

	int BindComputePipeline(EntryHandle pipelineId);

	int BindPipelineInternal(EntryHandle id, VkPipelineBindPoint bindPoint);

	int BindGraphicsDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	int BindComputeDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
		uint32_t dynamicOffsetCount, uint32_t* offsets);

	void BindVertexBuffer(EntryHandle bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets);

	void BindingDrawCmd(uint32_t first, uint32_t drawSize, uint32_t firstInstance, uint32_t instanceCount);

	void BindingDrawIndexedCmd(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	void BindingIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BindingIndexedIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset);

	void BeginRenderPassCommand(EntryHandle renderTargetIndex, uint32_t imageIndex,
		VkSubpassContents contents,
		VkRect2D rect,
		VkClearValue* clearVals, uint32_t clearValCount);

	void BindIndexBuffer(EntryHandle bufferIndex, uint32_t indexOffset, VkIndexType indexType);

	void BindPipelineBarrierCommand(RBOPipelineBarrierArgs* args);

	void DispatchCommand(uint32_t x, uint32_t y, uint32_t z);

	void EndRenderPassCommand();

	void PushConstantsCommand(uint32_t offset, uint32_t size, VkShaderStageFlags flag, void *data);

	void SetViewportCommand(float xs, float ys, float width, float height, float minDepth, float maxDepth);

	void SetScissorCommand(int xo, int yo, uint32_t extentx, uint32_t extenty);

	int BeginRecordingCommand(VkCommandBufferInheritanceInfo* info, VkCommandBufferUsageFlags flags);

	int EndRecordingCommand();

	int CommandBufferReset();

	int ResetCommandPoolForBuffer();

	int ExecuteSecondaryCommands(EntryHandle* handles, uint32_t count);

	void FillBuffer(EntryHandle bufferHandle, size_t size, size_t offset, uint32_t val);

	void UpdateBuffer(EntryHandle bufferHandle, size_t size, size_t offset, void* val);

	void BindingDrawIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount);
	void BindingDrawIndexedIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount);

	void BeginQuery(EntryHandle queryPoolHandle, uint32_t queryIndex);
	
	void EndQuery(EntryHandle queryPoolHandle, uint32_t queryIndex);

	void WriteTimestamp(EntryHandle queryPoolHandle, uint32_t queryIndex, VkPipelineStageFlagBits pipelineStage);

	void ResetQueries(EntryHandle poolIndex, uint32_t firstQuery, uint32_t queryCount);

	VKCommandBuffer cbBufferHandler;
	VKDevice* vkDeviceHandle;
	VkPipelineLayout currLayout;
};

struct RenderTarget
{
	RenderTarget() = default;
	
	RenderTarget(EntryHandle renderPass, uint32_t imageCount, uint32_t _wSize, uint32_t _hSize, uint32_t _wOff, uint32_t _hOff, void* data);

	EntryHandle renderPassIndex;
	EntryHandle* framebufferIndices;
	uint32_t count;
	uint32_t width;
	uint32_t height;
	uint32_t wOffset;
	uint32_t hOffset;
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
		VKDevice* _d, void* data);

	uint32_t GetQueue();

	void ReturnQueue(size_t queueNum);

	uint32_t ConvertQueueProps(uint32_t flags, bool present);

	void DestroyManager();

	std::bitset<16> bitmap;
	const int32_t maxQueueCount;
	const uint32_t queueFamilyIndex;
	const uint32_t queueCapabilities;
	EntryHandle* poolIndices;
	VKDevice* device;
	std::mutex queueSema;
	std::condition_variable queueCV;
	int queueCountCV;
};

struct ShaderHandle
{
	VkShaderModule sMod;
	VkShaderStageFlags flags;
};

struct DeviceOwnedAllocator
{
	uintptr_t memHead;
	size_t writeHead;
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

	void AddImageSamplerCombined(uint32_t size);

	void AddStorageImage(uint32_t size);

	void AddSampledImage(uint32_t size);

	void AddSampler(uint32_t size);

	VkDescriptorPoolSize* poolSizes;
	VkDescriptorPoolCreateFlags flags;
	size_t numPoolSizes;
	size_t counter;
};

struct ImageMemoryPool
{
	VkDeviceMemory memory;
};

struct BufferAlloc
{
	VkBuffer buffer;
	VkDeviceMemory memory;
};

struct BufferView
{
	uint32_t count;
	VkBufferView* views;
};

struct TexelBufferView
{
	EntryHandle bufferHandle;
	EntryHandle viewHandle;
};

struct ImageAllocation
{
	VkImage imageHandle;
	size_t deviceMemoryAddress;
	EntryHandle memIndex;
};

enum HandleType : uint64_t
{
	VulkBuffer,
	VulkImageHandle,
	VulkImageView,
	VulkImageSampler,
	VulkTextureHandle,
	VulkBufferView,
	VulkImageBufferView,
	VulkCommandPool,
	VulkComputeGraph,
	VulkDescriptorPool,
	VulkDescriptorSet,
	VulkDescriptorLayout,
	VulkFence,
	VulkFrameBuffer,
	VulkImageMemoryPool,
	VulkPipelineCacheObject,
	VulkRenderPassHandle,
	VulkRenderTarget,
	VulkVKCommandBuffer,
	VulkSemaphores,
	VulkShaderHandle,
	VulkSwapChain,
	VulkQueueManager,
	VulkQueryPool,
	VulkTimelineSemaphore,
	VulkMaxEnum
};

enum DeviceErrorCodeMajor
{
	DEVICE_HANDLE_RETRIEVE_OVERFLOW = 1,
	DEVICE_HANDLE_ENTRIES_EXHAUSTION = 2,
	DEVICE_STORAGE_EXHAUSTED = 3,
	DEVICE_CACHE_ALLOC_TOO_LARGE = 4,
	DEVICE_VK_TYPE_CREATION_FAILED = 5,
	DEVICE_VK_TYPE_HANDLE_INPUT_INVALID = 6,
	DEVICE_VK_TYPE_SWC_PRESENT_FAILED = 7,
	DEVICE_VK_TYPE_COMMAND_BUFFER_SUBMIT_FAILED = 8,
	DEVICE_VK_TYPE_SHADER_CONVERT_FLAGS_FAILED = 9,
	DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE = 10,
	DEVICE_VK_TYPE_INDEX_OUT_OF_BOUNDS = 11,
	DEVICE_VK_TYPE_TIMED_RESULT_FAILED = 12,
	DEVICE_VK_TYPE_ACQUIRE_IMAGE_FAILED = 13,
	DEVICE_VK_TYPE_ALLOCATION_FAILED = 14,
};

enum DeviceErrorCodeMinor
{
	DEVICE_VK_TYPE_BUFFER_VIEW_FAILED = 1,
	DEVICE_VK_TYPE_BUFFER_FAILED = 2,
	DEVICE_VK_TYPE_COMMAND_POOL_FAILED = 3,
	DEVICE_VK_TYPE_DESCRIPTOR_POOL_FAILED = 4,
	DEVICE_VK_TYPE_FENCE_FAILED = 5,
	DEVICE_VK_TYPE_FRAMEBUFFER_FAILED = 6,
	DEVICE_VK_TYPE_MEMORY_FAILED = 7,
	DEVICE_VK_TYPE_BIND_MEMORY_FAILED = 8,
	DEVICE_VK_TYPE_SHADER_MODULE_FAILED = 9,
	DEVICE_VK_TYPE_IMAGE_HANDLE_FAILED = 10,
	DEVICE_VK_TYPE_MEMORY_ALLOCATION_FAILED = 11,
	DEVICE_VK_TYPE_IMAGE_VIEW_FAILED = 12,
	DEVICE_VK_TYPE_QUERY_POOL_FAILED = 13,
	DEVICE_VK_TYPE_RENDER_PASS_FAILED = 14,
	DEVICE_VK_TYPE_COMMAND_BUFFER_FAILED = 15,
	DEVICE_VK_TYPE_SAMPLER_FAILED = 16,
	DEVICE_VK_TYPE_SEMAPHORE_FAILED = 17,
	DEVICE_VK_TYPE_LOGICAL_DEVICE_FAILED = 18,
	DEVICE_VK_TYPE_BUFFER_ALLOC_FAILED = 19,
	DEVICE_VK_TYPE_BUFFER_VIEW_CONTAINER_FAILED = 20,
	DEVICE_VK_TYPE_DESCRIPTOR_SET_FAILED = 21,
	DEVICE_VK_TYPE_DESCRIPTOR_LAYOUT_FAILED = 22,
	DEVICE_VK_TYPE_QUEUE_MANAGER_FAILED = 23,
	DEVICE_VK_TYPE_IMAGE_MEMORY_POOL_FAILED = 24,
	DEVICE_VK_TYPE_IMAGE_PIPELINE_CACHE_FAILED = 25,
	DEVICE_VK_TYPE_IMAGE_SWAPCHAIN_FAILED = 26,
	DEVICE_VK_TYPE_TEXEL_BUFFER_VIEW_FAILED = 27,
	DEVICE_VK_TYPE_TEXTURE_FAILED = 28,
	DEVICE_VK_TYPE_SWAPCHAIN_FAILED = 29,
	DEVICE_VK_TYPE_GRAPHICS_PIPELINE_FAILED = 30,
	DEVICE_VK_TYPE_COMPUTE_PIPELINE_FAILED = 31,
	DEVICE_VK_TYPE_PIPELINE_LAYOUT_FAILED = 32,
};

struct HandlePoolObject
{
	HandleType type;
	uintptr_t memoryLocation;
};

struct DeviceErrorCodeStruct
{
	int internalErrorCode;
	VkResult vkResult;
};

struct VKDevice
{
	VKDevice(VkPhysicalDevice _gpu, VKInstance* _inst);

	//CREATORS

	EntryHandle CompileShader(char* data, VkShaderStageFlags flags);

	EntryHandle CreateBufferView(EntryHandle bufferHandle, VkFormat format, size_t rangeSize, size_t offset, uint32_t numberOfAllocs);

	EntryHandle CreateBufferViewFromImagePool(EntryHandle imagePoolIndex, VkFormat format, size_t rangeSize, size_t offset);

	EntryHandle CreateCommandPool(QueueIndex queueIndex);

	EntryHandle CreateDesciptorPool(DescriptorPoolBuilder* builder, uint32_t maxSets);

	DescriptorPoolBuilder CreateDescriptorPoolBuilder(size_t poolSize, VkDescriptorPoolCreateFlags flags);

	DescriptorSetBuilder* CreateDescriptorSetBuilder(EntryHandle poolIndex, EntryHandle descriptorLayout, uint32_t numberofsets, uint32_t varDescriptorCount);

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
		VkImageUsageFlags flags, uint32_t sampleCount, size_t memoryAddr,
		VkMemoryPropertyFlags memProps, VkImageLayout layout, VkImageTiling tiling, VkImageCreateFlags cflags, VkImageType imageType, EntryHandle memIndex);
/*
	EntryHandle CreateImageHandle(
		uint32_t width, uint32_t height, uint32_t layers,
		uint32_t mipLevels, size_t memAddr, VkFormat imageFormat,
		EntryHandle memIndex,
		VkImageAspectFlags flags,
		VkImageType imageType, VkImageUsageFlags usageFlags, VkImageLayout imageLayout, VkImageCreateFlags createFlags
	);

	*/

	EntryHandle CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex);

	EntryHandle CreateImageView(
		EntryHandle imageIndex, uint32_t firstMip, uint32_t firstLayer, uint32_t mipLevels, uint32_t layersCount,
		VkFormat type, VkImageAspectFlags aspectMask, VkImageViewType imageViewType);

	EntryHandle CreateImageView(
		VkImage image, uint32_t firstMip, uint32_t firstLayer, uint32_t mipLevels, uint32_t layersCount,
		VkFormat type, VkImageAspectFlags aspectMask, VkImageViewType imageViewType);

	int CreateLogicalDevice(
		const char** deviceExtensions,
		uint32_t deviceExtCount,
		VkPhysicalDeviceFeatures2* features,
		QueueIndex* queueIndices,
		uint32_t* queueMaxCounts,
		float* queuePrios,
		uint32_t queueCount,
		size_t perDeviceDataSize,
		size_t perEntriesSize,
		size_t perCacheSize,
		size_t driverPerSize,
		size_t driverPerCache,
		void* driverPoolHead,
		void* devicePoolHead
	);

	VKGraphicsPipelineBuilder* CreateGraphicsPipelineBuilder(EntryHandle renderPassIndex, uint32_t colorCount, uint32_t descLayoutCount, uint32_t dynamicStateCount, uint32_t pushConstantRangeCount);
	
	VKComputePipelineBuilder* CreateComputePipelineBuilder(size_t numberOfDescriptors, uint32_t pushConstantRangeCount);

	EntryHandle CreatePipelineCacheObject(PipelineCacheObject* obj);

	EntryHandle CreateQueueManager(uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport);

	EntryHandle CreateQueryPool(VkQueryType queryType, uint32_t numberOfQueries);

	EntryHandle CreateRenderPasses(VKRenderPassBuilder& builder);

	VKRenderPassBuilder CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs);

	EntryHandle CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount, uint32_t width, uint32_t height, uint32_t wOffset, uint32_t hOffset);

	EntryHandle* CreateReusableCommandBuffers(
		EntryHandle managerIndex,
		uint32_t numberOfCommandBuffers,
		bool createFences, 
		VkCommandBufferLevel level
	);

	EntryHandle CreateSampler(uint32_t mipLevels);

	EntryHandle* CreateSemaphores(uint32_t count);

	EntryHandle* CreateTimelineSemaphores(uint32_t count, uint64_t initialValue);

	EntryHandle CreateShader(char* data, size_t dataSize, VkShaderStageFlags flags);

	EntryHandle CreateSwapChain(uint32_t requestedImageCount, uint32_t maxFramesInFlight, VkFormat requestedFormat, EntryHandle renderSurfaceIndex);

	
	//GETTERS

	BufferAlloc* GetBufferAlloc(EntryHandle handle);

	VkBuffer GetBufferHandle(EntryHandle handle);

	VkBufferView GetBufferView(EntryHandle handle, uint32_t index);

	BufferView* GetBufferViewContainer(EntryHandle handle);

	VKCommandBuffer* GetCommandBuffer(EntryHandle handle);

	VkCommandBuffer GetCommandBufferHandle(EntryHandle handle);

	VkCommandPool GetCommandPool(EntryHandle handle);

	VkDescriptorPool GetDescriptorPool(EntryHandle handle);

	VkDescriptorSet GetDescriptorSet(EntryHandle handle, uint32_t index);

	VkDescriptorSet* GetDescriptorSets(EntryHandle handle);

	VkDescriptorSetLayout GetDescriptorSetLayout(EntryHandle handle);

	VkFence GetFence(EntryHandle handle);

	VkFramebuffer GetFrameBuffer(EntryHandle handle);
	
	ImageAllocation* GetImageAllocation(EntryHandle handle);

	VkImage GetImageByHandle(EntryHandle handle);

//	VkImage GetImageByTexture(EntryHandle handle);

	ImageMemoryPool* GetImageMemoryPool(EntryHandle handle);

	VkImageView GetImageViewByHandle(EntryHandle handle);

//	VkImageView GetImageViewByTexture(EntryHandle handle, int imageViewIndex);

	PipelineCacheObject* GetPipelineCacheObject(EntryHandle handle);

	int GetPresentQueue(
		QueueIndex* queueIdx,
		uint32_t* maxQueueCount,
		VkSurfaceKHR renderSurface,
		VkQueueFamilyProperties* famProps);

	int GetQueueByMask(
		QueueIndex* queueIdx,
		uint32_t* maxQueueCount,
		uint32_t queueMask,
		VkQueueFamilyProperties* famProps);

	VkQueryPool GetQueryPool(EntryHandle poolHandle);

	struct QueueDetails
	{
		uint32_t queueIndex;
		uint32_t queueFamilyIndex;
		EntryHandle poolIndex;
	};

	QueueDetails GetQueueHandle(EntryHandle queueManagerIndex);
		
	QueueManager* GetQueueManager(EntryHandle queueManagerIndex);

	RecordingBufferObject GetRecordingBufferObject(EntryHandle handle);

	VkRenderPass GetRenderPass(EntryHandle handle);

	RenderTarget* GetRenderTarget(EntryHandle handle);

	VkSampler GetSamplerByHandle(EntryHandle handle);

//	VkSampler GetSamplerByTexture(EntryHandle handle, int samplerIndex);

	VkSemaphore GetSemaphore(EntryHandle handle);

	ShaderHandle* GetShader(EntryHandle handle);

	VKSwapChain* GetSwapChain(EntryHandle handle);

	VkSemaphore GetTimelineSemaphore(EntryHandle handle);

	TexelBufferView* GetTexelBufferView(EntryHandle handle);

//	VKTexture* GetTexture(EntryHandle handle);

	//Destructors

	void DestroyBuffer(EntryHandle handle);

	void DestroyBufferView(EntryHandle handle);

	void DestroyCommandBuffer(EntryHandle handle);

	void DestroyCommandPool(EntryHandle handle);

	void DestroyDescriptorPool(EntryHandle handle);

	void DestroyDescriptorLayout(EntryHandle handle);

	void DestroyDescriptorSet(EntryHandle handle);

	void DestroyDevice();

	void DestroyFence(EntryHandle handle);

	void DestroyFrameBuffer(EntryHandle handle);

	void DestroyImage(EntryHandle handle);

	void DestroyImagePool(EntryHandle handle);

	void DestroyImageView(EntryHandle handle);

	void DestroyPipelineCacheObject(EntryHandle handle);

	void DestroyRenderPass(EntryHandle handle);

	void DestroyRenderTarget(EntryHandle handle);

	void DestroyQueueManager(EntryHandle handle);

	void DestroyQueryPool(EntryHandle handle);

	void DestroySampler(EntryHandle handle);

	void DestroySemaphore(EntryHandle handle);

	void DestroyShader(EntryHandle handle);

	void DestroySwapChain(EntryHandle handle);

	void DestroyTimelineSemaphore(EntryHandle handle);

//	void DestroyTexture(EntryHandle handle);

	void DestroyTexelBufferView(EntryHandle handle);

	

	//Allocation

	HandlePoolObject GetVkTypeFromEntry(EntryHandle index);

	EntryHandle AddVkTypeToEntry(void* handle, HandleType type);

	void* AllocFromPerDeviceData(size_t size);

	void* AllocFromDeviceCache(size_t size);

	void ReturnHandleObject(EntryHandle index);

	void AddDeviceErrorCode(int internalErrorCode, VkResult vulkSpecificResult);

	char* PopErrorOffQueue(int* strLength);

	void FreeFromPerDeviceData(void* memoryAddress);


	//ACTIONS/HELPERS

	uint32_t BeginFrameForSwapchain(EntryHandle swapChainIndex, EntryHandle acquireSemaphoreHandle, uint32_t currentFrame);

	int CommandBufferResetFence(EntryHandle bufferIndex);

	VkShaderStageFlagBits ConvertShaderFlags(const char* filename, int nameLength);

	std::pair<uint32_t, VkDeviceSize> FindImageMemoryIndexForPool(uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps);

	void GetImageMemorySizeAndAlignment(
		uint32_t width,
		uint32_t height, uint32_t mipLevels,
		VkFormat type, uint32_t layers,
		VkImageUsageFlags flags, uint32_t sampleCount,
		VkMemoryPropertyFlags memProps, VkImageLayout layout, 
		VkImageTiling tiling, VkImageCreateFlags cflags, VkImageType imageType,
		size_t* actualImageSize, size_t* alignment
	);

	int PresentSwapChainCommandBufferInline(EntryHandle swapChainIdx, EntryHandle* presentWaitSemaphores, uint32_t presentWaitSemaphoreCount, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle commandBufferIndex);

	int PresentSwapChainSeparatePresentQueue(EntryHandle swapChainIdx, EntryHandle* presentWaitSemaphores, uint32_t presentWaitSemaphoreCount, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle presentQueue);

	void QueueFamilyDetails(VkQueueFamilyProperties* famProps, uint32_t *size);

	uint32_t QueueFamilyDetailsCount();

	int ResetRenderTarget(EntryHandle handle);

	int ReturnQueueToManager(EntryHandle queueManagerIndex, uint32_t queueIndex);

	int SubmitCommandBuffer(
		EntryHandle* wait,
		VkPipelineStageFlags* waitStages,
		EntryHandle* signal,
		uint32_t waitCount,
		uint32_t signalCount,
		EntryHandle cbIndex);

	int SubmitCommandBuffer(
		EntryHandle* waitBinary,
		VkPipelineStageFlags* waitStages,
		uint32_t waitBinaryCount,
		EntryHandle* waitTimeline,
		uint32_t waitTimelineCount,
		uint64_t* waitSignalCount,
		EntryHandle* signalBinary,
		uint32_t signalBinaryCount,
		EntryHandle* signalTimeline,
		uint32_t signalTimelineCount,
		uint64_t* timelineSignalCount,
		EntryHandle cbIndex
	);

	int TransitionImageLayout(EntryHandle imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers, EntryHandle queueManagerIndex);

	int TransitionImageLayout(RecordingBufferObject* rbo, EntryHandle imageIndex,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t mips, uint32_t layers);

	DescriptorSetBuilder* UpdateDescriptorSet(EntryHandle descriptorHandle);

	int CommandBufferWaitOn(uint64_t timeout, EntryHandle bufferIndex);
	
	void WaitOnDevice();

	int WriteToHostBuffer(EntryHandle hostIndex, void* data, size_t size, size_t offset, int copies, size_t stride);

	int WriteToHostBufferBatch(EntryHandle hostIndex, void** dataPoints, size_t* sizes, size_t* offsets, size_t range, size_t minOffset, size_t numCopies);

	int WriteToDeviceBuffer(
		EntryHandle deviceIndex,
		EntryHandle stagingBufferIndex,
		void* data,
		size_t size, size_t destOffset,
		int copies, size_t stride, size_t stagingOffset,
		EntryHandle queueManagerIndex
	);

	int ReadHostBuffer(void* dest, EntryHandle hostIndex, size_t size, size_t offset);

	int UploadImageData(EntryHandle imageIndex,
		char* imageData, size_t totalImageDataSize, EntryHandle stagingBufferIndex,
		int width, int height, int layers,
		int mipLevels, VkFormat format, VkImageLayout startingLayout, size_t stagingOffsetStart, EntryHandle queueManagerIndex
	);

	int UploadImageData(EntryHandle imageIndex,
		char* imageData, size_t totalImageDataSize,
		EntryHandle stagingBufferIndex,
		int width, int height, int layers,
		int mipLevels, VkFormat format, VkImageLayout startingLayout, size_t stagingOffsetStart, RecordingBufferObject* rbo
	);

	int WriteToDeviceBufferBatch(EntryHandle deviceIndex, EntryHandle stagingBufferIndex, void** data, size_t* sizes, size_t* destOffsets, size_t cumulativesize, size_t* stagingOffsets, int entries, RecordingBufferObject* rbo);
	
	int ResetQueryPool(EntryHandle poolHandle, uint32_t firstQuery, uint32_t queryCount);

	int ReadbackResultsFromQueries(EntryHandle poolIndex, uint32_t firstQuery, uint32_t queryCount, void* dataOut, size_t sizeOfDataOut, VkDeviceSize dataStride, VkQueryResultFlags flags);

	VkDevice device;
	VkPhysicalDevice gpu;
	VKInstance* parentInstance;

	DeviceOwnedAllocator deviceCacheAlloc;

	HandlePoolObject* entries;
	size_t indexForEntries = 0;
	size_t numberOfEntries = 0;

	size_t* handlesFreeList;
	size_t numberOfFreeListEntries = 0;

	VKAllocationCB *deviceDriverAllocator;

	static const uint32_t errorCodeWrapSize = 16;
	DeviceErrorCodeStruct errorCodes[errorCodeWrapSize];
	uint32_t currentErrorCodePos = 0;
	uint32_t readErrorCodePos = 0;
	uint32_t deviceMask = 0;
	int freeListTop = -1;

	tlsf_main_t* permanentDeviceAlloc;
};

