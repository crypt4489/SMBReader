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
#include "VKDevice.h"
#include "VKDescriptorLayoutBuilder.h"
#include "VKDescriptorSetBuilder.h"
#include "VKRenderPassBuilder.h"
#include "VKSwapChain.h"
#include "VKGraph.h"
#include "VKPipelineBuilder.h"
#include "VKPipelineObject.h"
#include "VertexTypes.h"
#include "WindowManager.h"


#include "ShaderResourceSet.h"

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
		case ImageFormat::R8G8B8A8_UNORM:
			vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case ImageFormat::D24UNORMS8STENCIL:
			vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;
			break;
		case ImageFormat::D32FLOATS8STENCIL:
			vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
			break;

		case ImageFormat::D32FLOAT:
			vkFormat = VK_FORMAT_D32_SFLOAT;
			break;
		}
		return vkFormat;
	}


	ImageFormat ConvertVkFormatToAppFormat(VkFormat vkFormat)
	{
		ImageFormat format = ImageFormat::IMAGE_UNKNOWN;
		switch (vkFormat)
		{
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			format = ImageFormat::DXT1;
			break;
		case VK_FORMAT_BC3_SRGB_BLOCK:
			format = ImageFormat::DXT3;
			break;
		case VK_FORMAT_R8G8B8A8_SRGB:
			format = ImageFormat::R8G8B8A8;
			break;
		case VK_FORMAT_R8G8B8A8_UNORM:
			format = ImageFormat::R8G8B8A8_UNORM;
			break;
		case VK_FORMAT_D24_UNORM_S8_UINT:
			format = ImageFormat::D24UNORMS8STENCIL;
			break;

		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			format = ImageFormat::D32FLOATS8STENCIL;
			break;

		case VK_FORMAT_D32_SFLOAT:
			format = ImageFormat::D32FLOAT;
			break;

		}
		return format;
	}

	VkPrimitiveTopology ConvertTopology(PrimitiveType type)
	{
		VkPrimitiveTopology top = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		switch (type)
		{
		case TRIANGLES:
			top = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		case TRISTRIPS:
			top = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			break;
		}

		return top;
	}

}


static void frameResizeCB(GLFWwindow* window, int width, int height)
{
	auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
	renderInst->resizeWindow = true;
}

#define KB 1024
#define MB 1024 * KB
#define GB 1024 * MB

RenderInstance::RenderInstance()
	:
	vulkanShaderGraphs{},
	descriptorManager{}
{
	vkInstance = new VKInstance();
	vulkanShaderGraphs.shaderGraphs = (uintptr_t)malloc(1 * KB);
	descriptorManager.hostResourceHeap = (uintptr_t)malloc(1 * KB);

	if (!vulkanShaderGraphs.shaderGraphs || !descriptorManager.hostResourceHeap)
	{
		throw std::runtime_error("cannot allocate render instance");
	}
	
	vulkanShaderGraphs.shaderGraphOffset = 0;
	descriptorManager.hostResourceHead = 0;

};

RenderInstance::~RenderInstance()
{
	free((void*)vulkanShaderGraphs.shaderGraphs);
	free((void*)descriptorManager.hostResourceHeap);

	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	for (size_t i = 0; i < vulkanShaderGraphs.shaders.size(); i++)
	{
		dev->DestroyShader(vulkanShaderGraphs.shaders[i]);
	}

	for (size_t i = 0; i < threadedRecordBuffers.size(); i++)
	{
		auto& trb = threadedRecordBuffers[i];
		for (size_t j = 0; j < trb.buffers.size(); j++)
		{
			dev->DestroyCommandBuffer(trb.buffers[j]);
		}
	}

	for (auto& i : vulkanDescriptorLayouts)
	{
		dev->DestroyDescriptorLayout(i);
	}

	dev->DestroyDescriptorPool(descriptorManager.deviceResourceHeap);

	for (auto& i : pipelinesIdentifier)
	{
		for (auto &j : i)
			dev->DestroyPipelineCacheObject(j);
	}

	for (uint32_t i = 0; i<maxMSAALevels; i++)
		dev->DestroyRenderPass(renderPasses[i]);

	DestroySwapChainAttachments();

	dev->DestroyImagePool(attachmentsIndex);
	
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	swapChain->DestroySwapChain();

	swapChain->DestroySyncObject();
	

	dev->DestroyBuffer(stagingBufferIndex);

	dev->DestroyBuffer(globalIndex);

	dev->DestroyBuffer(globalDeviceBufIndex);

	dev->DestroyDevice();

	delete vkInstance;
};

