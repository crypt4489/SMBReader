#pragma once

#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "IndexTypes.h"
#include "VKInstance.h"
#include "VKDescriptorSetCache.h"
#include "VKRenderGraph.h"
#include "VKPipelineCache.h"
#include "WindowManager.h"


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

			if (stoken.stop_requested()) break;

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

class RenderInstance
{
public:

	RenderInstance();

	void CreateDepthImage(uint32_t width, uint32_t height);

	void DestroySwapChainAttachments();

	void RecreateSwapChain();

	void CreateRenderPass();

	void BeginCommandBufferRecording(EntryHandle cb, uint32_t imageIndex);

	void EndCommandBufferRecording(EntryHandle cb);

	uint32_t BeginFrame();

	void SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height);

	void WaitOnRender();

	void CreatePipelines();

	void CreateGlobalBuffer();

	void UpdateDynamicGlobalBufferAbsolute(void* data, size_t dataSize, size_t offset);

	void UpdateDynamicGlobalBufferCurrent(void* data, size_t dataSize, size_t offset);

	void UpdateDynamicGlobalBufferForAllFrames(void* data, size_t dataSize, size_t offset);

	OffsetIndex GetPageFromUniformBuffer(size_t size, uint32_t alignment);

	EntryHandle GetMainBufferIndex() const;

	VkBuffer GetDynamicUniformBuffer();

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

	DescriptorSetBuilder CreateDescriptorSet(std::string layoutname, uint32_t frames);
	
	uint32_t GetCurrentFrame() const;

	VkSampleCountFlagBits GetMSAASamples() const;

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	void CreateVulkanPipelineObject(VKPipelineObject* pipeline);

	OffsetIndex CreateRenderGraph(size_t datasize, size_t alignment);

	void DrawScene(EntryHandle cbindex);

	void InvalidateRecordBuffer(uint32_t i);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void MonolithicDrawingTask(EntryHandle commandBufferIndex, uint32_t imageIndex);

	VKInstance vkInstance;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle descriptorPoolIndex;
	EntryHandle swapChainIndex;
	EntryHandle attachmentsIndex;
	EntryHandle depthView, depthImage;
	EntryHandle colorView, colorImage;
	EntryHandle stagingBufferIndex;
	EntryHandle globalIndex;
	EntryHandle mainRenderPass;
	std::array<EntryHandle, 3> currentCBIndex;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat;

	std::array<ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>, MAX_FRAMES_IN_FLIGHT> threadedRecordBuffers;
	
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}