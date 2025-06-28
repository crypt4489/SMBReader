#include "RenderInstance.h"



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

namespace VKRenderer {
	RenderInstance* gRenderInstance = nullptr;
}

static void frameResizeCB(GLFWwindow* window, int width, int height)
{
	auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
	renderInst->SetResizeBool(true);
}

RenderInstance::RenderInstance() : transferSemaphore(Semaphore(1))
{

};

void RenderInstance::CreateDepthImage(uint32_t width, uint32_t height)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	depthImage = dev.CreateImage(width, height,
		1, depthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		msaaSamples,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

	depthView = dev.CreateImageView(depthImage, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	dev.TransitionImageLayout(graphicsPresentIndex, graphicsIndex,
		depthImage, depthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);
}

void RenderInstance::DestroySwapChainAttachments()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	dev.DestroyImage(depthImage);
	dev.DestroyImageView(depthView);
	dev.DestroyImage(colorImage);
	dev.DestroyImageView(colorView);
}

void RenderInstance::RecreateSwapChain() {
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VkDevice logicalDevice = dev.device;
	VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
	int width = 0, height = 0;
	glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(logicalDevice);

	DestroySwapChainAttachments();
	swapChain.DestroySwapChain();

	CreateMSAAColorResources(width, height);
	CreateDepthImage(width, height);

	swapChain.RecreateSwapChain(width, height);
}

std::pair<VkShaderModule, VkShaderStageFlagBits> RenderInstance::GetShaderFromCache(const std::string& name)
{
	return shaderCache.GetShader(name);
}


void RenderInstance::CreateRenderPass()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);

	VKRenderPassBuilder rpb{};
	VkFormat format = swapChain.GetSwapChainFormat();

	rpb.CreateAttachment(VKRenderPassBuilder::COLORATTACH, format, msaaSamples,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);


	rpb.CreateAttachment(VKRenderPassBuilder::COLORRESOLVEATTACH, format, VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE
		, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 2);

	rpb.CreateAttachment(VKRenderPassBuilder::DEPTHSTENCILATTACH, depthFormat, msaaSamples,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

	rpb.CreateSubPassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, 0, 0);

	rpb.CreateSubPassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

	rpb.CreateInfo();

	dev.CreateRenderPasses(rpb);
}

void RenderInstance::BeginCommandBufferRecording(VkCommandBuffer cb, uint32_t imageIndex)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = dev.GetRenderPass(swapChain.renderPassIndex);
	renderPassInfo.framebuffer = dev.GetFrameBuffer(swapChain.swapChainFramebuffers[imageIndex]);
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChain.swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChain.GetSwapChainWidth());
	viewport.height = static_cast<float>(swapChain.GetSwapChainHeight());
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cb, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChain.swapChainExtent;
	vkCmdSetScissor(cb, 0, 1, &scissor);

}

void RenderInstance::EndCommandBufferRecording(VkCommandBuffer cb)
{
	vkCmdEndRenderPass(cb);

	if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void RenderInstance::CreateSyncObjects()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	std::vector<uint32_t> imageAvailablesIndices = dev.CreateSemaphores(MAX_FRAMES_IN_FLIGHT);
	std::vector<uint32_t> renderFinishedIndices = dev.CreateSemaphores(MAX_FRAMES_IN_FLIGHT);

	uint32_t size = static_cast<uint32_t>(imageAvailablesIndices.size());

	std::vector<uint32_t> indices(size * 2);

	auto iter = indices.begin();

	iter = std::copy(imageAvailablesIndices.begin(), imageAvailablesIndices.end(), iter);

	iter = std::copy(renderFinishedIndices.begin(), renderFinishedIndices.end(), iter);

	dev.CreateSwapChainsDependencies(swapChainIndex, indices, MAX_FRAMES_IN_FLIGHT, 2);

	dev.CreateCommandBuffers(graphicsPresentIndex, MAX_FRAMES_IN_FLIGHT * 3, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

}

uint32_t RenderInstance::BeginFrame()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	currentCBIndex = dev.RequestCommandBuffer(UINT64_MAX, currentFrame);

	uint32_t imageIndex = dev.BeginFrameForSwapchain(swapChainIndex, currentFrame);

	if (imageIndex == UINT32_MAX)
	{
		RecreateSwapChain();
		return imageIndex;
	}

	BeginCommandBufferRecording(dev.GetCommandBufferHandle(currentCBIndex), imageIndex);

	return imageIndex;
}