void RenderInstance::DestoryTexture(EntryHandle handle)
{
	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->DestroyTexture(handle);
}

void RenderInstance::CreateDepthImage(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	VkFormat vkDepthFormat = API::ConvertSMBToVkFormat(depthFormat);

	depthImages[index] = dev->CreateImage(width, height,
		1, vkDepthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		sampleCount,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, attachmentsIndex);

	depthViews[index] = dev->CreateImageView(depthImages[index], 1, vkDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	dev->TransitionImageLayout(
		depthImages[index], vkDepthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);
}

void RenderInstance::DestroySwapChainAttachments()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	for (uint32_t i = 0; i < maxMSAALevels; i++) {
		dev->DestroyImage(depthImages[i]);
		dev->DestroyImageView(depthViews[i]);	
	}

	for (uint32_t i = 0; i < maxMSAALevels-1; i++) {
		dev->DestroyImage(colorImages[i]);
		dev->DestroyImageView(colorViews[i]);
	}

	
}

void RenderInstance::RecreateSwapChain() {
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	
	int width = 0, height = 0;
	
	windowMan->GetWindowSize(&width, &height);

	dev->WaitOnDevice();

	DestroySwapChainAttachments();
	
	CreateSwapChain(width, height, true);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		threadedRecordBuffers[i].Reset();
	}
}

void RenderInstance::CreateRenderPass(uint32_t index, VkSampleCountFlagBits sampleCount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	VkFormat format = swapChain->GetSwapChainFormat();

	VkFormat vkDepthFormat = API::ConvertSMBToVkFormat(depthFormat);

	if (sampleCount > VK_SAMPLE_COUNT_1_BIT)
	{

		VKRenderPassBuilder rpb = dev->CreateRenderPassBuilder(3, 1, 1);


		rpb.CreateAttachment(VKRenderPassBuilder::COLORATTACH, format, sampleCount,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0);


		rpb.CreateAttachment(VKRenderPassBuilder::COLORRESOLVEATTACH, format, VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE
			, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 2);

		rpb.CreateAttachment(VKRenderPassBuilder::DEPTHSTENCILATTACH, vkDepthFormat, sampleCount,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

		rpb.CreateSubPassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, 2, 1);

		rpb.CreateSubPassDependency(VK_SUBPASS_EXTERNAL, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		rpb.CreateInfo();

		renderPasses[index] = dev->CreateRenderPasses(rpb);
	}
	else {
		VKRenderPassBuilder rpb = dev->CreateRenderPassBuilder(2, 1, 1);


		rpb.CreateAttachment(VKRenderPassBuilder::COLORATTACH, format, sampleCount,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0);

		rpb.CreateAttachment(VKRenderPassBuilder::DEPTHSTENCILATTACH, vkDepthFormat, sampleCount,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

		rpb.CreateSubPassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, ~0ui32, 1);

		rpb.CreateSubPassDependency(VK_SUBPASS_EXTERNAL, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		rpb.CreateInfo();

		renderPasses[index] = dev->CreateRenderPasses(rpb);
	}
}

void RenderInstance::BeginCommandBufferRecording(EntryHandle cb, uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto rbo = dev->GetRecordingBufferObject(cb);

	rbo.BeginRecordingCommand();
}

void RenderInstance::MonolithicDrawingTask(EntryHandle commandBufferIndex, uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	commandBufferIndex = dev->RequestWithPossibleBufferResetAndFenceReset(UINT64_MAX, commandBufferIndex, true, false);

	BeginCommandBufferRecording(commandBufferIndex, imageIndex);

	DrawScene(commandBufferIndex, imageIndex);

	EndCommandBufferRecording(commandBufferIndex);
}

void RenderInstance::EndCommandBufferRecording(EntryHandle cb)
{

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto rbo = dev->GetRecordingBufferObject(cb);

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



void RenderInstance::CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount) {

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	colorImages[index] = dev->CreateImage(width, height,
		1, swapChain->GetSwapChainFormat(), 1,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		sampleCount,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, attachmentsIndex);

	colorViews[index] = dev->CreateImageView(colorImages[index], 1, swapChain->GetSwapChainFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
}

void RenderInstance::WaitOnRender()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	dev->WaitOnDevice();
}

void RenderInstance::CreateSwapChain(uint32_t width, uint32_t height, bool recreate)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	if (recreate)
	{
		swapChain->ResetSwapChain();
		swapChain->RecreateSwapChain(width, height);
	}
	else
	{
		swapChain->CreateSwapChain(width, height);
		for (uint32_t i = 0; i < maxMSAALevels; i++)
		{
			swapChain->CreateRenderTarget(i, renderPasses[i]);
			swapchainRenderTargets[i] = swapChain->renderTargetIndex[i];
		}
	}
	

	EntryHandle* views = swapChain->CreateSwapchainViews();

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		CreateDepthImage(width, height, i, 1 << i);

		std::array<EntryHandle, 3> attachmentViews;

		uint32_t count = 0;

		if (0 < i)
		{
			CreateMSAAColorResources(width, height, i - 1, 1 << i);

			attachmentViews[0] = colorViews[i - 1];
			attachmentViews[1] = depthViews[i];
			attachmentViews[2] = EntryHandle();
			count = 3;
		}
		else 
		{
			attachmentViews[0] = EntryHandle();
			attachmentViews[1] = depthViews[i];
			count = 2;
		}
			
		swapChain->CreateSwapChainElements(i, count, attachmentViews.data(), views);
	}
}

