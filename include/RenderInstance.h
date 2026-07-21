#pragma once

#include <vulkan/vulkan.h>

#include "allocator/AppAllocator.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "math/VertexTypes.h"
#include "ShaderManagement.h"
#include "RenderInstanceManagement.h"
#include "WindowManager.h"

namespace API 
{
	VkFormat ConvertComponentFormatTypeToVulkanFormat(ComponentFormatType type);

	VkCompareOp ConvertRasterizerTestToVulkanCompareOp(RasterizerTest testApp);

	VkFormat ConvertImageFormatToVulkanFormat(ImageFormat format);

	VkPrimitiveTopology ConvertTopology(PrimitiveType type);

	VkAccessFlags ConvertBarrierActionToVulkanAccessFlags(BarrierAction action);

	VkPipelineStageFlags ConvertBarrierStageToVulkanPipelineStage(BarrierStage sourceStage);

	VkShaderStageFlags ConvertShaderStageToVulkanShaderStage(ShaderStageType type);

	VkImageLayout ConvertImageLayoutToVulkanImageLayout(ImageLayout layout);

	void ConvertVertexInputToVKVertexAttrDescription(VertexInputDescription* inputDescs, int numInputDescs, int vertexBufferLocation, VkVertexInputAttributeDescription* attrs);

	VkCullModeFlags ConvertCullMode(CullMode mode);

	VkFrontFace ConvertTriangleWinding(TriangleWinding winding);

	void ConvertGPUFeatureRequestToVkPhysicalDeviceProperties(const GPUFeatureRequest* request,
		VkPhysicalDeviceFeatures2* features2,
		VkPhysicalDeviceVulkan12Features* features12);

	VkImageType ConvertImageTypeToVulkanImageType(ImageType imageType);

	VkImageAspectFlags ConvertImageViewAspectMaskToVulkanImageAspectFlags(ImageViewAspectMask aspectMask);

	VkImageViewType ConvertImageTypeToVulkanImageViewType(ImageType imageType);

	VkImageUsageFlags ConvertImageUsageFlagsToVulkanImageUsageFlags(ImageUsageFlags flags);

	VkMemoryPropertyFlags ConvertMemoryTypeToVkMemoryPropertyFlags(MemoryType memType);

	VkBlendFactor ConvertBlendFactorToVulkanBlendFactor(BlendFactor factor);

	VkBlendOp ConvertBlendOpToVulkanBlendOp(BlendOp op);

	VkLogicOp ConvertBlendLogicOpToVulkanLogicOp(BlendLogicOp op);

}

struct RenderInstanceCreateInfo
{
	uint32_t maxGPUS;
	uint32_t maxLogicalDevices;
	uint32_t maxWindows;
	uint32_t maxSwapChains;
	uint32_t maxAttachmentGraphTemplates;
	uint32_t maxAttachmentGraphInstances;
	uint32_t maxImagePoolsCount;
	uint32_t maxBufferPoolsCount;
	uint32_t maxRenderTargets;
	uint32_t maxShaderGraphs;
	uint32_t maxShaderHandles;
	uint32_t maxShaderResourceTemplates;
	uint32_t maxDescriptorManagers;
	uint32_t maxComputeQueues;
	uint32_t maxRenderQueues;
	uint32_t maxPipelineTemplates;
	uint32_t maxPipelineInstances;
	uint32_t maxPipelineHandles;
	uint32_t maxAllocations;
	uint32_t maxSubAllocations;
	uint32_t maxGPUCommandsStreams;
	uint32_t maxTextureHandles;
	uint32_t maxSamplerHandles;
	uint32_t maxResourceStatuses;
	uint32_t commandBuffersSize;
	uint32_t commandsCacheSize;
	uint32_t internalLoggerRingSize;
	uint32_t numberOfDriverHostAllocations;
	uint32_t numberOfTransferCommandAllocations;
	uint32_t numberOfResourceUpdateAllocations;
	uint32_t numberOfDriverDeviceAllocations;
	uint32_t numberOfImageMemoryAllocations;
	uint32_t maxConcurrentRecordings;
	OSFileHandle internalRendererHandle;
};

