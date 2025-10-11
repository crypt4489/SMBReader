#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "IndexTypes.h"
#include "VKTypes.h"
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
	size_t vertexBufferIndex;
	uint32_t vertexCount;
	size_t indirectDrawBuffer;
	EntryHandle pipelinename;
	EntryHandle descriptorsetid;
	uint32_t maxDynCap;
	size_t indexBufferHandle;
	uint32_t indexCount;
	uint32_t pushRangeCount;
};

struct ComputeIntermediaryPipelineInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t maxDynCap;
	EntryHandle pipelinename;
	EntryHandle descriptorsetid;
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

};

class RenderInstance
{
public:

	RenderInstance();

	~RenderInstance();

	void CreateDepthImage(uint32_t width, uint32_t height);

	void DestroySwapChainAttachments();

	void RecreateSwapChain();

	void CreateRenderPass();

	void BeginCommandBufferRecording(EntryHandle cb, uint32_t imageIndex);

	void EndCommandBufferRecording(EntryHandle cb);

	void UsePipelineBuilders(VKGraphicsPipelineBuilder* generic, VKGraphicsPipelineBuilder* text);

	uint32_t BeginFrame();

	void SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height);

	void WaitOnRender();

	void CreatePipelines();

	void CreateGlobalBuffer();

	void UpdateAllocation(void* data, size_t handle, size_t size, size_t offset);

	size_t GetPageFromUniformBuffer(size_t size, uint32_t alignment);

	size_t GetPageFromDeviceBuffer(size_t size, uint32_t alignment);

	VkBuffer GetDynamicUniformBuffer();

	VkBuffer GetDeviceBufferHandle();

	EntryHandle CreateVulkanImage(
		char* imageData,
		uint32_t* imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type);

	EntryHandle CreateVulkanImageView(EntryHandle& imageIndex, uint32_t mipLevels,
		ImageFormat type, VkImageAspectFlags aspectMask);

	EntryHandle CreateVulkanSampler(uint32_t mipLevels);

	void DeleteVulkanImageView(EntryHandle& index);

	void DeleteVulkanImage(EntryHandle& index);

	void DeleteVulkanSampler(EntryHandle& index);

	void CreateVulkanRenderer(WindowManager* window);

	VkImageView GetImageView(EntryHandle viewIndex);

	VkSampler GetSampler(EntryHandle index);

	void SetResizeBool(bool set);

	DescriptorSetBuilder* CreateDescriptorSet(EntryHandle layoutname, uint32_t frames);
	
	uint32_t GetCurrentFrame() const;

	VkSampleCountFlagBits GetMSAASamples() const;

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	EntryHandle CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info, size_t *offsets, std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>* pushArgs);

	EntryHandle CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info, size_t* offsets, std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>* pushArgs);

	size_t CreateRenderGraph(size_t datasize, size_t alignment);

	void DrawScene(EntryHandle cbindex, uint32_t imageIndex);

	void InvalidateRecordBuffer(uint32_t i);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void MonolithicDrawingTask(EntryHandle commandBufferIndex, uint32_t imageIndex);

	void DestoryTexture(EntryHandle handle);

	void CreateBufferMemBarrier(EntryHandle computHandle, size_t allocation, size_t size);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle descriptorPoolIndex;
	EntryHandle swapChainIndex;
	EntryHandle attachmentsIndex;
	EntryHandle depthView, depthImage;
	EntryHandle colorView, colorImage;
	EntryHandle stagingBufferIndex;
	EntryHandle globalIndex, globalDeviceBufIndex;
	EntryHandle mainRenderPass;
	EntryHandle mainRenderTarget;
	EntryHandle computeGraphIndex;
	std::array<EntryHandle, 3> currentCBIndex;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat = VK_FORMAT_UNDEFINED;

	std::array<ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>, MAX_FRAMES_IN_FLIGHT> threadedRecordBuffers;

	std::array<EntryHandle, 5> shaders;

	std::unordered_map<std::string, EntryHandle> pipelinesIdentifier;
	std::unordered_map<std::string, EntryHandle> descriptorLayouts;

	RenderAllocationHolder<50> allocations;
	std::atomic<size_t> allocationsIndex;

	
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}