void RenderInstance::CreateShaderResourceMap(ShaderGraph* graph)
{
	static int descriptorLayoutIndex = 0;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	std::array<DescriptorSetLayoutBuilder*, 5> descriptorBuilders{};

	int count = graph->resourceSetCount;

	for (int j = 0; j < graph->resourceSetCount; j++)
	{
		ShaderSetLayout* set = (ShaderSetLayout*)graph->GetSet(j);
		descriptorBuilders[j] = dev->CreateDescriptorSetLayoutBuilder(set->bindingCount);
		set->vkDescriptorLayout = descriptorLayoutIndex+j;
	}

	

	for (int j = 0; j < graph->shaderMapCount; j++)
	{
		ShaderMap* map = (ShaderMap*)graph->GetMap(j);

		VkShaderStageFlags stageFlags = ConvertShaderStageToVKShaderStageFlags(map->type);

		for (int h = 0; h < map->resourceCount; h++)
		{
			ShaderResource* resource = (ShaderResource*)map->GetResource(h);

			if (resource->type & CONSTANT_BUFFER) {
				descriptorBuilders[resource->set]->bindingCounts--;
				continue;
			}
			
			DescriptorSetLayoutBuilder* descriptorBuilder = descriptorBuilders[resource->set];

			switch (resource->type)
			{
			case UNIFORM_BUFFER:
				descriptorBuilder->AddDynamicBufferLayout(resource->binding, stageFlags);
				break;
			case IMAGESTORE2D:
				descriptorBuilder->AddStorageImageLayout(resource->binding, stageFlags);
				break;
			case SAMPLER:
				descriptorBuilder->AddPixelImageSamplerLayout(resource->binding, stageFlags);
				break;
			case STORAGE_BUFFER:
				descriptorBuilder->AddDynamicStorageBufferLayout(resource->binding, stageFlags);
				break;
			}
		}
	}


	for (int j = 0; j < count; j++)
	{
		vulkanDescriptorLayouts[descriptorLayoutIndex++] = dev->CreateDescriptorSetLayout(descriptorBuilders[j]);
	}

}

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	

	std::array shaders1 = { "3dtextured.vert.spv", "3dtextured.frag.spv", "text.vert.spv" , "text.frag.spv", "mesh_interpolate.comp.spv", "polynomialimage.comp.spv"};

	vulkanShaderGraphs.shaderGraphPtrs[0] = ShaderGraphReader::CreateShaderGraph("C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\3DTexturedLayout.xml", vulkanShaderGraphs.shaderGraphs, &vulkanShaderGraphs.shaderGraphOffset);

	CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[0]);

	vulkanShaderGraphs.shaderGraphPtrs[1] = ShaderGraphReader::CreateShaderGraph("C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\TextLayout.xml", vulkanShaderGraphs.shaderGraphs, &vulkanShaderGraphs.shaderGraphOffset);

	CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[1]);

	vulkanShaderGraphs.shaderGraphPtrs[2] = ShaderGraphReader::CreateShaderGraph("C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\InterpolateMeshLayout.xml", vulkanShaderGraphs.shaderGraphs, &vulkanShaderGraphs.shaderGraphOffset);

	CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[2]);

	vulkanShaderGraphs.shaderGraphPtrs[3] = ShaderGraphReader::CreateShaderGraph("C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\PolynomialLayout.xml", vulkanShaderGraphs.shaderGraphs, &vulkanShaderGraphs.shaderGraphOffset);

	CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[3]);

	size_t counter = 0;

	for (size_t i = 0; i < shaders1.size(); i++)
	{
		std::string name = shaders1[i];
		std::vector<char> buffer;
		if (FileManager::FileExists(name)) {

			auto ret = FileManager::ReadFileInFull(name, buffer, std::ios::binary);
		}
		else
		{
			std::string uncompiled = name.substr(0, name.length() - 4);

			auto ret = FileManager::ReadFileInFull(uncompiled, buffer, std::ios::binary);

			if (buffer.back() != '\0') buffer.push_back('\0');
		}

		vulkanShaderGraphs.shaders[counter++] = dev->CreateShader(buffer.data(), buffer.size(), dev->ConvertShaderFlags(name));
	}

	auto computePipeline = dev->CreateComputePipelineBuilder(1, 1);

	computePipeline->AddPushConstantRange(0, sizeof(float), VK_SHADER_STAGE_COMPUTE_BIT, 0);

	pipelinesIdentifier[MESH_INTERPOLATE] = std::vector<EntryHandle>(1, computePipeline->CreateComputePipeline(&vulkanDescriptorLayouts[3], 1, vulkanShaderGraphs.shaders[4]));

	auto polyPipeline = dev->CreateComputePipelineBuilder(1, 0);

	pipelinesIdentifier[POLY] = std::vector<EntryHandle>(1, computePipeline->CreateComputePipeline(&vulkanDescriptorLayouts[4], 1, vulkanShaderGraphs.shaders[5]));

	std::vector<EntryHandle> l(maxMSAALevels);
	std::vector<EntryHandle> r(maxMSAALevels);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		auto genericBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 2, 2, 0);
		auto textBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 1, 2, 0);

		UsePipelineBuilders(genericBuilder, textBuilder, (VkSampleCountFlagBits)(1<<i));

		r[i] = genericBuilder->CreateGraphicsPipeline(&vulkanDescriptorLayouts[0], 2, &vulkanShaderGraphs.shaders[0], 2);

		l[i] = textBuilder->CreateGraphicsPipeline(&vulkanDescriptorLayouts[2], 1, &vulkanShaderGraphs.shaders[2], 2);

	}

	pipelinesIdentifier[GENERIC] = r;
	
	pipelinesIdentifier[TEXT] = l;



}

