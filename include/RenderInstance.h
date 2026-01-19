#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "AppAllocator.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "ResourceDependencies.h"
#include "ShaderResourceSet.h"
#include "ThreadManager.h"
#include "WindowManager.h"


namespace API {

	VkCompareOp ConvertDepthTestAppToVulkan(DepthTest testApp);

	VkFormat ConvertSMBToVkFormat(ImageFormat format);

	VkPrimitiveTopology ConvertTopology(PrimitiveType type);
}

struct GraphicsIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferHandle;
	uint32_t vertexCount;
	uint32_t pipelinename;
	uint32_t descCount;
	int *descriptorsetid;
	int indexBufferHandle;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t indexSize;
	uint32_t indexOffset;
	uint32_t vertexOffset;
};

struct ComputeIntermediaryPipelineInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
};

struct IndirectIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferIndex;
	uint32_t vertexCount;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
	int indexBufferHandle;
	uint32_t indexSize;
	uint32_t indexOffset;
	uint32_t vertexOffset;
	int indirectAllocation;
	int indirectDrawCount;
};


#define FULL_ALLOCATION_SIZE 0
#define ABSOLUTE_ALLOCATION_OFFSET 0

enum AllocationType
{
	STATIC = 0,
	PERFRAME = 1,
	PERDRAW = 2
};

struct RenderAllocation
{
	EntryHandle memIndex;
	size_t offset;
	size_t deviceAllocSize;
	size_t requestedSize;
	size_t alignment;
	AllocationType allocType;
};

/*
struct HostTransferRegion
{
	uint32_t offsetinstaging;
	size_t size;
	int copyCount;
};


struct HostDriverTransferPool
{
	char stagingbuffer[8192];
	std::array<HostTransferRegion, 50> transferRegions;
	int currentTransferRegionsRead = 0;
	int currentTransferRegionsWrite = 0;
	int currentStagingBufferRead = 0;
	int currentStagingBufferWrite = 0;

	void Create(void* data, size_t size, int allocationIndex, int copyCount)
	{
		if ((currentStagingBufferWrite + size) >= 8192)
		{
			return;
		}

		memcpy(stagingbuffer + currentStagingBufferWrite, data, size);


		HostTransferRegion* region = &transferRegions[allocationIndex];
		region->copyCount = copyCount;
		region->offsetinstaging = currentStagingBufferWrite;
		region->size = size;

		currentStagingBufferWrite += size;
	}
};
*/

template <int N>
struct RenderAllocationHolder
{
	std::array<RenderAllocation, N> allocations;
	
	RenderAllocation operator[](size_t index)
	{
		if (index >= N || index < 0)
			return { EntryHandle(), ~0ui64, ~0ui64 };

		return allocations[index];
	}

	RenderAllocation operator[](int index)
	{
		if (index >= N || index < 0)
			return { EntryHandle(), ~0ui64, ~0ui64 };

		return allocations[index];
	}

	int Allocate()
	{
		return allocationsIndex.fetch_add(1);
	}

	std::atomic<int> allocationsIndex;

};

enum PipelineLabels
{
	MESH_INTERPOLATE = 0,
	TEXT = 1,
	GENERIC = 2,
	POLY = 3,
};




/*
struct TransferRegion
{
	int hostordevice;
	uint32_t offsetinstaging;
	size_t size;
	size_t offsetindriverbuffer;
};

struct TransferPool
{
	std::array<EntryHandle, 3> stagingBuffer; //each frame in flight loads data into staging buffer
	std::array<TransferRegion, 50> transferRegions;
	int currentTransferRegionsCount = 0;
};
*/

struct RenderInstance
{

	RenderInstance();

	~RenderInstance();

	void CreateDepthImage( uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void DestroySwapChainAttachments();

	int RecreateSwapChain();

	void CreateRenderPass(uint32_t index, VkSampleCountFlagBits sampleCount);

	void UsePipelineBuilders(VKGraphicsPipelineBuilder* generic, VKGraphicsPipelineBuilder* text, VKGraphicsPipelineBuilder* debug, VkSampleCountFlagBits flags);

	EntryHandle CreateVulkanGraphicPipelineTemplate(VKGraphicsPipelineBuilder* pipeline, ShaderGraph* graph);

	EntryHandle CreateVulkanComputePipelineTemplate(ShaderGraph* graph);

	uint32_t BeginFrame();

	int SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void WaitOnRender();

	void CreatePipelines();

	void CreateSwapChain(uint32_t width, uint32_t height, bool recreate);

	void UpdateAllocation(void* data, size_t handle, size_t size, size_t offset, size_t frame, int copies);

	int GetAllocFromUniformBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	int GetAllocFromDeviceStorageBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	int GetAllocFromDeviceBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	

	EntryHandle CreateImage(
		char* imageData,
		uint32_t* sizes,
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	int CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment);

	void CreateVulkanRenderer(WindowManager* window);

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	int CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info);

	int CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info);

	int CreateIndirectVulkanPipelineObject(IndirectIntermediaryPipelineInfo* info, bool addToGraph);

	void CreateRenderTargetData(int* desc, int descCount);

	void DrawScene(uint32_t imageIndex);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void DestoryTexture(EntryHandle handle);

	void IncreaseMSAA();

	void DecreaseMSAA();

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	uint32_t GetDynamicOffsetsForDescriptorSet(int descriptorSet, std::vector<uint32_t>& dyanmicOffsets);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	ShaderResourceHeader* PopShaderResourceBarrier(int descriptorSet, int* counter);

	void AddVulkanMemoryBarrier(VKPipelineObject* vkPipelineObject, int descriptorid);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void SetActiveComputePipeline(uint32_t objectIndex, bool active);

	void UpdateSamplerBinding(int descriptorSet, int bindingIndex, EntryHandle* handles, uint32_t destinationArray, uint32_t texCount);

	int GetBufferAllocationViaDescriptor(int descriptorSet, int bindingIndex);

	EntryHandle CreateBufferView(int allocationIndex, VkFormat bufferViewFormat);

	void DestroyBufferView(EntryHandle bufferViewIndex);

	void EndFrame();

	void AddPipelineToMainQueue(int psoIndex, int computeorgraphics);

	void ReadData(int handle, void* dest, int size, int offset);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle swapChainIndex;
	EntryHandle stagingBufferIndex;
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

	std::array<std::vector<EntryHandle>, 25> pipelinesIdentifier{};
	std::array<EntryHandle, 60> vulkanDescriptorLayouts{};
	std::array<ShaderDetails*, 70> shaderDetails{};
	char* shaderDetailsData;
	std::atomic<int> shaderDetailAlloc = 0;

	RenderAllocationHolder<50> allocations{};

	ArrayAllocator<EntryHandle, 50> renderStateObjects{};

	ArrayAllocator<EntryHandle, 50> computeStateObjects{};

	ArrayAllocator<int, 50> graphIndices{};

	enum
	{
		COMPUTESO,
		GRAPHICSO,
		INDIRECTSO,
	};

	struct PipelineHandle
	{
		int group;
		int indexForHandles;
		int numHandles;
		int graphIndex;
		int graphCount;
	};

	ArrayAllocator<PipelineHandle, 100> stateObjectHandles{};

	int width = 0; 
	int height = 0;

	int minUniformAlignment;
	int minStorageAlignment; 

};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}