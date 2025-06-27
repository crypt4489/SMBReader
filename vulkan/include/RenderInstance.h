#pragma once


#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

#include "FileManager.h"
#include "ThreadManager.h"
#include "VKInstance.h"
#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKSwapChain.h"
#include "VKShaderCache.h"
#include "VKPipelineCache.h"
#include "WindowManager.h"





class RenderInstance
{
public:

	RenderInstance();

	void CreateDepthImage(uint32_t width, uint32_t height);

	void DestroySwapChainAttachments();

	void RecreateSwapChain();

	std::pair<VkShaderModule, VkShaderStageFlagBits> GetShaderFromCache(const std::string& name);

	void CreateRenderPass();

	void BeginCommandBufferRecording(VkCommandBuffer cb, uint32_t imageIndex);

	void EndCommandBufferRecording(VkCommandBuffer cb);

	void CreateSyncObjects();

	uint32_t BeginFrame();

	void SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height);

	void WaitOnQueues();

	void CreatePipelines();

	PipelineCacheObject GetPipeline(std::string ptype);

	void CreateGlobalBuffer();

	void UpdateDynamicGlobalBuffer(void* data, size_t dataSize, size_t offset, uint32_t frame);

	uint32_t GetPageFromUniformBuffer(size_t size, size_t alignment);

	uint32_t GetMainBufferIndex() const;

	VkBuffer GetDynamicUniformBuffer();

	uint32_t CreateVulkanImage(
		std::vector<std::vector<char>>& imageData,
		std::vector<uint32_t>& imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type);

	uint32_t CreateVulkanImageView(uint32_t imageIndex, uint32_t mipLevels,
		ImageFormat type, VkImageAspectFlags aspectMask);

	uint32_t CreateVulkanSampler(uint32_t mipLevels);

	void DeleteVulkanImageView(uint32_t index);

	void DeleteVulkanImage(uint32_t index);

	void DeleteVulkanSampler(uint32_t index);

	void CreateVulkanRenderer(WindowManager* window);

	VkImageView GetImageView(uint32_t viewIndex);

	VkSampler GetSampler(uint32_t index);
	
	void DestroyVulkanRenderer();

	void DestroyRenderInstance();

	void SetResizeBool(bool set);

	VkDevice GetVulkanDevice();

	VkPhysicalDevice GetVulkanPhysicalDevice();

	VkCommandPool GetVulkanGraphincsCommandPool();

	VkQueue GetGraphicsQueue();

	VkCommandPool GetVulkanTransferCommandPool();

	auto GetTransferQueue();

	void ReturnTranferQueue(int32_t i);

	VkRenderPass GetRenderPass();

	VkDescriptorPool GetDescriptorPool();

	VkCommandBuffer GetCurrentCommandBuffer();
	
	uint32_t GetCurrentFrame() const;

	VkSampleCountFlagBits GetMSAASamples() const;

	Semaphore& GetTransferSemaphore();

	VKPipelineCache* GetMainRenderPassCache();

	VKDescriptorLayoutCache* GetDescriptorLayoutCache();
	 
	VKDescriptorSetCache* GetDescriptorSetCache();

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

private:

	VKInstance vkInstance;
	uint32_t graphicsIndex, presentIndex, presentMax, graphicsMax;
	uint32_t deviceIndex;
	uint32_t physicalIndex;
	uint32_t transferIndex, graphicsPresentIndex;
	uint32_t descriptorPoolIndex;
	uint32_t swapChainIndex;
	uint32_t attachmentsIndex;
	uint32_t depthView, depthImage;
	uint32_t colorView, colorImage;
	uint32_t stagingBufferIndex;
	uint32_t globalIndex;
	uint32_t dynamicIndex;
	uint32_t currentCBIndex;

	QueueManager* gptManager;

	Semaphore transferSemaphore; // semaphore for transfer command pool

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat;

	VKShaderCache shaderCache;
	VKPipelineCache mainRenderPassCache;
	VKDescriptorLayoutCache descriptorLayoutCache;
	VKDescriptorSetCache descriptorSetCache;
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}