void RenderInstance::UsePipelineBuilders(VKGraphicsPipelineBuilder* generic, VKGraphicsPipelineBuilder* text, VkSampleCountFlagBits flags)
{
	std::array<VkDynamicState, 2> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	generic->CreateDynamicStateInfo(dynamicStates.data(), 2);
	text->CreateDynamicStateInfo(dynamicStates.data(), 2);

	std::array<VkVertexInputBindingDescription, 1> bindings1 = { BasicVertex::getBindingDescription() };

	auto ref1 = BasicVertex::getAttributeDescriptions();

	std::array<VkVertexInputBindingDescription, 1> bindings = { TextVertex::getBindingDescription() };

	auto ref = TextVertex::getAttributeDescriptions();

	generic->CreateVertexInput(bindings1.data(), 1, ref1.data(), ref1.size());
	text->CreateVertexInput(bindings.data(), 1, ref.data(), ref.size());

	generic->CreateInputAssembly(API::ConvertTopology(TRIANGLES), false);
	text->CreateInputAssembly(API::ConvertTopology(TRISTRIPS), false);

	generic->CreateViewportState(1, 1);
	text->CreateViewportState(1, 1);

	generic->CreateRasterizer(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	text->CreateRasterizer(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	generic->CreateMultiSampling(flags);
	text->CreateMultiSampling(flags);

	generic->CreateColorBlendAttachment(0, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	text->CreateColorBlendAttachment(0, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	generic->CreateColorBlending(VK_LOGIC_OP_COPY);
	text->CreateColorBlending(VK_LOGIC_OP_COPY);

	generic->CreateDepthStencil(VK_COMPARE_OP_LESS);
	text->CreateDepthStencil(VK_COMPARE_OP_ALWAYS);
}

void RenderInstance::CreateGlobalBuffer()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	globalIndex = dev->CreateHostBuffer
	(
		128'000'000, true,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	);

	globalDeviceBufIndex = dev->CreateDeviceBuffer(32'000'000,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

void RenderInstance::UpdateAllocation(void* data, size_t handle, size_t size, size_t offset)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	
	size_t intSize = allocations[handle].size;
	size_t intOffset = allocations[handle].offset + offset;
	
	if (size)
		intSize = size;

	EntryHandle index = allocations[handle].memIndex;

	if (index == globalIndex)
		dev->WriteToHostBuffer(index, data, intSize, intOffset);
	else if (index == globalDeviceBufIndex)
		dev->WriteToDeviceBuffer(index, stagingBufferIndex, data, intSize, intOffset);
}

int RenderInstance::GetPageFromUniformBuffer(size_t size, uint32_t alignment)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	size_t location = dev->GetMemoryFromBuffer(globalIndex, size, 64);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = globalIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].size = size;


	return index;
}

int RenderInstance::GetPageFromDeviceBuffer(size_t size, uint32_t alignment)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	size_t location = dev->GetMemoryFromBuffer(globalDeviceBufIndex, size, alignment);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = globalDeviceBufIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].size = size;


	return index;
}


