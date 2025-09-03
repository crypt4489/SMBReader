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

RenderInstance::RenderInstance()
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

	dev.TransitionImageLayout(
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

	mainRenderPass = dev.CreateRenderPasses(rpb);
}

void RenderInstance::BeginCommandBufferRecording(uint32_t cb, uint32_t imageIndex)
{

	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);

	auto rbo = dev.GetRecordingBufferObject(cb);

	rbo.BeginRecordingCommand();
	
	rbo.BeginRenderPassCommand(swapChain.renderTargetIndex, imageIndex, { {0, 0}, swapChain.swapChainExtent });

	uint32_t x = swapChain.GetSwapChainWidth(), y = swapChain.GetSwapChainHeight();

	rbo.SetViewportCommand(0, 0, x, y, 0.0f, 1.0f);

	rbo.SetScissorCommand(0, 0, x, y);

}

void RenderInstance::MonolithicDrawingTask(uint32_t commandBufferIndex, uint32_t imageIndex)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	commandBufferIndex = dev.RequestWithPossibleBufferResetAndFenceReset(UINT64_MAX, commandBufferIndex, true, false);

	BeginCommandBufferRecording(commandBufferIndex, imageIndex);

	DrawScene(commandBufferIndex);

	EndCommandBufferRecording(commandBufferIndex);
}

void RenderInstance::EndCommandBufferRecording(uint32_t cb)
{

	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	auto rbo = dev.GetRecordingBufferObject(cb);

	rbo.EndRenderPassCommand();

	rbo.EndRecordingCommand();
}

uint32_t RenderInstance::BeginFrame()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	auto& trb = threadedRecordBuffers[currentFrame];

	uint32_t nextCbIndex = trb.GetCurrentBuffer();

	int32_t res;

	if (nextCbIndex != currentCBIndex[currentFrame])
		res = dev.WaitOnCommandBufferAndPossibleResetFence(UINT64_MAX, currentCBIndex[currentFrame], true); //wait on previous

	currentCBIndex[currentFrame] = nextCbIndex;

	res = dev.WaitOnCommandBufferAndPossibleResetFence(UINT64_MAX, currentCBIndex[currentFrame], true); // wait on current, could be updated queue

	uint32_t imageIndex = dev.BeginFrameForSwapchain(swapChainIndex, trb.outputImageIndex);

	if (imageIndex == ~0ui32)
	{
		RecreateSwapChain();
		return imageIndex;
	}

	return trb.outputImageIndex;
}

void RenderInstance::SubmitFrame(uint32_t imageIndex)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t res = dev.SubmitCommandsForSwapChain(swapChainIndex, imageIndex, currentCBIndex[currentFrame]);

	res = dev.PresentSwapChain(swapChainIndex, imageIndex, currentCBIndex[currentFrame]);

	if (!res || resizeWindow) {
		resizeWindow = false;
		RecreateSwapChain();
	}

	auto& trb = threadedRecordBuffers[currentFrame];

	trb.ReleaseCurrentCommandBuffer();

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

void RenderInstance::WaitOnRender()
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	dev.WaitOnDevice();
}

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

	std::vector<std::string> shaders1 = { "3dtextured.vert.spv", "3dtextured.frag.spv" };

	std::vector<std::string> shaders2 = { "text.vert.spv" , "text.frag.spv" };

	auto &mainRenderPassCache = dev.GetPipelineCache(mainRenderPass);

	DescriptorSetLayoutBuilder textDescriptor = dev.CreateDescriptorSetLayoutBuilder();
	DescriptorSetLayoutBuilder globalBufferBuilder = dev.CreateDescriptorSetLayoutBuilder(); 
	DescriptorSetLayoutBuilder genericObjectBuilder = dev.CreateDescriptorSetLayoutBuilder();
	std::vector<std::string> textDescriptorContainers = { "oneimage" };
	std::vector<std::string> regularMeshConatiners = { "mainrenderpass", "genericobject" };

	textDescriptor.AddPixelImageSamplerLayout(0);

	textDescriptor.CreateDescriptorSetLayout(textDescriptorContainers[0]);

	globalBufferBuilder.AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	globalBufferBuilder.CreateDescriptorSetLayout(regularMeshConatiners[0]);

	genericObjectBuilder.AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	genericObjectBuilder.AddPixelImageSamplerLayout(1);

	genericObjectBuilder.CreateDescriptorSetLayout(regularMeshConatiners[1]);

	mainRenderPassCache.CreatePipeline(regularMeshConatiners, std::nullopt, std::nullopt, shaders1, VK_COMPARE_OP_LESS, GetMSAASamples(), "genericpipeline");

	mainRenderPassCache.CreatePipeline(textDescriptorContainers, TextVertex::getBindingDescription(), TextVertex::getAttributeDescriptions(), shaders2, VK_COMPARE_OP_ALWAYS, GetMSAASamples(), "text");
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

void RenderInstance::UpdateDynamicGlobalBufferCurrent(void* data, size_t dataSize, size_t offset)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	dev.WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * this->currentFrame));
}

void RenderInstance::UpdateDynamicGlobalBufferAbsolute(void* data, size_t dataSize, size_t offset)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	dev.WriteToHostBuffer(globalIndex, data, dataSize, offset);
}

void RenderInstance::UpdateDynamicGlobalBufferForAllFrames(void* data, size_t dataSize, size_t offset)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	for (int i = 0; i<MAX_FRAMES_IN_FLIGHT; i++)
		dev.WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * i));
}


