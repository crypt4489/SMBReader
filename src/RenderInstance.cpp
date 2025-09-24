#include "RenderInstance.h"



#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <limits>

#include "FileManager.h"
#include "ThreadManager.h"
#include "VKInstance.h"
#include "VKDescriptorLayoutBuilder.h"
#include "VKDescriptorSetBuilder.h"
#include "VKRenderPassBuilder.h"
#include "VKSwapChain.h"
#include "VKRenderGraph.h"
#include "VKPipelineCache.h"
#include "VertexTypes.h"
#include "WindowManager.h"

namespace VKRenderer {
	RenderInstance* gRenderInstance = nullptr;
}


namespace API {

	VkCompareOp ConvertDepthTestAppToVulkan(DepthTest testApp)
	{
		VkCompareOp ret = VK_COMPARE_OP_LESS;
		switch (testApp)
		{
		case DepthTest::ALLPASS:
			ret = VK_COMPARE_OP_ALWAYS;
			break;
		default:
			break;
		}

		return ret;
	}

	VkFormat ConvertSMBToVkFormat(ImageFormat format)
	{
		VkFormat vkFormat = VK_FORMAT_MAX_ENUM;
		switch (format)
		{
		case ImageFormat::DXT1:
			vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
			break;
		case ImageFormat::DXT3:
			vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
			break;
		case ImageFormat::R8G8B8A8:
			vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}
		return vkFormat;
	}

}


static void frameResizeCB(GLFWwindow* window, int width, int height)
{
	auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
	renderInst->SetResizeBool(true);
}

RenderInstance::RenderInstance()
{
	vkInstance = new VKInstance();
};

RenderInstance::~RenderInstance()
{
	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	for (size_t i = 0; i < shaders.size(); i++)
	{
		dev->DestroyShader(shaders[i]);
	}

	for (size_t i = 0; i < threadedRecordBuffers.size(); i++)
	{
		auto& trb = threadedRecordBuffers[i];
		for (size_t j = 0; j < trb.buffers.size(); j++)
		{
			dev->DestroyCommandBuffer(trb.buffers[j]);
		}
	}

	for (auto& i : descriptorLayouts)
	{
		dev->DestroyDescriptorLayout(i.second);
	}

	dev->DestroyDescriptorPool(descriptorPoolIndex);

	for (auto& i : pipelinesIdentifier)
	{
		dev->DestroyPipelineCacheObject(i.second);
	}

	dev->DestroyRenderPass(mainRenderPass);

	DestroySwapChainAttachments();

	dev->DestroyImagePool(attachmentsIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	swapChain->DestroySwapChain();

	swapChain->DestroySyncObject();

	dev->DestroyBuffer(stagingBufferIndex);

	dev->DestroyBuffer(globalIndex);

	dev->DestroyDevice();

	delete vkInstance;
};

void RenderInstance::DestoryTexture(EntryHandle handle)
{
	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->DestroyTexture(handle);
}

void RenderInstance::CreateDepthImage(uint32_t width, uint32_t height)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	depthImage = dev->CreateImage(width, height,
		1, depthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		msaaSamples,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

	depthView = dev->CreateImageView(depthImage, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	dev->TransitionImageLayout(
		depthImage, depthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);
}

void RenderInstance::DestroySwapChainAttachments()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->DestroyImage(depthImage);
	dev->DestroyImageView(depthView);
	dev->DestroyImage(colorImage);
	dev->DestroyImageView(colorView);
}

void RenderInstance::RecreateSwapChain() {
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VkDevice logicalDevice = dev->device;
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);
	int width = 0, height = 0;
	glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(logicalDevice);

	DestroySwapChainAttachments();
	swapChain->DestroySwapChain();

	CreateMSAAColorResources(width, height);
	CreateDepthImage(width, height);

	swapChain->RecreateSwapChain(width, height);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		InvalidateRecordBuffer(i);
	}
}

void RenderInstance::CreateRenderPass()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	VKRenderPassBuilder rpb = dev->CreateRenderPassBuilder(3, 1, 1);
	VkFormat format = swapChain->GetSwapChainFormat();

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

	rpb.CreateSubPassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, 2, 1);

	rpb.CreateSubPassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

	rpb.CreateInfo();

	mainRenderPass = dev->CreateRenderPasses(rpb);
}

void RenderInstance::BeginCommandBufferRecording(EntryHandle cb, uint32_t imageIndex)
{

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	auto rbo = dev->GetRecordingBufferObject(cb);

	rbo.BeginRecordingCommand();
	
	rbo.BeginRenderPassCommand(swapChain->renderTargetIndex, imageIndex, { {0, 0}, swapChain->swapChainExtent });

	uint32_t x = swapChain->GetSwapChainWidth(), y = swapChain->GetSwapChainHeight();

	rbo.SetViewportCommand(0, 0, x, y, 0.0f, 1.0f);

	rbo.SetScissorCommand(0, 0, x, y);

}