EntryHandle RenderInstance::CreateImage(
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

EntryHandle RenderInstance::CreateStorageImage(
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateStorageImage(
		width, height,
		mipLevels, API::ConvertSMBToVkFormat(type),
		attachmentsIndex,
		stagingBufferIndex, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
}


int RenderInstance::AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount)
{
	uintptr_t head = descriptorManager.hostResourceHeap + descriptorManager.hostResourceHead;
	
	uintptr_t ptr = head;
	ShaderResourceSet* set = (ShaderResourceSet*)ptr;
	ptr += sizeof(ShaderResourceSet);

	ShaderSetLayout* resourceSet = (ShaderSetLayout*)vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex]->GetSet(targetSet);

	set->bindingCount = resourceSet->bindingCount;
	set->layoutHandle = resourceSet->vkDescriptorLayout;
	set->setCount = setCount;

	uintptr_t* offset = (uintptr_t*)ptr;

	ptr += sizeof(uintptr_t) * (set->bindingCount);

	for (int j = 0; j < vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex]->shaderMapCount; j++)
	{

		ShaderMap* map = (ShaderMap*)vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex]->GetMap(j);
		int constantCount = map->resourceCount-1;
		for (int h = 0; h < map->resourceCount; h++)
		{
			MemoryBarrierType memBarrierType = MEMORY_BARRIER;

			ShaderResource* resource = (ShaderResource*)map->GetResource(h);

			if (resource->set != targetSet) continue;

			ShaderResourceHeader* desc = (ShaderResourceHeader*)ptr;

			if (resource->binding != ~0)
				desc->binding = resource->binding;
			else
				desc->binding = constantCount--;

			desc->type = resource->type;
			desc->action = resource->action;

			offset[desc->binding] = ptr;

			switch (resource->type)
			{
			case IMAGESTORE2D:
			{
				ptr += sizeof(ShaderResourceImage);
				memBarrierType = IMAGE_BARRIER;
				if (resource->action & SHADERWRITE)
				{
					ImageShaderResourceBarrier* barriers = (ImageShaderResourceBarrier*)ptr;
					barriers->dstStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->dstAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
				
					barriers[1].srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers[1].srcAction = WRITE_SHADER_RESOURCE;
					barriers[1].type = memBarrierType;

					ptr += (sizeof(ImageShaderResourceBarrier) * 2);
				}
				break;
			}
			case SAMPLER:
			{
				ptr += sizeof(ShaderResourceImage);
				memBarrierType = IMAGE_BARRIER;
				if (resource->action & SHADERWRITE)
				{
					ImageShaderResourceBarrier* barriers = (ImageShaderResourceBarrier*)ptr;
					barriers->srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->srcAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
					ptr += (sizeof(ImageShaderResourceBarrier));
				}
				break;
			}
			case CONSTANT_BUFFER:
			{
				ShaderResourceConstantBuffer* constants = (ShaderResourceConstantBuffer*)ptr;
				constants->size = 4;
				constants->offset = 0;
				constants->stage = map->type;
				ptr += sizeof(ShaderResourceConstantBuffer);
				break;
			}
			case STORAGE_BUFFER:
			case UNIFORM_BUFFER:
			{
				memBarrierType = BUFFER_BARRIER;
				ptr += sizeof(ShaderResourceBuffer);
				if (resource->action & SHADERWRITE)
				{
					ShaderResourceBarrier* barriers = (ShaderResourceBarrier*)ptr;
					barriers->srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->srcAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
					ptr += (sizeof(ShaderResourceBarrier));
				}
				break;
			}
			}

			
		}
	}

	return descriptorManager.AddShaderToSets(head, ptr - head);
}





