#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "AppAllocator.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VertexTypes.h"
#include "ResourceDependencies.h"
#include "RenderInstanceManagement.h"
#include "ShaderResourceSet.h"
#include "ThreadManager.h"
#include "WindowManager.h"


namespace API {

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
}

struct RenderInstance
{

	RenderInstance(SlabAllocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator);

	~RenderInstance();

	void DestroySwapChainAttachments(EntryHandle swapChainIndex);

	int RecreateSwapChain(EntryHandle swapChainIndex);

	int CreateAttachmentResources(int graphIndex, int renderPassIndex, int imageCount, EntryHandle* backBufferViews, uint32_t width, uint32_t height, RenderPassType rpType, AttachmentClear* clears);

	int CreateSwapChainAttachment(EntryHandle swapChainIndex, int graphIndex, int renderPassIndex, AttachmentClear* clears);

	int CreatePerFrameAttachment(int graphIndex, int renderPassIndex, int imageCount, uint32_t width, uint32_t height, AttachmentClear* clears);

	int CreateFrameGraphInstance(AttachmentGraph* graph);

	int CreateRenderPass(uint32_t index, AttachmentGraphInstance* graph);

	EntryHandle CreateVulkanComputePipelineTemplate(ShaderGraph* graph);

	uint32_t BeginFrame(EntryHandle swapChainIndex);

	int SubmitFrame(EntryHandle swapChainIndex, uint32_t imageIndex);

	void WaitOnRender();

	void CreatePipelines(StringView* pipelineDescriptions, int pipelineDescriptionsCount);

	void CreateSwapChainData(EntryHandle swapChainIndex, uint32_t width, uint32_t height, bool recreate);

	void UploadHostTransfers();

	void UploadDescriptorsUpdates();

	void InvokeTransferCommands(RecordingBufferObject* rbo);

	void UploadImageMemoryTransfers(RecordingBufferObject* rbo);

	void UploadDeviceLocalTransfers(RecordingBufferObject* rbo);

	int GetAllocFromBuffer(int bufferHandle, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType);

	EntryHandle CreateImageHandle(
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex, EntryHandle samplerIndex);

	EntryHandle CreateCubeImageHandle(
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat format, int poolIndex, EntryHandle samplerIndex);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	int CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment);

	void CreateVulkanRenderer(WindowManager* window,  int attachmentGraphCount);

	uint32_t GetSwapChainHeight(EntryHandle swapChainIndex);

	uint32_t GetSwapChainWidth(EntryHandle swapChainIndex);

	int CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info, bool addToGraph);

	int CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info);

	void CreateRenderGraphData(int frameGraph, int* descsSets, int descCount);

	void DrawScene(uint32_t imageIndex);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void DestoryTexture(EntryHandle handle);

	void IncreaseMSAA(int frameGraph, int renderPassIndex);

	void DecreaseMSAA(int frameGraph, int renderPassIndex);

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	void AddVulkanMemoryBarrier(VKPipelineObject* vkPipelineObject, int* descriptorid, int descriptorcount);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void EndFrame();

	void AddPipelineToRPGraphicsQueue(int psoIndex, int frameGraphIndex, int renderPass);

	void AddPipelineToComputeQueue(int queueIndex, int psoIndex);

	void ReadData(int handle, void* dest, int size, int offset);

	int CreatePipelineFromGraphAndSpec(GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer, AttachmentGraphInstance* graphInstance, uint32_t graphRenderPassIndex);

	void UpdateDriverMemory(void* data, int allocationIndex, int size, int allocOffset, TransferType transferType);

	void UpdateImageMemory(void* data, EntryHandle textureIndex, uint32_t* imageSizes, size_t totalSize, int width, int height, int mipLevels, int layers, ImageFormat format);

	void InsertTransferCommand(int allocationIndex, int size, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action);

	void UpdateShaderResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, DeviceHandleArrayUpdate* resourceArrayData);

	void UpdateBufferResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, BufferArrayUpdate* resourceArrayData);

	void SwapUpdateCommands();

	EntryHandle CreateSampler(uint32_t maxMipsLevel);

	int CreateRSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight);
	int CreateDSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight);

	ImageFormat FindSupportedBackBufferColorFormat(ImageFormat* requestedFormats, uint32_t requestSize);
	ImageFormat FindSupportedDepthFormat(ImageFormat* requestedFormats, uint32_t requestSize);

	int CreateAttachmentGraph(StringView* attachmentLayout, int* subAttachCount);

	EntryHandle CreateSwapChainHandle(ImageFormat mainBackBufferColorFormat, uint32_t width, uint32_t height);

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

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	std::array<EntryHandle, MAX_FRAMES_IN_FLIGHT> currentCBIndex;

	uint32_t maxMSAALevels = 0;

	std::array<EntryHandle, 10> bufferHandles{};
	std::array<BufferType, 10> bufferTypes{};
	int bufferPoolsCounter = 0;

	std::array<EntryHandle, 20> renderTargets{};
	std::array<EntryHandle, 20> renderPasses{};

	std::array<EntryHandle, 9> imagePools{};
	int imagePoolCounter = 0;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	ShaderGraphsHolder<50, 60> vulkanShaderGraphs;
	
	ShaderResourceManager<50> descriptorManager;

	std::array<EntryHandle*, 25> pipelinesIdentifier{};
	std::array<PipelineInstanceData, 25> pipelinesInstancesInfo{};

	std::array<EntryHandle, 60> vulkanDescriptorLayouts{};

	RenderAllocationHolder<100> allocations{};

	ArrayAllocator<EntryHandle, 100> renderStateObjects{};

	ArrayAllocator<EntryHandle, 100> computeStateObjects{};

	ArrayAllocator<PipelineHandle, 200> stateObjectHandles{};

	ArrayAllocator<EntryHandle, 10> computeQueues{};

	std::array<GPUCommand, 10> gpuCommands{};
	int gpuCommandCount = 0;

	int minUniformAlignment;
	int minStorageAlignment; 

	MemoryDriverTransferPool driverHostMemoryUpdater;

	TransferCommandsPool transferCommandPool;

	ShaderResourceUpdatePool descriptorUpdatePool;

	MemoryDriverTransferPool driverDeviceMemoryUpdater;

	ImageMemoryUpdateManager imageMemoryUpdateManager;

	EntryHandle stagingBuffers[MAX_FRAMES_IN_FLIGHT];

	std::array<GenericPipelineStateInfo, 15> pipelineInfos{};

	RingAllocator* cacheAllocator;
	SlabAllocator* storageAllocator;

	RingAllocator* updateCommandsCache;

	SlabAllocator* updateCommandBuffers[2];

	int currentUpdateCommandBuffer = 0;

	EntryHandle mainRenderTargets[20]{};

	ArrayAllocator<EntryHandle, 10> renderTargetQueues{};

	AttachmentGraph* attachmentGraphs;

	AttachmentGraphInstance* attachmentGraphsInstances;

	int attachmentGraphInstancesCount = 0;

	int pipelineIdentifierCount = 0;
};

namespace GlobalRenderer {
	extern RenderInstance* gRenderInstance;
}