void RenderInstance::MonolithicDrawingTask(EntryHandle commandBufferIndex, uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	commandBufferIndex = dev->RequestWithPossibleBufferResetAndFenceReset(UINT64_MAX, commandBufferIndex, true, false);

	BeginCommandBufferRecording(commandBufferIndex, imageIndex);

	DrawScene(commandBufferIndex);

	EndCommandBufferRecording(commandBufferIndex);
}

void RenderInstance::EndCommandBufferRecording(EntryHandle cb)
{

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto rbo = dev->GetRecordingBufferObject(cb);

	rbo.EndRenderPassCommand();

	rbo.EndRecordingCommand();
}

uint32_t RenderInstance::BeginFrame()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto& trb = threadedRecordBuffers[currentFrame];

	EntryHandle nextCbIndex = trb.GetCurrentBuffer();

	if (nextCbIndex == EntryHandle()) {
		return ~0ui32;
	}

	int32_t res;

	if (nextCbIndex != currentCBIndex[currentFrame])
		res = dev->WaitOnCommandBufferAndPossibleResetFence(UINT64_MAX, currentCBIndex[currentFrame], false); //wait on previous, but don't reset

	currentCBIndex[currentFrame] = nextCbIndex;

	res = dev->WaitOnCommandBufferAndPossibleResetFence(UINT64_MAX, currentCBIndex[currentFrame], true); // wait on current, could be updated queue

	uint32_t imageIndex = dev->BeginFrameForSwapchain(swapChainIndex, currentFrame);

	if (imageIndex == ~0ui32)
	{
		RecreateSwapChain();
		return imageIndex;
	}

	return imageIndex;
}

void RenderInstance::SubmitFrame(uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t res = dev->SubmitCommandsForSwapChain(swapChainIndex, imageIndex, currentCBIndex[currentFrame]);

	res = dev->PresentSwapChain(swapChainIndex, imageIndex, currentCBIndex[currentFrame]);

	auto& trb = threadedRecordBuffers[currentFrame];

	trb.ReleaseCurrentCommandBuffer();

	if (!res || resizeWindow) {
		resizeWindow = false;
		RecreateSwapChain();
		currentFrame = 0;
	}
	else {
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
}



void RenderInstance::CreateMSAAColorResources(uint32_t width, uint32_t height) {

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	colorImage = dev->CreateImage(width, height,
		1, swapChain->GetSwapChainFormat(), 1,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		msaaSamples,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

	colorView = dev->CreateImageView(colorImage, 1, swapChain->GetSwapChainFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
}

void RenderInstance::WaitOnRender()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->WaitOnDevice();
}

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	

	std::vector<std::string> shaders1 = { "3dtextured.vert.spv", "3dtextured.frag.spv" };

	std::vector<EntryHandle> shaders1Handles;

	size_t counter = 0;

	for (size_t i = 0; i < shaders1.size(); i++)
	{
		std::string name = shaders1[i];
		std::vector<char> buffer;
		if (FileManager::FileExists(name)) {
			
			auto ret = FileManager::ReadFileInFull(name, buffer);

			shaders1Handles.push_back(dev->CreateShader(buffer.data(), buffer.size(), dev->ConvertShaderFlags(name)));
		}
		else
		{
			std::string uncompiled = name.substr(0, name.length() - 4);

			auto ret = FileManager::ReadFileInFull(uncompiled, buffer);

			if (buffer.back() != '\0') buffer.push_back('\0');

			shaders1Handles.push_back(dev->CompileShader(buffer.data(), dev->ConvertShaderFlags(name)));
		}

		shaders[counter++] = shaders1Handles[i];
	}

	std::vector<std::string> shaders2 = { "text.vert.spv" , "text.frag.spv" };

	std::vector<EntryHandle> shaders2Handles;

	for (size_t i = 0; i < shaders2.size(); i++)
	{
		std::string name = shaders2[i];
		std::vector<char> buffer;
		if (FileManager::FileExists(name)) {

			auto ret = FileManager::ReadFileInFull(name, buffer);

			shaders2Handles.push_back(dev->CreateShader(buffer.data(), buffer.size(), dev->ConvertShaderFlags(name)));
		}
		else
		{
			std::string uncompiled = name.substr(0, name.length() - 4);

			auto ret = FileManager::ReadFileInFull(uncompiled, buffer);

			if (buffer.back() != '\0') buffer.push_back('\0');

			shaders2Handles.push_back(dev->CompileShader(buffer.data(), dev->ConvertShaderFlags(name)));
		}

		shaders[counter++] = shaders2Handles[i];
	}


	auto mainRenderPassCache = dev->GetPipelineCache(mainRenderPass);

	DescriptorSetLayoutBuilder* textDescriptor = dev->CreateDescriptorSetLayoutBuilder(1);
	DescriptorSetLayoutBuilder* globalBufferBuilder = dev->CreateDescriptorSetLayoutBuilder(1); 
	DescriptorSetLayoutBuilder* genericObjectBuilder = dev->CreateDescriptorSetLayoutBuilder(2);
	std::array<std::string, 1> textDescriptorContainers = { "oneimage" };
	std::array<std::string, 2> regularMeshConatiners = { "mainrenderpass", "genericobject" };

	std::array<EntryHandle, 1> tdsIDs;
	std::array<EntryHandle, 2> rmcIDs;

	textDescriptor->AddPixelImageSamplerLayout(0);

	descriptorLayouts[textDescriptorContainers[0]] = tdsIDs[0] = dev->CreateDescriptorSetLayout(textDescriptor);

	globalBufferBuilder->AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	descriptorLayouts[regularMeshConatiners[0]] = rmcIDs[0] = dev->CreateDescriptorSetLayout(globalBufferBuilder);

	genericObjectBuilder->AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

	genericObjectBuilder->AddPixelImageSamplerLayout(1);

	descriptorLayouts[regularMeshConatiners[1]] = rmcIDs[1] = dev->CreateDescriptorSetLayout(genericObjectBuilder);


	std::array<VkVertexInputBindingDescription, 1> bindings1 = { BasicVertex::getBindingDescription() };

	auto ref1 = BasicVertex::getAttributeDescriptions();


	pipelinesIdentifier["genericpipeline"] = mainRenderPassCache->CreatePipeline(rmcIDs.data(), 2, bindings1.data(), 1, ref1.data(), ref1.size(),
		shaders1Handles.data(), shaders1Handles.size(), VK_COMPARE_OP_LESS, GetMSAASamples()
	);

	std::array<VkVertexInputBindingDescription, 1> bindings = { TextVertex::getBindingDescription() };

	auto ref = TextVertex::getAttributeDescriptions();

	pipelinesIdentifier["text"] = mainRenderPassCache->CreatePipeline(
		tdsIDs.data(), 1,
		bindings.data(), 1,
		ref.data(), ref.size(), 
		shaders2Handles.data(), shaders2Handles.size(),
		VK_COMPARE_OP_ALWAYS, GetMSAASamples()
	);
}

void RenderInstance::CreateGlobalBuffer()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	globalIndex = dev->CreateHostBuffer
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
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * this->currentFrame));
}