void RenderInstance::SubmitFrame(uint32_t imageIndex)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	EndCommandBufferRecording(dev.GetCommandBufferHandle(currentCBIndex));

	uint32_t res = dev.SubmitCommandsForSwapChain(swapChainIndex, currentFrame, graphicsIndex, 0, currentCBIndex);

	res = dev.PresetSwapChain(swapChainIndex, currentFrame, imageIndex, presentIndex, 0);

	if (!res || resizeWindow) {
		resizeWindow = false;
		RecreateSwapChain();
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}



void RenderInstance::CreateMSAAColorResources(uint32_t width, uint32_t height) {

	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VkDevice logicalDevice = dev.device;
	VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);

	colorImage = dev.CreateImage(width, height,
		1, swapChain.GetSwapChainFormat(), 1,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		msaaSamples,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

	colorView = dev.CreateImageView(colorImage, 1, swapChain.GetSwapChainFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
}

void RenderInstance::WaitOnQueues()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	vkQueueWaitIdle(dev.GetQueueHandle(graphicsIndex, 0));
	vkQueueWaitIdle(dev.GetQueueHandle(presentIndex, 0));
}

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VkDevice logicalDevice = dev.device;

	auto shader1 = shaderCache.GetShader("3dtextured.vert.spv");
	auto shader2 = shaderCache.GetShader("3dtextured.frag.spv");

	std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders = { shader1, shader2 };

	auto shader3 = shaderCache.GetShader("text.vert.spv");
	auto shader4 = shaderCache.GetShader("text.frag.spv");

	std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders2 = { shader3, shader4 };

	DescriptorSetLayoutBuilder textDescriptor{}, globalBufferBuilder{}, genericObjectBuilder{};
	std::vector<VkDescriptorSetLayout> textDescriptorContainers(1);
	std::vector<VkDescriptorSetLayout> regularMeshConatiners(2);

	textDescriptor.AddPixelImageSamplerLayout(0);

	textDescriptorContainers[0] = textDescriptor.CreateDescriptorSetLayout(logicalDevice);

	descriptorLayoutCache.AddLayout("oneimage", textDescriptorContainers[0]);

	globalBufferBuilder.AddBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	regularMeshConatiners[0] = globalBufferBuilder.CreateDescriptorSetLayout(logicalDevice);

	descriptorLayoutCache.AddLayout("mainrenderpass", regularMeshConatiners[0]);

	genericObjectBuilder.AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	genericObjectBuilder.AddPixelImageSamplerLayout(1);

	regularMeshConatiners[1] = genericObjectBuilder.CreateDescriptorSetLayout(logicalDevice);

	descriptorLayoutCache.AddLayout("genericobject", regularMeshConatiners[1]);

	mainRenderPassCache.CreatePipeline(regularMeshConatiners, std::nullopt, std::nullopt, shaders, VK_COMPARE_OP_LESS, GetMSAASamples(), "genericpipeline");

	mainRenderPassCache.CreatePipeline(textDescriptorContainers, TextVertex::getBindingDescription(), TextVertex::getAttributeDescriptions(), shaders2, VK_COMPARE_OP_ALWAYS, GetMSAASamples(), "text");
}

PipelineCacheObject RenderInstance::GetPipeline(std::string ptype)
{
	return mainRenderPassCache.GetPipelineFromCache(ptype);
}

void RenderInstance::CreateGlobalBuffer()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	globalIndex = dev.CreateHostBuffer
	(
		128'000'000, true, true,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	);
}

