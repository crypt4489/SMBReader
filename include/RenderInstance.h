#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
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
	uint32_t instanceCount;
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

	void IncreaseMSAA();

	void DecreaseMSAA();

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	ShaderResourceHeader* PopShaderResourceBarrier(int descriptorSet, int* counter);

	void AddVulkanMemoryBarrier(VKPipelineObject* vkPipelineObject, int descriptorid);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle swapChainIndex;
	EntryHandle attachmentsIndex;
	EntryHandle stagingBufferIndex;
	EntryHandle globalIndex, globalDeviceBufIndex;
	EntryHandle computeGraphIndex;
	std::array<EntryHandle, 3> currentCBIndex;

	uint32_t currentMSAALevel = 0;
	uint32_t maxMSAALevels = 0;

	std::array<EntryHandle, 5> swapchainRenderTargets{};
	std::array<EntryHandle, 5> depthViews{};
	std::array<EntryHandle, 5> colorViews{};
	std::array<EntryHandle, 5> depthImages{};
	std::array<EntryHandle, 5> colorImages{};
	std::array<EntryHandle, 5> renderPasses{};

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	ImageFormat depthFormat = ImageFormat::IMAGE_UNKNOWN;

	std::array<ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>, MAX_FRAMES_IN_FLIGHT> threadedRecordBuffers;

	ShaderGraphsHolder<4, 6> vulkanShaderGraphs;
	
	ShaderResourceManager<50> descriptorManager;

	std::array<std::vector<EntryHandle>, 4> pipelinesIdentifier;
	std::array<EntryHandle, 5> vulkanDescriptorLayouts;
	std::array<ShaderDetails*, 6> shaderDetails;
	std::array<char, 2 * KB> shaderDetailsData;
	int shaderDetailAlloc = 0;

	RenderAllocationHolder<50> allocations;
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}