void RenderInstance::CreateVulkanRenderer(WindowManager* window)
{
	this->windowMan = window;
	windowMan->SetWindowResizeCallback(frameResizeCB);
	glfwSetWindowUserPointer(windowMan->GetWindow(), this);

	vkInstance->SetInstanceDataAndSize(800 * KB, 256 * KB);
	vkInstance->CreateRenderInstance();
	vkInstance->CreateDrawingSurface(window->GetWindow());

	physicalIndex = vkInstance->CreatePhysicalDevice(1);
	VKDevice* majorDevice = vkInstance->CreateLogicalDevice(physicalIndex, &deviceIndex);

	maxMSAALevels = (uint32_t)log2((float)vkInstance->GetMaxMSAALevels(physicalIndex));
	maxMSAALevels += 1;
	currentMSAALevel = maxMSAALevels - 1;

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
		12 * KB, 
		2 * KB,
		16 * KB,
		8 * MB,
		10 * KB);



	std::array<VkFormat, 3> formats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };

	VkFormat depthFormatVK = VK::Utils::findSupportedFormat(majorDevice->gpu,
		formats.data(),
		formats.size(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice->CreateHostBuffer(64 * MB, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);


	
	swapChainIndex = majorDevice->CreateSwapChain(3, MAX_FRAMES_IN_FLIGHT, 1, 2, maxMSAALevels);
	

	VKSwapChain* swapChain = majorDevice->GetSwapChain(swapChainIndex);

	VkFormat swcFormat = swapChain->GetSwapChainFormat();

	depthFormat = API::ConvertVkFormatToAppFormat(depthFormatVK);

	auto swcPool = majorDevice->FindImageMemoryIndexForPool(1920, 1200,
		1, swcFormat, 1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	auto depthPool = majorDevice->FindImageMemoryIndexForPool(1920, 1200,
		1, depthFormatVK, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	attachmentsIndex = majorDevice->CreateImageMemoryPool(512 * MB, depthPool.first);
	
	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		CreateRenderPass(i, (VkSampleCountFlagBits)(1 << i));
	}

	CreateSwapChain(800, 600, false);
	


	DescriptorPoolBuilder builder = majorDevice->CreateDescriptorPoolBuilder(3);
	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddStoragePoolSize(MAX_FRAMES_IN_FLIGHT * 100);

	descriptorManager.deviceResourceHeap = majorDevice->CreateDesciptorPool(builder, MAX_FRAMES_IN_FLIGHT * 101);

	CreateGlobalBuffer();

	CreatePipelines();

	computeGraphIndex = majorDevice->CreateComputeGraph(0, 5);

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
		ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>::DrawLoop),
				&ref, std::placeholders::_1));
	}
}

size_t RenderInstance::CreateRenderGraph(size_t datasize, size_t alignment)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	int perRenderPassStuff = GetPageFromUniformBuffer(datasize, (uint32_t)alignment);
	int mrpDescHandle = AllocateShaderResourceSet(0, 0, MAX_FRAMES_IN_FLIGHT);

	descriptorManager.BindBufferToShaderResource(mrpDescHandle, perRenderPassStuff, REPEAT, 0);

	EntryHandle mrpDescId = CreateShaderResourceSet(mrpDescHandle);

	std::array<uint32_t, 1> data = { (uint32_t)allocations[perRenderPassStuff].offset };

	for (uint32_t i = 0; i<maxMSAALevels; i++)

		majorDevice->UpdateRenderGraph(swapchainRenderTargets[i], data.data(), (uint32_t)data.size(), mrpDescId);
	

	return perRenderPassStuff;
}


uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

EntryHandle RenderInstance::CreateShaderResourceSet(int descriptorSet)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	int frames = set->setCount;

	DescriptorSetBuilder* builder = dev->CreateDescriptorSetBuilder(descriptorManager.deviceResourceHeap, vulkanDescriptorLayouts[set->layoutHandle], frames);

	int count = set->bindingCount;

	for (int i = 0; i < count; i++)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];

		switch (header->type)
		{
			case IMAGESTORE2D:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)offsets[i];
				builder->AddStorageImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case SAMPLER:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)offsets[i];
				builder->AddPixelShaderImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), dev->GetSamplerByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case STORAGE_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)offsets[i];
				auto alloc = allocations[buffer->allocation];
				if (buffer->copyPattern == REPEAT)
					builder->AddDynamicStorageBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.size / frames, i, frames, 0);
				else
					builder->AddDynamicStorageBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.size, i, frames, 0);
				break;
			}
			case UNIFORM_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)offsets[i];
				auto alloc = allocations[buffer->allocation];
				if (buffer->copyPattern == REPEAT)
					builder->AddDynamicUniformBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.size / frames, i, frames, 0);
				else
					builder->AddDynamicUniformBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.size, i, frames, 0);
				break;
			}
		}
	}

	return builder->AddDescriptorsToCache();
}