void RenderInstance::UpdateDynamicGlobalBuffer(void* data, size_t dataSize, size_t offset, uint32_t frame)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	dev.WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * frame));
}


uint32_t RenderInstance::GetPageFromUniformBuffer(size_t size, size_t alignment)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetOffsetIntoHostBuffer(globalIndex, size, alignment);
}

uint32_t RenderInstance::GetMainBufferIndex() const
{
	return globalIndex;
}

VkBuffer RenderInstance::GetDynamicUniformBuffer()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).hostBuffers[globalIndex].first;
}

ImageIndex RenderInstance::CreateVulkanImage(
	std::vector<std::vector<char>>& imageData,
	std::vector<uint32_t>& imageSizes,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice.CreateSampledImage(
		imageData, imageSizes,
		width, height,
		mipLevels, VK::API::ConvertSMBToVkFormat(type),
		1,
		graphicsIndex, transferIndex,
		attachmentsIndex, stagingBufferIndex);
}

ImageIndex RenderInstance::CreateVulkanImageView(ImageIndex& imageIndex, uint32_t mipLevels,
	ImageFormat type, VkImageAspectFlags aspectMask)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice.CreateImageView(imageIndex, mipLevels, VK::API::ConvertSMBToVkFormat(type), aspectMask);
}

ImageIndex RenderInstance::CreateVulkanSampler(uint32_t mipLevels)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice.CreateSampler(mipLevels);
}

void RenderInstance::DeleteVulkanImageView(ImageIndex& index)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice.DestroyImage(index);
}

void RenderInstance::DeleteVulkanImage(ImageIndex& index)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice.DestroyImage(index);
}

void RenderInstance::DeleteVulkanSampler(ImageIndex& index)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice.DestorySampler(index);
}