void RenderInstance::UpdateDynamicGlobalBufferAbsolute(void* data, size_t dataSize, size_t offset)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->WriteToHostBuffer(globalIndex, data, dataSize, offset);
}

void RenderInstance::UpdateDynamicGlobalBufferForAllFrames(void* data, size_t dataSize, size_t offset)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	for (int i = 0; i<MAX_FRAMES_IN_FLIGHT; i++)
		dev->WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * i));
}


OffsetIndex RenderInstance::GetPageFromUniformBuffer(size_t size, uint32_t alignment)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return std::move(dev->GetOffsetIntoHostBuffer(globalIndex, size, alignment));
}

EntryHandle RenderInstance::GetMainBufferIndex() const
{
	return globalIndex;
}

VkBuffer RenderInstance::GetDynamicUniformBuffer()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetHostBuffer(globalIndex);
}

EntryHandle RenderInstance::CreateVulkanImage(
	char* imageData,
	uint32_t* imageSizes,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateSampledImage(
		imageData, imageSizes,
		width, height,
		mipLevels, API::ConvertSMBToVkFormat(type),
		attachmentsIndex, 
		stagingBufferIndex, VK_IMAGE_ASPECT_COLOR_BIT);
}

EntryHandle RenderInstance::CreateVulkanImageView(EntryHandle& imageIndex, uint32_t mipLevels,
	ImageFormat type, VkImageAspectFlags aspectMask)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateImageView(imageIndex, mipLevels, API::ConvertSMBToVkFormat(type), aspectMask);
}

EntryHandle RenderInstance::CreateVulkanSampler(uint32_t mipLevels)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateSampler(mipLevels);
}

void RenderInstance::DeleteVulkanImageView(EntryHandle& index)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice->DestroyImage(index);
}

void RenderInstance::DeleteVulkanImage(EntryHandle& index)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice->DestroyImage(index);
}

void RenderInstance::DeleteVulkanSampler(EntryHandle& index)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	majorDevice->DestorySampler(index);
}

#define KB 1024
#define MB 1024 * KB
#define GB 1024 * MB