struct LogicalDeviceCreateInfo
{
	GPUFeatureRequest* requestedPhysicalFeatures; 
	LogicalDeviceFeatures* requestedDeviceFeatures;
	size_t driverPermanentSize;
	size_t driverCacheSize;
	size_t deviceInstPermanentSize;
	size_t deviceInstHandleSize;
	size_t deviceInstCacheSize;
	uint32_t maxQueries;
	int surfaceIndexForPresent;
	int physicalDeviceIndex;
};

struct RenderInstance
{
	RenderInstance() = default;

	~RenderInstance();

	void CreateRenderInstance(RenderInstanceCreateInfo *info, Allocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator);

	void DestroySwapChainAttachments(int deviceSelection, EntryHandle swapChainIndex);

	int RecreateSwapChain(int deviceSelection, int swapChainIndex, uint32_t width, uint32_t height);

	int CreateAttachmentResources(int deviceSelection, int graphIndex, int renderPassIndex, int imageCount, EntryHandle* backBufferViews, int* backBufferTextureIds, uint32_t width, uint32_t height,
		RenderPassType rpType, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreateSwapChainAttachment(int deviceSelection, int swapChainIndex, int graphIndex, int renderPassIndex, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreatePerFrameAttachment(int deviceSelection, int graphIndex, int renderPassIndex, int imageCount, uint32_t width, uint32_t height, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreateAttachmentGraphInstance(int deviceSelection, AttachmentGraph* graph);

	int CreateRenderPass(int deviceSelection, AttachmentGraphInstance* graph);

	EntryHandle CreateVulkanComputePipelineTemplate(int deviceSelection, ShaderGraph* graph);

	uint32_t BeginFrame(int deviceSelection, int swapChainIndex);

	int SubmitFrame(int deviceSelection, int swapChainIndex, uint32_t imageIndex);

	void WaitOnRender(int deviceSelection);

	void CreatePipelines(StringView* pipelineDescriptions, int pipelineDescriptionsCount);

	void CreateSwapChainData(int deviceSelection, EntryHandle swapChainIndex, uint32_t width, uint32_t height, bool recreate);

	void UploadHostTransfers(int deviceSelection);

	void UploadDescriptorsUpdates(int deviceSelection);

	void InvokeTransferCommands(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum);

	void UploadImageMemoryTransfers(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum);

	void UploadDeviceLocalTransfers(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum);

	int GetAllocFromBuffer(int deviceSelection, int bufferHandle, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator);

	int CreateImageHandle(
		int deviceSelection,
		size_t gpuMemAddress,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, uint32_t arrayLayers, ImageFormat format, ImageType imageType, ImageUsageFlags usageFlags, int poolIndex);

	int CreateImageView(int deviceSelection, int imageHandle, int firstMip, int mipCount, int firstLayer, int layerCount, ImageViewAspectMask imageAspect, ImageLayout desiredImageLayoutUsage);

	int CreateImagePool(int deviceSelection, size_t size, ImageFormat format, int maxWidth, int maxHeight, ImageUsageFlags usageFlags, MemoryType memType);

	int CreateLogicalDevice(LogicalDeviceCreateInfo* createInfo);

	uint32_t GetSwapChainHeight(int swapChainIndex);

	uint32_t GetSwapChainWidth(int swapChainIndex);

	int CreateGraphicsPipelineObject(int deviceSelection, GraphicsIntermediaryPipelineInfo *info);

	int CreateComputePipelineObject(int deviceSelection, ComputeIntermediaryPipelineInfo* info);

	void DrawScene(int deviceSelection, int commandStreamIndex, uint32_t imageIndex);

	void DestoryTexture(int deviceSelection, EntryHandle handle);

	void IncreaseMSAA(int frameGraph, int renderPassIndex);

	void DecreaseMSAA(int frameGraph, int renderPassIndex);

	void CreateShaderResourceMap(int deviceSelection, ShaderGraph *graph);

	ShaderResourceSetBuilder AllocateShaderResourceSet(int descriptorManagerIndex, int shaderGraphIndex, int targetSet, int setCount);

	EntryHandle CreateShaderResourceSet(ShaderResourceManager* descriptorManager, int deviceSelection, int descriptorSet);

	void GeneratePipelineDescriptorBarriers(int deviceSelection, RecordingBufferObject* rcb, ShaderResourceSetHandle* descriptorid, int descriptorcount, BarrierAccumulator* accumulator, int pipelineIndex);

	void GenerateDrawBindingsBarriers(int deviceSelection, RecordingBufferObject* rcb, PipelineHandle* pipelineHandle, BarrierAccumulator* accumulator);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void EndFrame(int deviceSelection, int commandStreamIndex);

	void AddPipelineToRPGraphicsQueue(int psoIndex, int frameGraphIndex, int renderPass);

	void AddPipelineToComputeQueue(int queueIndex, int psoIndex);

	void ReadData(int deviceSelection, int handle, void* dest, int size, int offset);

	int CreatePipelineFromGraphAndSpec(int deviceSelection, GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer, AttachmentGraphInstance* graphInstance, uint32_t graphRenderPassIndex);

	void UpdateDriverMemory(void* data, int allocationIndex, int size, int allocOffset, TransferType transferType);

	void UpdateImageMemory(void* data, int textureIndex, size_t totalSize, int width, int height, int mipLevels, int mipStart, int layerCount, int layerStart, ImageViewAspectMask mask);

	void InsertTransferCommand(int allocationIndex, int size, int allocOffset, uint32_t fillValue);

	void UpdateShaderResourceArray(ShaderResourceSetHandle handle, int bindingindex, ShaderResourceType type, DeviceHandleArrayUpdate* resourceArrayData);

	void UpdateBufferResourceArray(ShaderResourceSetHandle handle, int bindingindex, ShaderResourceType type, BufferArrayUpdate* resourceArrayData);

	void SwapUpdateCommands();

	int CreateSampler(int deviceSelection, uint32_t maxMipsLevel);

	ImageFormat FindSupportedBackBufferColorFormat(int physicalDeviceIndex, int surfaceLevel, ImageFormat* requestedFormats, uint32_t requestSize);
	ImageFormat FindSupportedDepthFormat(int deviceSelection, ImageFormat* requestedFormats, uint32_t requestSize);

	int CreateAttachmentGraph(int deviceSelection, StringView* attachmentLayout, int* subAttachCount);

	int CreateSwapChainHandle(int deviceSelection, int surfaceIndex, ImageFormat mainBackBufferColorFormat, uint32_t width, uint32_t height);

	void CreateShaderGraphs(int deviceSelection, StringView* shaderGraphLayouts, int shaderGraphLayoutsCount);

	int CreateGraphicRenderStateObject(int deviceSelection, int shaderGraphIndex, int pipelineDescriptionIndex, int* frameGraphAttachments, int* perFrameRenderPassSelection, int frameGraphCount);
	int CreateComputePipelineStateObject(int deviceSelection, int shaderGraphIndex);

	void ResetCommandList(int commandStreamIndex);

	void CreateGraphicsQueueForAttachments(int frameGraphIndex, int renderPassIndex, uint32_t pipelineCount);

	int CreateComputeQueue();

	void AddCommandQueue(int commandStreamIndex, int handleIndex, GPUCommandStreamType type);

	int UploadFrameAttachmentResource(int frameGraph, int resourceIndex, int perTextureViewIndex, ShaderResourceSetHandle handle, int bindingIndex, int textureStart);

	void PipelineUpdateIndirectCommandBuffer(int pipelineIndex, int allocationIndex);
	void PipelineUpdateVertexBuffer(int pipelineIndex, int allocationIndex, uint32_t vertexCount);
	void PipelineUpdateIndexBuffer(int pipelineIndex, int allocationIndex, uint32_t indexCount, uint32_t indexStride);
	void PipelineUpdateIndirectCountBuffer(int pipelineIndex, int allocationIndex);
	void PipelineUpdateDispatchCommands(int pipelineIndex, uint32_t x, uint32_t y, uint32_t z);

	int CreateUniversalBuffer(int deviceSelection, size_t size, MemoryType bufferMemoryType);

	void GetGPURequestedImageSizeAndAlignment(int deviceSelection, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers, ImageFormat type, ImageUsageFlags usageFlags, size_t* actualImageSize, size_t* actualAlignment);

	int CreateDescriptorHeap(int deviceSelection, DescriptorTypes* types, uint32_t* descriptorCountPerFrame, uint32_t numDescriptorTypesCount, uint32_t maxDescriptorSets, uint32_t maxShaderResourceSets);

	int CreateWindowedSurface(OSWindowInternalData* windowData);

	int CreateHighLevelInstance(uint32_t vkDriverSpecificMemory, uint32_t vkDriverCacheSize, uint32_t instancePermanentSpecificMemory, uint32_t instanceCacheMemory);

	int CreatePhysicalDeviceAdapter(GPUFeatureRequest* requestedPhysicalFeatures, LogicalDeviceFeatures* requestedDeviceFeatures);

	int OpenPhysicalDevicePicker();

	int CreatePhysicalDeviceAdapterWithQuerying(GPUFeatureRequest* requestedPhysicalFeatures, LogicalDeviceFeatures* requestedDeviceFeatures);
	
	void ClosePhysicalDevicePicker();

	int CreatePerFrameStagingBuffers(int deviceSelection, uint32_t bufferSize);

	int CreateSuballocation(int deviceSelection, int parentAllocation, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator);

	int CreateResourceStatusActions(ResourceStatus* status, int numberOfCurrentActions, int numberOfCurrentStages, int numberOfCurrentLayouts);

	void InitializeResourceStatus(ResourceStatus* status, int numberOfCurrentActions, int numberOfCurrentStages, int numberOfCurrentLayouts, BarrierAction action, BarrierStage stage, ImageLayout imageLayout);

	void TransitionImageLayout(VKDevice* dev, RecordingBufferObject* rcb, int imageIndex, int perImageViewIndex, BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator, int pipelineIndex);

	void TransitionImageLayout(VKDevice* dev, RecordingBufferObject* rcb, EntryHandle imageIndex, int mipStart, int mipCount, int totalMipCount, int layerStart, int layerCount,
		ImageViewAspectMask mask, ImageLayout requestedLayout, ResourceStatus* status,
		BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator, int pipelineIndex);

	void InsertBufferBarrier(VKDevice* dev, RecordingBufferObject* rcb, int allocationIndex, BarrierStage destBarrierStage, ShaderResourceHeader* header, int pipelineIndex, BarrierAccumulator* accumulator);

	void InsertBufferBarrier(VKDevice* dev, RecordingBufferObject* rcb, int allocationIndex, BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator);

	int CreateAttachmentImage(
		uint32_t width, uint32_t height,
		uint32_t arrayLayers, uint32_t mipCount,
		ImageType imageType, int sampleCount,
		ImageFormat format, ImageUsageFlags usageFlags,
		DeviceSlabAllocator* attachmentAllocator, ImageLayout initialLayout,
		VKDevice* dev, int imageMemoryPoolIndex, ResourceStatusType resourceType);

	int CreateAttachmentImageView(int textureIndex, uint32_t firstMip, uint32_t mipCount, uint32_t firstArrayLayer, uint32_t arrayLayerCount, ImageViewAspectMask mask, ImageLayout desiredLayout, VKDevice* dev);

	int CreateAttachmentImageView(int deviceSelection, int attachmentGraphInstance, int attachmentResourceIndex, uint32_t firstMip, uint32_t mipCount, uint32_t firstArrayLayer, uint32_t arrayLayerCount, ImageViewAspectMask mask, ImageLayout desiredLayout);

	int CreateGPUCommandStream(int maxGPUCommandCount);

	void CreateDriverSpecificBarrierArenas(BarrierAccumulator* barrierAccumulator, int maxTextures, int maxAllocations);

	void InsertAccumulatedBarriers(RecordingBufferObject* rcb, BarrierAccumulator* accumulator);

	uint32_t PopBarrierAccumulator();

	void ReturnBarrierAccumulator(uint32_t returnIndex);

	IntraPassBarrier* GetIntraPassBarrier(BarrierAccumulator* accum, BarrierType type, int pipelineIndex, void* driverBarrierData);

	void InsertIntraPassBarrier(RecordingBufferObject* rbo, BarrierAccumulator* accum, int pipelineIndex);

	void ResetIntraBarrierAccumulator(BarrierAccumulator* accumulator);
	
	void DeletePhysicalDevice(int physicalDeviceIndex);

	void DeleteLogicalDevice(int logicalDeviceIndex);

	void DeleteWindowSurface(int windowSurfaceIndex);

	void DeleteSwapChain(int swapChainIndex);

	void DeleteBufferHandle(int bufferHandleIndex);

	void DeleteImagePools(int imagePoolIndex);
	
	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	VKInstance *vkInstance = nullptr;

	RenderPhysicalDeviceContainer* physicalDeviceIndices{};

	RenderLogicalDeviceContainer* logicalDeviceIndices{};

	PoolAllocator<RenderWindowSpecificData> windowsSurfaces{};

	PoolAllocator<RenderSwapchainData> swapChains{};

	PoolAllocator<RenderBufferDescription> bufferHandles{};

	PoolAllocator<ImagePoolDescription> imagePools{};

	PoolAllocator<RenderPipelineDescription> pipelineInstancesIdentifier{};

	PoolAllocator<PipelineHandle> pipelineHandles{};
	
	PoolAllocator<AttachmentGraph> attachmentGraphs{};

	PoolAllocator<AttachmentGraphInstance> attachmentGraphsInstances{};

	PoolAllocator<RenderQueue> renderTargetQueues{};

	PoolAllocator<ComputeQueue> computeQueues{};

	PoolAllocator<RenderTextureDescription> textureResourceHandles{};

	PoolAllocator<RenderImageViewDescription> textureViewsResourceHandles{};

	PoolAllocator<EntryHandle> samplerResourceHandles{};

	PoolAllocator<ResourceStatus> resourceStatuses{};

	PoolAllocator<GenericPipelineStateInfo> pipelineInfos{};

	PoolAllocator<EntryHandle> renderPasses{};

	PoolAllocator<EntryHandle> mainRenderTargets{};

	PoolAllocator<EntryHandle> shaderResourceTemplates{};

	PoolAllocator<RenderAllocation> allocations{};

	PoolAllocator<ShaderResourceManager> descriptorManagers{};

	PoolAllocator<GPUCommandStreamAllocator> gpuCommandStreams{};
	
	ShaderGraphsHolder shaderGraphs;

	MemoryDriverTransferPool driverHostMemoryUpdater;

	TransferCommandsPool transferCommandPool;

	ShaderResourceUpdatePool descriptorUpdatePool;

	MemoryDriverTransferPool driverDeviceMemoryUpdater;

	ImageMemoryUpdateManager imageMemoryUpdateManager;

	RingAllocator* cacheAllocator;

	Allocator* storageAllocator;

	RingAllocator* updateCommandsCache;

	SlabAllocator* updateCommandBuffers[2];

	Logger* internalRendererLogger;

	uint32_t* barriersQueue = nullptr;

	BarrierAccumulator* barrierAccumulators = nullptr;

	uint32_t maxBarrierAccumulationCount = 0;
	uint32_t currentBarrierAccumulationTop = 0;

	int currentUpdateCommandBuffer = 0;
	uint32_t currentFrame = 0;
	uint32_t previousFrame = ~0U;
	uint32_t physicalDeviceCounter = 0;
	uint32_t logicalDeviceCounter = 0;
	uint32_t maxLogicalDevices = 0;
	uint32_t maxPhysicalDevices = 0;
	uint32_t physicalDevicesOnComputerPerDriver = 0;
};

namespace GlobalRenderer 
{
	extern RenderInstance gRenderInstance;
}