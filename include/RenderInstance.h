#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "ResourceDependencies.h"
#include "ThreadManager.h"
#include "WindowManager.h"


namespace API {

	VkCompareOp ConvertDepthTestAppToVulkan(DepthTest testApp);

	VkFormat ConvertSMBToVkFormat(ImageFormat format);

	VkPrimitiveTopology ConvertTopology(PrimitiveType type);
}

template <int N>
struct ThreadedRecordBuffer
{
	std::array<EntryHandle, N> buffers; //indices
	uint32_t currentBuffer = N-1; // current good buffer
	Semaphore currentGuard{};
	SPSC invalidate{};
	uint32_t outputImageIndex;
	std::function<void(EntryHandle, uint32_t)> drawingFunction;

	std::atomic<bool> ready = false;
	
	EntryHandle GetCurrentBuffer()
	{
		if (!ready.load()) return EntryHandle();
		currentGuard.Wait();
		return buffers[currentBuffer];
	}

	void ReleaseCurrentCommandBuffer()
	{
		currentGuard.Notify();
	}

	void Invalidate()
	{
		invalidate.Notify();
	}

	void Reset()
	{
		ready.store(false);
		invalidate.Notify();
	}

	void DrawLoop(std::stop_token stoken)
	{
		while (true)
		{
			invalidate.Wait(stoken);

			if (stoken.stop_requested()) {
				break;
			}

			DrawMain(false);

			if (!ready.load())
				ready.store(true);
		}
	}


	void DrawMain(bool wait = true)
	{
		if (wait) {
			invalidate.Wait();
		}

	
		uint32_t next = (currentBuffer + 1) % N;

		EntryHandle recordBuffer = buffers[next];

		//do stuff

		drawingFunction(recordBuffer, outputImageIndex);

		{
			SemaphoreGuard guard(currentGuard);
			currentBuffer = next;
		}

		
	}
};


struct GraphicsIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferIndex;
	uint32_t vertexCount;
	size_t indirectDrawBuffer;
	uint32_t pipelinename;
	int descriptorsetid;
	uint32_t maxDynCap;
	int indexBufferHandle;
	uint32_t indexCount;
	uint32_t pushRangeCount;
};

struct ComputeIntermediaryPipelineInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t maxDynCap;
	uint32_t pipelinename;
	int descriptorsetid;
	uint32_t barrierCount;
	uint32_t pushRangeCount;
};


#define FULL_ALLOCATION_SIZE 0
#define ABSOLUTE_ALLOCATION_OFFSET 0

struct RenderAllocation
{
	EntryHandle memIndex;
	size_t offset;
	size_t size;
};

template <int N>
struct RenderAllocationHolder
{
	std::array<RenderAllocation, N> allocations;
	
	RenderAllocation operator[](size_t index)
	{
		if (index >= N)
			return { EntryHandle(), ~0ui64, ~0ui64 };

		return allocations[index];
	}

	RenderAllocation operator[](int index)
	{
		if (index >= N)
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

struct DescriptorSet;
struct DescriptorHeader;

class RenderInstance
{
public:

	RenderInstance();

	~RenderInstance();

	void CreateDepthImage( uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void DestroySwapChainAttachments();

	void RecreateSwapChain();

	void CreateRenderPass(uint32_t index, VkSampleCountFlagBits sampleCount);

	void BeginCommandBufferRecording(EntryHandle cb, uint32_t imageIndex);

	void EndCommandBufferRecording(EntryHandle cb);

	void UsePipelineBuilders(VKGraphicsPipelineBuilder* generic, VKGraphicsPipelineBuilder* text, VkSampleCountFlagBits sampleCount);

	uint32_t BeginFrame();

	void SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void WaitOnRender();

	void CreatePipelines();

	void CreateSwapChain(uint32_t width, uint32_t height, bool recreate);

	void CreateGlobalBuffer();

	void UpdateAllocation(void* data, size_t handle, size_t size, size_t offset);

	int GetPageFromUniformBuffer(size_t size, uint32_t alignment);

	int GetPageFromDeviceBuffer(size_t size, uint32_t alignment);

	EntryHandle CreateImage(
		char* imageData,
		uint32_t* imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type);

	void CreateVulkanRenderer(WindowManager* window);

	void SetResizeBool(bool set);
	
	uint32_t GetCurrentFrame() const;

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	EntryHandle CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info, int* offsets);

	EntryHandle CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info, int* offsets);

	size_t CreateRenderGraph(size_t datasize, size_t alignment);

	void DrawScene(EntryHandle cbindex, uint32_t imageIndex);

	void InvalidateRecordBuffer(uint32_t i);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void MonolithicDrawingTask(EntryHandle commandBufferIndex, uint32_t imageIndex);

	void DestoryTexture(EntryHandle handle);

	void AllocateVectorsForMSAA();

	void IncreaseMSAA();

	void DecreaseMSAA();

	void CreateShaderResourceMap(ShaderGraph *graph);

	uintptr_t AllocateShaderGraph(uint32_t shaderMapCount, uint32_t* shaderResourceCount, ShaderStageType* types, uint32_t* shaderReferences);

	int AllocateDescriptorSet(uint32_t shaderGraphIndex, uint32_t targetSet, int index, int setCount);

	void BindBufferToDescriptor(int descriptorSet, int allocationIndex, bool direct, int bindingIndex);

	void BindSampledImageToDescriptor(int descriptorSet, EntryHandle index, int bindingIndex);

	void BindBarrier(int descriptorSet, int binding, BarrierStage stage, BarrierAction action);

	void BindImageBarrier(int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, VkImageLayout oldLayout, VkImageLayout dstLayout, bool location);

	void UploadConstant(int descriptorset, void* data, int bufferLocation);

	EntryHandle CreateDescriptorSet(int descriptorSet);

	DescriptorHeader* PopDescriptorBarrier(int descriptorSet, int* counter);

	DescriptorHeader* GetConstantBuffer(int descriptorSet, int constantBuffer);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle swapChainIndex;
	EntryHandle descriptorPoolIndex;
	EntryHandle attachmentsIndex;
	EntryHandle stagingBufferIndex;
	EntryHandle globalIndex, globalDeviceBufIndex;
	EntryHandle computeGraphIndex;
	std::array<EntryHandle, 3> currentCBIndex;

	uint32_t currentMSAALevel = 0;
	uint32_t maxMSAALevels = 0;

	std::vector<EntryHandle> swapchainRenderTargets{};
	std::vector<EntryHandle> depthViews{};
	std::vector<EntryHandle> colorViews{};
	std::vector<EntryHandle> depthImages{};
	std::vector<EntryHandle> colorImages{};
	std::vector<EntryHandle> renderPasses{};

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkFormat depthFormat = VK_FORMAT_UNDEFINED;

	std::array<ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>, MAX_FRAMES_IN_FLIGHT> threadedRecordBuffers;

	std::array<EntryHandle, 6> shaders;
	
	uintptr_t shaderGraphs;
	uint32_t shaderGraphOffset;

	uintptr_t descriptorResourceSet;
	uint32_t descriptorResourceOffset;

	std::array<std::vector<EntryHandle>, 4> pipelinesIdentifier;
	std::array<EntryHandle, 5> descriptorLayouts;
	std::array<ShaderGraph*, 4> shaderGraphPtrs;
	std::array<uintptr_t, 50> descriptorSets;
	std::atomic<int> descriptorSetIndex;

	RenderAllocationHolder<50> allocations;
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}