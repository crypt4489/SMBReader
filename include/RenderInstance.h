#pragma once

#include <functional>
#include <vulkan/vulkan.h>

#include "allocator/AppAllocator.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "math/VertexTypes.h"
#include "ShaderManagement.h"
#include "RenderInstanceManagement.h"
#include "ShaderResourceSet.h"
#include "ThreadManager.h"
#include "WindowManager.h"

namespace API 
{
	VkFormat ConvertComponentFormatTypeToVulkanFormat(ComponentFormatType type);

	VkCompareOp ConvertRasterizerTestToVulkanCompareOp(RasterizerTest testApp);

	VkFormat ConvertImageFormatToVulkanFormat(ImageFormat format);

	VkPrimitiveTopology ConvertTopology(PrimitiveType type);

	VkAccessFlags ConvertResourceActionToVulkanAccessFlags(BarrierAction action);

	VkPipelineStageFlags ConvertBarrierStageToVulkanPipelineStage(BarrierStage sourceStage);

	VkShaderStageFlags ConvertShaderStageToVulkanShaderStage(ShaderStageType type);

	VkImageLayout ConvertImageLayoutToVulkanImageLayout(ImageLayout layout);

	void ConvertVertexInputToVKVertexAttrDescription(VertexInputDescription* inputDescs, int numInputDescs, int vertexBufferLocation, VkVertexInputAttributeDescription* attrs);

	VkCullModeFlags ConvertCullMode(CullMode mode);

	VkFrontFace ConvertTriangleWinding(TriangleWinding winding);

	void ConvertGPUFeatureRequestToVkPhysicalDeviceProperties(const GPUFeatureRequest* request,
		VkPhysicalDeviceFeatures2* features2,
		VkPhysicalDeviceVulkan12Features* features12);
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
	uint32_t maxShaderResourceSets;
	uint32_t maxShaderResourceTemplates;
	uint32_t maxShaderResourceSetSlabAllocator;
	uint32_t maxComputeQueues;
	uint32_t maxRenderQueues;
	uint32_t maxPipelineTemplates;
	uint32_t maxPipelineInstances;
	uint32_t maxPipelineHandles;
	uint32_t maxAllocations;
	uint32_t maxGPUCommands;
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

	void CreateRenderInstance(RenderInstanceCreateInfo *info, SlabAllocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator);

	void DestroySwapChainAttachments(EntryHandle swapChainIndex);

	int RecreateSwapChain(int swapChainIndex, uint32_t width, uint32_t height);

