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



struct ThreadedRecordBuffer
{
	std::array<uint32_t, 3> buffers; //indices
	uint32_t currentBuffer = 2; // current good buffer
	Semaphore currentGuard{};
	SPSC invalidate{};
	uint32_t outputImageIndex;
	std::function<void(uint32_t, uint32_t)> drawingFunction;

	uint32_t GetCurrentBuffer()
	{
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

	void DrawLoop()
	{
		std::chrono::nanoseconds timeout = std::chrono::nanoseconds(1);

		if (!invalidate.Wait(timeout)) return;
		
		uint32_t next = (currentBuffer + 1) % 3;

		uint32_t recordBuffer = buffers[next];

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

	void BeginCommandBufferRecording(uint32_t cb, uint32_t imageIndex);

	void EndCommandBufferRecording(uint32_t cb);

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

	uint32_t GetMainBufferIndex() const;

	VkBuffer GetDynamicUniformBuffer();

	ImageIndex CreateVulkanImage(
		std::vector<std::vector<char>>& imageData,
		std::vector<uint32_t>& imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type);

	ImageIndex CreateVulkanImageView(ImageIndex& imageIndex, uint32_t mipLevels,
		ImageFormat type, VkImageAspectFlags aspectMask);

	ImageIndex CreateVulkanSampler(uint32_t mipLevels);

	void DeleteVulkanImageView(ImageIndex& index);

	void DeleteVulkanImage(ImageIndex& index);

	void DeleteVulkanSampler(ImageIndex& index);

	void CreateVulkanRenderer(WindowManager* window);

	VkImageView GetImageView(uint32_t viewIndex);

	VkSampler GetSampler(uint32_t index);

	void SetResizeBool(bool set);

	DescriptorSetBuilder CreateDescriptorSet(std::string layoutname, uint32_t frames);
	
	uint32_t GetCurrentFrame() const;

	VkSampleCountFlagBits GetMSAASamples() const;

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	void CreateVulkanPipelineObject(VKPipelineObject* pipeline);

	OffsetIndex CreateRenderGraph(size_t datasize, size_t alignment);

	void DrawScene(uint32_t cbindex);

	void InvalidateRecordBuffer(uint32_t i);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void MonolithicDrawingTask(uint32_t commandBufferIndex, uint32_t imageIndex);

	VKInstance vkInstance;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	uint32_t descriptorPoolIndex;
	uint32_t swapChainIndex;
	uint32_t attachmentsIndex;
	ImageIndex depthView, depthImage;
	ImageIndex colorView, colorImage;
	BufferIndex stagingBufferIndex;
	BufferIndex globalIndex;
	uint32_t mainRenderPass;
	std::array<uint32_t, 3> currentCBIndex;
	std::vector<uint32_t> cbsIndices;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat;

	std::array<ThreadedRecordBuffer, MAX_FRAMES_IN_FLIGHT> threadedRecordBuffers;
	
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}