EntryHandle RenderInstance::CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo* info, int* offsets)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle descHandle = CreateShaderResourceSet(info->descriptorsetid);

	VKGraphicsPipelineObjectCreateInfo create = {
			.drawType = info->drawType,
			.vertexBufferIndex = allocations[info->vertexBufferIndex].memIndex,
			.vertexBufferOffset = static_cast<uint32_t>(allocations[info->vertexBufferIndex].offset),
			.vertexCount = info->vertexCount,
			.indirectDrawBuffer = allocations[info->indirectDrawBuffer].memIndex,
			.indirectDrawOffset = static_cast<uint32_t>(allocations[info->indirectDrawBuffer].offset),
			.pipelinename = EntryHandle(),
			.descriptorsetid = descHandle,
			.maxDynCap = info->maxDynCap,
			.data = nullptr,
			.indexBufferHandle = allocations[info->indexBufferHandle].memIndex,
			.indexBufferOffset = static_cast<uint32_t>(allocations[info->indexBufferHandle].offset),
			.indexCount = info->indexCount,
			.pushRangeCount = info->pushRangeCount
	};

	auto& ref = pipelinesIdentifier[info->pipelinename];

	for (uint32_t i = 0; i < maxMSAALevels; i++) {

		create.pipelinename = ref[i];

		EntryHandle pipelineIndex = dev->CreateGraphicsPipelineObject(&create);

		VKPipelineObject* VKGraphicsPipelineObject = dev->GetPipelineObject(pipelineIndex);

		for (uint32_t j = 0; j < info->maxDynCap; j++)
		{
			VKGraphicsPipelineObject->SetPerObjectData(static_cast<uint32_t>(allocations.allocations[offsets[j]].offset));
		}

		for (uint32_t i = 0; i < info->pushRangeCount; i++)
		{
			ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid, i);

			if (pushArgs) {
				VKGraphicsPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
			}
		}

		AddVulkanMemoryBarrier(VKGraphicsPipelineObject, info->descriptorsetid);

		auto graph = dev->GetRenderGraph(swapchainRenderTargets[i]);
		graph->AddObject(pipelineIndex);
	}

	return EntryHandle();
}

EntryHandle RenderInstance::CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info, int* offsets)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle descHandle = CreateShaderResourceSet(info->descriptorsetid);

	VKComputePipelineObjectCreateInfo create = {
		.x = info->x,
		.y = info->y,
		.z = info->z,
		.descriptorId = descHandle,
		.pipelineId = pipelinesIdentifier[info->pipelinename][0],
		.maxDynCap = info->maxDynCap,
		.data = nullptr,
		.barrierCount = info->barrierCount,
		.pushRangeCount = info->pushRangeCount
	};


	EntryHandle pipelineIndex = dev->CreateComputePipelineObject(&create);

	VKPipelineObject* vkPipelineObject = dev->GetPipelineObject(pipelineIndex);

	for (uint32_t i = 0; i < info->maxDynCap; i++)
	{
		vkPipelineObject->SetPerObjectData(static_cast<uint32_t>(allocations.allocations[offsets[i]].offset));
	}

	for (uint32_t i = 0; i < info->pushRangeCount; i++)
	{
		ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid, i);

		if (pushArgs) {
			vkPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
		}
	}
	
	AddVulkanMemoryBarrier(vkPipelineObject, info->descriptorsetid);
	
	auto graph = dev->GetComputeGraph(computeGraphIndex);
	graph->AddObject(pipelineIndex);

	return pipelineIndex;
}