	int CreateAttachmentResources(int graphIndex, int renderPassIndex, int imageCount, EntryHandle* backBufferViews, uint32_t width, uint32_t height, 
		RenderPassType rpType, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreateSwapChainAttachment(int swapChainIndex, int graphIndex, int renderPassIndex, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreatePerFrameAttachment(int graphIndex, int renderPassIndex, int imageCount, uint32_t width, uint32_t height, AttachmentClear* clears, DeviceSlabAllocator* rsvAllocator, DeviceSlabAllocator* dsvAllocator, int rsvPoolIndex, int dsvPoolIndex);

	int CreateFrameGraphInstance(AttachmentGraph* graph);

	int CreateRenderPass(AttachmentGraphInstance* graph);

	EntryHandle CreateVulkanComputePipelineTemplate(ShaderGraph* graph);

	uint32_t BeginFrame(int swapChainIndex);

	int SubmitFrame(int swapChainIndex, uint32_t imageIndex);

	void WaitOnRender();

	void CreatePipelines(StringView* pipelineDescriptions, int pipelineDescriptionsCount);

	void CreateSwapChainData(EntryHandle swapChainIndex, uint32_t width, uint32_t height, bool recreate);

	void UploadHostTransfers();

	void UploadDescriptorsUpdates();

	void InvokeTransferCommands(RecordingBufferObject* rbo);

	void UploadImageMemoryTransfers(RecordingBufferObject* rbo);

	void UploadDeviceLocalTransfers(RecordingBufferObject* rbo);

	int GetAllocFromBuffer(int bufferHandle, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator);

	int CreateImageHandle(
		size_t gpuMemAddress,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex, int samplerIndex);

	int CreateCubeImageHandle(
		size_t gpuMemAddress,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat format, int poolIndex, int samplerIndex);

	int CreateStorageImage(
		size_t gpuMemAddress,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat format, int poolIndex, int samplerIndex);

	int CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment);

	int CreateLogicalDevice(LogicalDeviceCreateInfo* createInfo);

	uint32_t GetSwapChainHeight(int swapChainIndex);

	uint32_t GetSwapChainWidth(int swapChainIndex);

	int CreateGraphicsPipelineObject(GraphicsIntermediaryPipelineInfo *info, bool addToGraph);

	int CreateComputePipelineObject(ComputeIntermediaryPipelineInfo* info);

	void DrawScene(uint32_t imageIndex);

	void DestoryTexture(EntryHandle handle);

	void IncreaseMSAA(int frameGraph, int renderPassIndex);

	void DecreaseMSAA(int frameGraph, int renderPassIndex);

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	void AddVulkanMemoryBarrier(RecordingBufferObject* rcb, int* descriptorid, int descriptorcount);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void EndFrame();

	void AddPipelineToRPGraphicsQueue(int psoIndex, int frameGraphIndex, int renderPass);

	void AddPipelineToComputeQueue(int queueIndex, int psoIndex);

	void ReadData(int handle, void* dest, int size, int offset);

	int CreatePipelineFromGraphAndSpec(GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer, AttachmentGraphInstance* graphInstance, uint32_t graphRenderPassIndex);

	void UpdateDriverMemory(void* data, int allocationIndex, int size, int allocOffset, TransferType transferType);

	void UpdateImageMemory(void* data, int textureIndex, size_t totalSize, int width, int height, int mipLevels, int layers, ImageFormat format);

	void InsertTransferCommand(int allocationIndex, int size, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action);

	void UpdateShaderResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, DeviceHandleArrayUpdate* resourceArrayData);

