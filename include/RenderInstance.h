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

	RenderInstance(SlabAllocator* instanceStorageAllocator, SlabAllocator* instanceCacheAllocator);

	~RenderInstance();

	void CreateDepthImage( uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void DestroySwapChainAttachments();

	int RecreateSwapChain();

	void CreateRenderPass(uint32_t index, VkSampleCountFlagBits sampleCount);

	EntryHandle CreateVulkanComputePipelineTemplate(ShaderGraph* graph);

	uint32_t BeginFrame();

	int SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void WaitOnRender();

	void CreatePipelines(std::string* shaderGraphLayouts, int shaderGraphLayoutsCount, std::string* pipelineDescriptions, int pipelineDescriptionsCount);

	void CreateSwapChain(uint32_t width, uint32_t height, bool recreate);

	void UploadHostTransfers();

	void UploadDescriptorsUpdates();

	void InvokeTransferCommands(RecordingBufferObject* rbo);

	void UploadImageMemoryTransfers(RecordingBufferObject* rbo);

	void UploadDeviceLocalTransfers(RecordingBufferObject* rbo);

	int GetAllocFromBuffer(size_t structureSize, size_t copiesOfStructure, uint32_t alignment, AllocationType allocType, ComponentFormatType formatType, int storageLocation);

	EntryHandle CreateImageHandle(
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	EntryHandle CreateCubeImageHandle(
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat format, int poolIndex);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	int CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment);

	void CreateVulkanRenderer(WindowManager* window, std::string* shaderGraphLayouts, int shaderGraphLayoutsCount, std::string* pipelineDescriptions, int pipelineDescriptionsCount);

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	int CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info, bool addToGraph);

	int CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info);

	void CreateRenderTargetData(int* desc, int descCount);

	void DrawScene(uint32_t imageIndex);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void DestoryTexture(EntryHandle handle);

	void IncreaseMSAA();

	void DecreaseMSAA();

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	uint32_t GetDynamicOffsetsForDescriptorSet(int descriptorSet, uint32_t* dynamicOffsets, uint32_t topOfDynamicOffsets);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	void AddVulkanMemoryBarrier(VKPipelineObject* vkPipelineObject, int* descriptorid, int descriptorcount);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void SetActiveComputePipeline(uint32_t objectIndex, bool active);

	void EndFrame();

	void AddPipelineToMainQueue(int psoIndex, int computeorgraphics);

	void ReadData(int handle, void* dest, int size, int offset);

	void CreatePipelineFromGraphAndSpec(GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle swapChainIndex;
	EntryHandle globalIndex, globalDeviceBufIndex;
	EntryHandle computeGraphIndex;
	std::array<EntryHandle, MAX_FRAMES_IN_FLIGHT> currentCBIndex;
	EntryHandle graphicsOTQ, computeOTQ;

	uint32_t currentMSAALevel = 0;
	uint32_t maxMSAALevels = 0;

	std::array<EntryHandle, 5> swapchainRenderTargets{};
	std::array<EntryHandle, 5> depthViews{};
	std::array<EntryHandle, 5> colorViews{};
	std::array<EntryHandle, 5> depthImages{};
	std::array<EntryHandle, 5> colorImages{};
	std::array<EntryHandle, 5> renderPasses{};

	std::array<EntryHandle, 9> imagePools{};
	int imagePoolCounter = 0;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	ImageFormat depthFormat = ImageFormat::IMAGE_UNKNOWN;
	ImageFormat colorFormat = ImageFormat::IMAGE_UNKNOWN;

	ShaderGraphsHolder<50, 60> vulkanShaderGraphs{};
	
	ShaderResourceManager<50> descriptorManager{};

	std::array<EntryHandle*, 25> pipelinesIdentifier{};
	std::array<EntryHandle, 60> vulkanDescriptorLayouts{};
	std::array<ShaderDetails*, 70> shaderDetails{};
	char* shaderDetailsData;
	std::atomic<int> shaderDetailAlloc = 0;

	RenderAllocationHolder<100> allocations{};

	ArrayAllocator<EntryHandle, 100> renderStateObjects{};

	ArrayAllocator<EntryHandle, 100> computeStateObjects{};

	ArrayAllocator<int, 100> graphIndices{};

	ArrayAllocator<PipelineHandle, 200> stateObjectHandles{};

	int width = 0; 
	int height = 0;

	int minUniformAlignment;
	int minStorageAlignment; 


	HostDriverTransferPool<3> transferPool;

	TransferCommandsPool<3> transferCommandPool;

	ShaderResourceUpdatePool<3> descriptorUpdatePool;

	DeviceMemoryUpdateManager deviceMemoryUpdater;

	EntryHandle stagingBuffers[MAX_FRAMES_IN_FLIGHT];

	ImageMemoryUpdateManager imageMemoryUpdateManager;

	EntryHandle samplerIndex;

	std::array<GenericPipelineStateInfo, 15> pipelineInfos;

	SlabAllocator* cacheAllocator;
	SlabAllocator* storageAllocator;
};

namespace GlobalRenderer {
	extern RenderInstance* gRenderInstance;
}