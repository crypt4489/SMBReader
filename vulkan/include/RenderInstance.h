#pragma once


#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "IndexTypes.h"
#include "VKInstance.h"
#include "VKDescriptorSetCache.h"
#include "VKPipelineCache.h"
#include "WindowManager.h"

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

	void CreateSyncObjects();

	uint32_t BeginFrame();

	void SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height);

	void WaitOnQueues();

	void CreatePipelines();

	void CreateGlobalBuffer();

	void UpdateDynamicGlobalBuffer(void* data, size_t dataSize, size_t offset, uint32_t frame);

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

	RecordingBufferObject GetCurrentCommandBuffer();

	DescriptorSetBuilder CreateDescriptorSet(std::string layoutname, uint32_t frames);
	
	uint32_t GetCurrentFrame() const;

	VkSampleCountFlagBits GetMSAASamples() const;

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;


	VKInstance vkInstance;
	QueueIndex graphicsIndex, presentIndex, presentMax, graphicsMax;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	uint32_t transferIndex, graphicsPresentIndex;
	uint32_t descriptorPoolIndex;
	uint32_t swapChainIndex;
	uint32_t attachmentsIndex;
	ImageIndex depthView, depthImage;
	ImageIndex colorView, colorImage;
	BufferIndex stagingBufferIndex;
	BufferIndex globalIndex;
	uint32_t mainRenderTarget;
	uint32_t currentCBIndex;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat;
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}