void RenderInstance::CreateVulkanRenderer(WindowManager* window)
{
	this->windowMan = window;
	windowMan->SetWindowResizeCallback(frameResizeCB);
	glfwSetWindowUserPointer(windowMan->GetWindow(), this);

	vkInstance->SetInstanceDataAndSize(16 * MB, 256 * KB);
	vkInstance->CreateRenderInstance();
	vkInstance->CreateDrawingSurface(window->GetWindow());

	physicalIndex = vkInstance->CreatePhysicalDevice(1);
	VKDevice* majorDevice = vkInstance->CreateLogicalDevice(physicalIndex, deviceIndex);

	msaaSamples = vkInstance->GetMaxMSAALevels(physicalIndex);

	VkPhysicalDeviceFeatures features{};
	features.geometryShader = VK_TRUE;
	features.textureCompressionBC = VK_TRUE;
	features.tessellationShader = VK_TRUE;
	features.samplerAnisotropy = VK_TRUE;
	features.multiDrawIndirect = VK_TRUE;

	majorDevice->CreateLogicalDevice(vkInstance->instanceLayers,
		vkInstance->instanceLayerCount,
		vkInstance->deviceExtensions, 
		vkInstance->deviceExtCount,
		VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, 
		features, vkInstance->renderSurface, 
		6 * MB, 
		1 * MB);



	std::array<VkFormat, 3> formats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };

	depthFormat = VK::Utils::findSupportedFormat(majorDevice->gpu,
		formats.data(),
		formats.size(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice->CreateHostBuffer(64 * MB, true, false, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	swapChainIndex = majorDevice->CreateSwapChain(3, MAX_FRAMES_IN_FLIGHT, 1, 2);

	VKSwapChain* swapChain = majorDevice->GetSwapChain(swapChainIndex);

	VkFormat swcFormat = swapChain->GetSwapChainFormat();

	auto swcPool = majorDevice->FindImageMemoryIndexForPool(1920, 1200,
		1, swcFormat, 1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	auto depthPool = majorDevice->FindImageMemoryIndexForPool(1920, 1200,
		1, depthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	attachmentsIndex = majorDevice->CreateImageMemoryPool(512 * MB, depthPool.first);

	CreateMSAAColorResources(800, 600);

	CreateDepthImage(800, 600);

	CreateRenderPass();

	std::array attachmentViews = { &colorView, &depthView };

	swapChain->CreateSwapChain(800, 600, mainRenderPass, attachmentViews.data(), 2);

	DescriptorPoolBuilder builder = majorDevice->CreateDescriptorPoolBuilder(2);
	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);

	descriptorPoolIndex = majorDevice->CreateDesciptorPool(builder, MAX_FRAMES_IN_FLIGHT * 101);

	CreateGlobalBuffer();

	CreatePipelines();

	auto drawingCallback = [this](EntryHandle cbIndex, uint32_t iIndex)
		{
			this->MonolithicDrawingTask(cbIndex, iIndex);
		};

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		EntryHandle* cbsIndices = majorDevice->CreateReusableCommandBuffers(3, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true, COMPUTE | TRANSFER | GRAPHICS);
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
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	DescriptorSetBuilder *dsb = CreateDescriptorSet(descriptorLayouts["mainrenderpass"], MAX_FRAMES_IN_FLIGHT);
	dsb->AddDynamicUniformBuffer(GetDynamicUniformBuffer(), sizeof(glm::mat4) * 2, 0, MAX_FRAMES_IN_FLIGHT, 0);
	EntryHandle mrpDescId =  dsb->AddDescriptorsToCache();
	OffsetIndex perRenderPassStuff = GetPageFromUniformBuffer(datasize, alignment);
	std::vector<uint32_t> data(1, perRenderPassStuff);
	majorDevice->UpdateRenderGraph(mainRenderPass, data.data(), data.size(), mrpDescId);
	return perRenderPassStuff;
}

VkImageView RenderInstance::GetImageView(EntryHandle viewIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->GetImageViewByTexture(viewIndex);
}

VkSampler RenderInstance::GetSampler(EntryHandle index)
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSamplerByTexture(index);
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
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

DescriptorSetBuilder* RenderInstance::CreateDescriptorSet(EntryHandle layoutname, uint32_t frames)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return dev->CreateDescriptorSetBuilder(descriptorPoolIndex, layoutname, frames);
}

void RenderInstance::CreateVulkanPipelineObject(VKPipelineObject *pipeline)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	auto graph = dev->GetRenderGraph(mainRenderPass);
	graph->AddObject(pipeline);
}

void RenderInstance::DrawScene(EntryHandle cbindex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto graph = dev->GetRenderGraph(mainRenderPass);
	//if (!graph.objects.size()) return;
	auto rcb = dev->GetRecordingBufferObject(cbindex);
	graph->DrawScene(rcb, currentFrame);
}

void RenderInstance::InvalidateRecordBuffer(uint32_t i)
{
	threadedRecordBuffers[i].Invalidate();
	//threadedRecordBuffers[i].DrawLoop();
}