void RenderInstance::CreateVulkanRenderer(WindowManager* window)
{
	this->windowMan = window;
	windowMan->SetWindowResizeCallback(frameResizeCB);
	glfwSetWindowUserPointer(windowMan->GetWindow(), this);


	vkInstance.CreateRenderInstance();
	vkInstance.CreateDrawingSurface(window->GetWindow());

	physicalIndex = vkInstance.CreatePhysicalDevice();
	VKDevice& majorDevice = vkInstance.CreateLogicalDevice(physicalIndex, deviceIndex);

	msaaSamples = vkInstance.GetMaxMSAALevels(physicalIndex);

	std::vector<VkQueueFamilyProperties> famProps;
	majorDevice.QueueFamilyDetails(famProps);

	uint32_t graphicsIndex, presentIndex, presentMax, graphicsMax;
	majorDevice.GetPresentQueue(presentIndex, presentMax, famProps, vkInstance.renderSurface);
	majorDevice.GetQueueByMask(graphicsIndex, graphicsMax, famProps, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);

	std::set queueFamilies = { graphicsIndex, presentIndex };
	std::vector queueCounts = { graphicsMax, presentMax };

	VkPhysicalDeviceFeatures features{};
	features.geometryShader = VK_TRUE;
	features.textureCompressionBC = VK_TRUE;
	features.tessellationShader = VK_TRUE;
	features.samplerAnisotropy = VK_TRUE;
	features.multiDrawIndirect = VK_TRUE;

	graphicsPresentIndex = 0;
	transferIndex = 1;

	majorDevice.CreateLogicalDevice(vkInstance.instanceLayers, vkInstance.deviceExtensions, queueFamilies, queueCounts, features);

	majorDevice.CreateCommandPool(graphicsIndex, graphicsPresentIndex);
	majorDevice.CreateCommandPool(graphicsIndex, transferIndex);

	gptManager = majorDevice.CreateQueueManager(graphicsIndex, transferIndex, graphicsMax);

	depthFormat = VK::Utils::findSupportedFormat(majorDevice.gpu,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice.CreateHostBuffer(64'000'000, true, false, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	swapChainIndex = majorDevice.CreateSwapChain(vkInstance.renderSurface, 3);

	VKSwapChain& swapChain = majorDevice.GetSwapChain(swapChainIndex);

	auto scd = vkInstance.GetSwapChainSupport(physicalIndex);

	swapChain.SetSwapChainProperties(scd);

	VkFormat swcFormat = swapChain.GetSwapChainFormat();

	auto swcPool = majorDevice.FindImageMemoryIndexForPool(1920, 1200,
		1, swcFormat, 1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	auto depthPool = majorDevice.FindImageMemoryIndexForPool(1920, 1200,
		1, depthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	attachmentsIndex = majorDevice.CreateImageMemoryPool(512'000'000, depthPool.first);

	CreateMSAAColorResources(800, 600);

	CreateDepthImage(800, 600);

	CreateRenderPass();

	std::vector attachmentViews = { &colorView, &depthView };

	uint32_t queuefamilies[] = { graphicsIndex };

	swapChain.CreateSwapChain(800, 600, queuefamilies, 1, 0, attachmentViews);

	DescriptorPoolBuilder builder{};
	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
	majorDevice.CreateDesciptorPool(descriptorPoolIndex, builder, MAX_FRAMES_IN_FLIGHT * 100);

	CreateSyncObjects();

	descriptorLayoutCache.device = majorDevice.device;
	mainRenderPassCache.renderPass = majorDevice.GetRenderPass(0);
	mainRenderPassCache.device = majorDevice.device;
	shaderCache.SetLogicalDevice(majorDevice.device);

	CreateGlobalBuffer();

	CreatePipelines();
}

VkImageView RenderInstance::GetImageView(uint32_t viewIndex)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice.imageViews[viewIndex];
}

VkSampler RenderInstance::GetSampler(uint32_t index)
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).samplers[index];
}

void RenderInstance::DestroyVulkanRenderer()
{
	DestroyRenderInstance();
}

void RenderInstance::DestroyRenderInstance()
{
	shaderCache.DestroyShaderCache();

	mainRenderPassCache.DestroyPipelineCache();

	descriptorLayoutCache.DestroyLayoutCache();
}

void RenderInstance::SetResizeBool(bool set)
{
	resizeWindow = set;
}

VkDevice RenderInstance::GetVulkanDevice()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.device;
}

VkPhysicalDevice RenderInstance::GetVulkanPhysicalDevice()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.gpu;
}

VkCommandPool RenderInstance::GetVulkanGraphincsCommandPool()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetCommandPool(graphicsPresentIndex);
}

VkQueue RenderInstance::GetGraphicsQueue()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetQueueHandle(graphicsIndex, 0);
}

VkCommandPool RenderInstance::GetVulkanTransferCommandPool()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetCommandPool(transferIndex);
}

auto RenderInstance::GetTransferQueue()
{
	return gptManager->GetQueue();
}

void RenderInstance::ReturnTranferQueue(int32_t i)
{
	gptManager->ReturnQueue(i);
}

VkRenderPass RenderInstance::GetRenderPass()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetRenderPass(0);
}

VkDescriptorPool RenderInstance::GetDescriptorPool()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetDescriptorPool(descriptorPoolIndex);
}

VkCommandBuffer RenderInstance::GetCurrentCommandBuffer()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.GetCommandBufferHandle(currentCBIndex);
}

uint32_t RenderInstance::GetCurrentFrame() const
{
	return currentFrame;
}

VkSampleCountFlagBits RenderInstance::GetMSAASamples() const
{
	return msaaSamples;
}

Semaphore& RenderInstance::GetTransferSemaphore()
{
	return transferSemaphore;
}

VKPipelineCache* RenderInstance::GetMainRenderPassCache()
{
	return &mainRenderPassCache;
}

VKDescriptorLayoutCache* RenderInstance::GetDescriptorLayoutCache()
{
	return &descriptorLayoutCache;
}

VKDescriptorSetCache* RenderInstance::GetDescriptorSetCache()
{
	return &descriptorSetCache;
}

uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainWidth();
}