	void UpdateBufferResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, BufferArrayUpdate* resourceArrayData);

	void SwapUpdateCommands();

	int CreateSampler(uint32_t maxMipsLevel);

	int CreateRSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight);
	int CreateDSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight);

	ImageFormat FindSupportedBackBufferColorFormat(int physicalDeviceIndex, int surfaceLevel, ImageFormat* requestedFormats, uint32_t requestSize);
	ImageFormat FindSupportedDepthFormat(ImageFormat* requestedFormats, uint32_t requestSize);

	int CreateAttachmentGraph(StringView* attachmentLayout, int* subAttachCount);

	int CreateSwapChainHandle(int surfaceIndex, ImageFormat mainBackBufferColorFormat, uint32_t width, uint32_t height);

	void CreateShaderGraphs(StringView* shaderGraphLayouts, int shaderGraphLayoutsCount);

	int CreateGraphicRenderStateObject(int shaderGraphIndex, int pipelineDescriptionIndex, int* frameGraphAttachments, int* perFrameRenderPassSelection, int frameGraphCount);
	int CreateComputePipelineStateObject(int shaderGraphIndex);

	void ResetCommandList();

	void CreateGraphicsQueueForAttachments(int frameGraphIndex, int renderPassIndex, uint32_t pipelineCount);

	int CreateComputeQueue(uint32_t maxNumPipelines);

	void AddCommandQueue(int index, GPUCommandStreamType type);

	int UploadFrameAttachmentResource(int frameGraph, int resourceIndex, int descriptorSet, int bindingIndex, int textureStart);

	void PipelineUpdateInstanceCommandsBuffer(int pipelineIndex, int allocationIndex);
	void PipelineUpdateVertexBuffer(int pipelineIndex, int allocationIndex, int vertexCount, int vertexBuffersubAlloc);
	void PipelineUpdateIndexBuffer(int pipelineIndex, int allocationIndex, int indexCount, int indexStride, int indexSubAlloc);
	void PipelineUpdateIndirectCountBuffer(int pipelineIndex, int allocationIndex);
	void PipelineUpdateDispatchCommands(int pipelineIndex, int x, int y, int z);

	int CreateUniversalBuffer(size_t size, BufferType bufferMemoryType);

	void GetGPURequestedImageSizeAndAlignment(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers, ImageFormat type, size_t* actualImageSize, size_t* actualAlignment);

	int CreateDescriptorHeap(DescriptorTypes* types, uint32_t* descriptorCountPerFrame, uint32_t numDescriptorTypesCount, uint32_t maxSets);

	int CreateWindowedSurface(OSWindowInternalData* windowData);

	int CreateHighLevelInstance(uint32_t vkDriverSpecificMemory, uint32_t vkDriverCacheSize, uint32_t instancePermanentSpecificMemory, uint32_t instanceCacheMemory);

	int CreatePhysicalDeviceAdapter(int windowIndex, GPUFeatureRequest* requestedPhysicalFeatures, LogicalDeviceFeatures* requestedDeviceFeatures);

	int CreatePerFrameStagingBuffers(uint32_t bufferSize);

	void SetCurrentInstanceDeviceIndex(int selectedDeviceIndex);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	VKInstance *vkInstance = nullptr;

	RenderPhysicalDeviceContainer* physicalDeviceIndices{};

	RenderLogicalDeviceContainer* logicalDeviceIndices{};

	PoolAllocator<RenderWindowSpecificData> windowsSurfaces{};

	PoolAllocator<RenderSwapchainData> swapChains{};

	PoolAllocator<EntryHandle> bufferHandles{};
	
	BufferType* bufferTypes{};
	
	int* bufferToResourceStatus{};

	PoolAllocator<EntryHandle> imagePools{};

	PoolAllocator<EntryHandle*> pipelineInstancesIdentifier{};
	
	PipelineInstanceData* pipelinesInstancesInfo{};

	PoolAllocator<PipelineHandle> pipelineHandles{};

	GPUCommand* gpuCommands{};
	
	PoolAllocator<AttachmentGraph> attachmentGraphs{};

	PoolAllocator<AttachmentGraphInstance> attachmentGraphsInstances{};

	PoolAllocator<RenderQueue> renderTargetQueues{};

	PoolAllocator<ComputeQueue> computeQueues{};

	PoolAllocator<EntryHandle> textureResourceHandles{};

	int* textureToResourceStatus{};

	PoolAllocator<EntryHandle> samplerResourceHandles{};

	PoolAllocator<ResourceStatus> resourceStatuses{};

	PoolAllocator<GenericPipelineStateInfo> pipelineInfos{};

	PoolAllocator<EntryHandle> renderPasses{};

	PoolAllocator<EntryHandle> mainRenderTargets{};

	PoolAllocator<EntryHandle> shaderResourceTemplates{};

	PoolAllocator<RenderAllocation> allocations{};

	ShaderResourceManager descriptorManager{};
	
	ShaderGraphsHolder shaderGraphs;

	MemoryDriverTransferPool driverHostMemoryUpdater;

	TransferCommandsPool transferCommandPool;

	ShaderResourceUpdatePool descriptorUpdatePool;

	MemoryDriverTransferPool driverDeviceMemoryUpdater;

	ImageMemoryUpdateManager imageMemoryUpdateManager;

	RingAllocator* cacheAllocator;

	SlabAllocator* storageAllocator;

	RingAllocator* updateCommandsCache;

	SlabAllocator* updateCommandBuffers[2];

	Logger* internalRendererLogger;

	int gpuCommandCount = 0;
	int maxGPUCommandCount = 0;
	int currentUpdateCommandBuffer = 0;
	uint32_t currentFrame = 0;
	uint32_t previousFrame = ~0ui32;
	int physicalDeviceCounter = 0;
	int logicalDeviceCounter = 0;
	int instanceCurrentLogicalDeviceIndex = -1;
};

namespace GlobalRenderer 
{
	extern RenderInstance gRenderInstance;
}