OffsetIndex RenderInstance::GetPageFromUniformBuffer(size_t size, uint32_t alignment)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return std::move(dev.GetOffsetIntoHostBuffer(globalIndex, size, alignment));
}

uint32_t RenderInstance::GetMainBufferIndex() const
{
	return globalIndex;
}

VkBuffer RenderInstance::GetDynamicUniformBuffer()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetHostBuffer(globalIndex);
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
		attachmentsIndex, 
		stagingBufferIndex);
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


#define MB 1024 * 1024
#define GB 1024 * MB


void RenderInstance::CreateVulkanRenderer(WindowManager* window)
{
	this->windowMan = window;
	windowMan->SetWindowResizeCallback(frameResizeCB);
	glfwSetWindowUserPointer(windowMan->GetWindow(), this);

	vkInstance.SetInstanceDataAndSize(16 * MB);
	vkInstance.CreateRenderInstance();
	vkInstance.CreateDrawingSurface(window->GetWindow());

	physicalIndex = vkInstance.CreatePhysicalDevice();
	VKDevice& majorDevice = vkInstance.CreateLogicalDevice(physicalIndex, deviceIndex);

	msaaSamples = vkInstance.GetMaxMSAALevels(physicalIndex);

	VkPhysicalDeviceFeatures features{};
	features.geometryShader = VK_TRUE;
	features.textureCompressionBC = VK_TRUE;
	features.tessellationShader = VK_TRUE;
	features.samplerAnisotropy = VK_TRUE;
	features.multiDrawIndirect = VK_TRUE;

	majorDevice.CreateLogicalDevice(vkInstance.instanceLayers, 
		vkInstance.deviceExtensions, 
		VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, features, vkInstance.renderSurface, 6 * MB, 1.0f/6.0f);

	depthFormat = VK::Utils::findSupportedFormat(majorDevice.gpu,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice.CreateHostBuffer(64 * MB, true, false, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

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

	attachmentsIndex = majorDevice.CreateImageMemoryPool(512 * MB, depthPool.first);

	CreateMSAAColorResources(800, 600);

	CreateDepthImage(800, 600);

	CreateRenderPass();

	majorDevice.CreatePipelineCache(mainRenderPass);

	std::vector attachmentViews = { &colorView, &depthView };

	swapChain.CreateSwapChain(800, 600, mainRenderPass, attachmentViews);

	DescriptorPoolBuilder builder{};
	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);

	majorDevice.CreateDesciptorPool(descriptorPoolIndex, builder, MAX_FRAMES_IN_FLIGHT * 100);

	CreateGlobalBuffer();

	CreatePipelines();

	auto drawingCallback = [this](uint32_t cbIndex, uint32_t iIndex)
		{
			this->MonolithicDrawingTask(cbIndex, iIndex);
		};

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		cbsIndices = majorDevice.CreateReusableCommandBuffers(3, i*3, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true, COMPUTE | TRANSFER | GRAPHICS);
		auto& ref = threadedRecordBuffers[i];
		int n = MAX_FRAMES_IN_FLIGHT;
		currentCBIndex[i] = cbsIndices[0];
		for (int j = 0; j < 3; j++)
		{
			ref.buffers[j] = cbsIndices[j];
		}
		ref.outputImageIndex = i;
		ref.drawingFunction = drawingCallback;
		//ref.DrawMain();
		ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>::DrawLoop),
				&ref, std::placeholders::_1));
	}
}

OffsetIndex RenderInstance::CreateRenderGraph(size_t datasize, size_t alignment)
{
	VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	DescriptorSetBuilder dsb = CreateDescriptorSet("mainrenderpass", MAX_FRAMES_IN_FLIGHT);
	dsb.AddDynamicUniformBuffer(GetDynamicUniformBuffer(), sizeof(glm::mat4) * 2, 0, MAX_FRAMES_IN_FLIGHT, 0);
	dsb.AddDescriptorsToCache("mainrenderpass");
	OffsetIndex perRenderPassStuff = GetPageFromUniformBuffer(datasize, alignment);
	std::vector<uint32_t> data(1, perRenderPassStuff);
	majorDevice.CreateRenderGraph(mainRenderPass, data, "mainrenderpass");
	return perRenderPassStuff;
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

void RenderInstance::SetResizeBool(bool set)
{
	resizeWindow = set;
}

uint32_t RenderInstance::GetCurrentFrame() const
{
	return currentFrame;
}

VkSampleCountFlagBits RenderInstance::GetMSAASamples() const
{
	return msaaSamples;
}

uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainWidth();
}

DescriptorSetBuilder RenderInstance::CreateDescriptorSet(std::string layoutname, uint32_t frames)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	return dev.CreateDescriptorSetBuilder(descriptorPoolIndex, layoutname, frames);
}

void RenderInstance::CreateVulkanPipelineObject(VKPipelineObject *pipeline)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	auto& graphIndex = dev.graphMapping[mainRenderPass];
	auto& graph = dev.graphs[graphIndex];
	graph->AddObject(pipeline);
}

void RenderInstance::DrawScene(uint32_t cbindex)
{
	VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
	if (!dev.graphMapping.count(mainRenderPass)) return;
	auto rcb = dev.GetRecordingBufferObject(cbindex);
	auto& graphIndex = dev.graphMapping[mainRenderPass];
	auto& graph = dev.graphs[graphIndex];
	graph->DrawScene(rcb, currentFrame);
}

void RenderInstance::InvalidateRecordBuffer(uint32_t i)
{
	threadedRecordBuffers[i].Invalidate();
	//threadedRecordBuffers[i].DrawLoop();
}