void RenderInstance::AddVulkanMemoryBarrier(VKPipelineObject *vkPipelineObject, int descriptorid)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	int popCounter = 0;
	ShaderResourceHeader* header = PopShaderResourceBarrier(descriptorid, &popCounter);

	while (header)
	{
		switch (header->type)
		{
		case IMAGESTORE2D:
		{
			ShaderResourceImage* imageBarrier = (ShaderResourceImage*)header;
			ImageShaderResourceBarrier* barrier = (ImageShaderResourceBarrier*)(imageBarrier + 1);
			ImageShaderResourceBarrier* barrier2 = &barrier[1];

			VkImageSubresourceRange range{};
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.levelCount = 1;
			range.layerCount = 1;

			VkAccessFlags srcAction = ConvertResourceActionToVulkan(barrier->srcAction);
			VkAccessFlags dstAction = ConvertResourceActionToVulkan(barrier->dstAction);
			VkShaderStageFlags srcStage = ConvertResourceStageToVulkan(barrier->srcStage);
			VkShaderStageFlags dstStage = ConvertResourceStageToVulkan(barrier->dstStage);

			if (barrier->dstResourceLayout != barrier->srcResourceLayout)
			{

				EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, barrier->srcResourceLayout, 
																			barrier->dstResourceLayout, imageBarrier->textureHandle, range);
				vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
					BEFORE,
					srcStage,
					dstStage
				);
			}


			srcAction = ConvertResourceActionToVulkan(barrier2->srcAction);
			dstAction = ConvertResourceActionToVulkan(barrier2->dstAction);
			srcStage = ConvertResourceStageToVulkan(barrier2->srcStage);
			dstStage = ConvertResourceStageToVulkan(barrier2->dstStage);

			if (barrier2->dstResourceLayout != barrier2->srcResourceLayout)
			{


				EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, barrier2->srcResourceLayout,
					barrier2->dstResourceLayout, imageBarrier->textureHandle, range);

				vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
					AFTER,
					srcStage,
					dstStage
				);
			}

			break;
		}
		case SAMPLER:
		{
			ShaderResourceImage* imageBarrier = (ShaderResourceImage*)header;

			ImageShaderResourceBarrier* barrier = (ImageShaderResourceBarrier*)(imageBarrier + 1);

			VkImageSubresourceRange range{};

			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.levelCount = 1;
			range.layerCount = 1;

			VkAccessFlags srcAction = ConvertResourceActionToVulkan(barrier->srcAction);
			VkAccessFlags dstAction = ConvertResourceActionToVulkan(barrier->dstAction);
			VkShaderStageFlags srcStage = ConvertResourceStageToVulkan(barrier->srcStage);
			VkShaderStageFlags dstStage = ConvertResourceStageToVulkan(barrier->dstStage);

			if (barrier->dstResourceLayout != barrier->srcResourceLayout)
			{

				EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, barrier->srcResourceLayout,
					barrier->dstResourceLayout, imageBarrier->textureHandle, range);
				vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
					BEFORE,
					srcStage,
					dstStage
				);
			}

			break;
		}
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:
		{
			ShaderResourceBuffer* bufferBarrier = (ShaderResourceBuffer*)header;
			ShaderResourceBarrier* barrier = (ShaderResourceBarrier*)(bufferBarrier + 1);

			VkAccessFlags srcAction = ConvertResourceActionToVulkan(barrier->srcAction);
			VkAccessFlags dstAction = ConvertResourceActionToVulkan(barrier->dstAction);
			VkShaderStageFlags srcStage = ConvertResourceStageToVulkan(barrier->srcStage);
			VkShaderStageFlags dstStage = ConvertResourceStageToVulkan(barrier->dstStage);

			EntryHandle barrierIndex = dev->CreateBufferMemoryBarrier(srcAction, dstAction, 0, 0, allocations[bufferBarrier->allocation].memIndex,
				allocations[bufferBarrier->allocation].size,
				allocations[bufferBarrier->allocation].offset);


			vkPipelineObject->AddBufferMemoryBarrier(
				barrierIndex,
				AFTER,
				srcStage,
				dstStage
			);

			break;
		}
		}

		header = PopShaderResourceBarrier(descriptorid, &popCounter);
	}
}


ShaderResourceHeader* RenderInstance::PopShaderResourceBarrier(int descriptorSet, int* counter)
{
	int i = *counter;
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));
	while (i < set->bindingCount)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];
		*counter = ++i;
		if (header->action & SHADERWRITE)
		{
			return (ShaderResourceHeader*)header;
		}
	}

	return nullptr;
}


void RenderInstance::DrawScene(EntryHandle cbindex, uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);

	auto graph = dev->GetRenderGraph(swapchainRenderTargets[currentMSAALevel]);
	auto cGraph = dev->GetComputeGraph(computeGraphIndex);
	//if (!graph.objects.size()) return;
	auto rcb = dev->GetRecordingBufferObject(cbindex);

	cGraph->DispatchWork(&rcb, imageIndex);

	graph->DrawScene(&rcb, imageIndex, &swc->swapChainExtent);
}

void RenderInstance::InvalidateRecordBuffer(uint32_t i)
{
	threadedRecordBuffers[i].Invalidate();
}




void RenderInstance::IncreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);
	uint32_t next = currentMSAALevel + 1;
	if (next >= maxMSAALevels)
		return;
	currentMSAALevel = next;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		threadedRecordBuffers[i].Reset();
}

void RenderInstance::DecreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);
	int32_t next = currentMSAALevel - 1;
	if (next < 0)
		return;
	currentMSAALevel = next;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		threadedRecordBuffers[i].Reset();
}
