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

struct DescriptorSet
{
	int bindingCount;
	int descriptorLayoutHandle;
	int setCount;
};

struct DescriptorHeader
{
	ShaderResourceType type;
	ShaderResourceAction action;
	int binding;
};

struct DescriptorImage : public DescriptorHeader
{
	EntryHandle textureHandle;
};

struct DescriptorBuffer : public DescriptorHeader
{
	int allocation;
	bool direct;
};

struct DescriptorConstantBuffer : public DescriptorHeader
{
	ShaderStageType stage;
	int size;
	int offset;
	void* data;
};

struct DescriptorBarrier
{
	MemoryBarrierType type;
	BarrierStage srcStage;
	BarrierStage dstStage;
	BarrierAction srcAction;
	BarrierAction dstAction;
};

struct ImageDescriptorBarrier : public DescriptorBarrier
{
	VkImageLayout srcResourceLayout;
	VkImageLayout dstResourceLayout;
	VkImageAspectFlags imageType;
};


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
		}
		return vkFormat;
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
	renderInst->SetResizeBool(true);
}

#define KB 1024
#define MB 1024 * KB
#define GB 1024 * MB

RenderInstance::RenderInstance()
	:
	descriptorSets{},
	shaderGraphPtrs{}
{
	vkInstance = new VKInstance();
	shaderGraphs = (uintptr_t)malloc(1 * KB);
	descriptorResourceSet = (uintptr_t)malloc(1 * KB);

	if (!shaderGraphs || !descriptorResourceSet)
	{
		throw std::runtime_error("cannot allocate render instance");
	}
	
	shaderGraphOffset = sizeof(ShaderGraphsHolder);
	descriptorResourceOffset = 0;

	ShaderGraphsHolder* holder = (ShaderGraphsHolder*)shaderGraphs;
	holder->graphCount = 3;
};

uintptr_t RenderInstance::AllocateShaderGraph(uint32_t shaderMapCount, uint32_t *shaderResourceCount,  ShaderStageType *types, uint32_t *shaderReferences)
{
	uintptr_t ret = (shaderGraphs + shaderGraphOffset);
	ShaderGraph* graph = (ShaderGraph*)(ret);

	graph->shaderMapCount = shaderMapCount;
	graph->resourceSetCount = 2;
	shaderGraphOffset += sizeof(ShaderGraph) + (graph->resourceSetCount * sizeof(ShaderResourceSet));

	for (uint32_t i = 0; i < shaderMapCount; i++)
	{
		ShaderMap* map = (ShaderMap*)(shaderGraphs + shaderGraphOffset);
		map->type = types[i];
		map->shaderReference = shaderReferences[i];
		map->resourceCount = shaderResourceCount[i];
		shaderGraphOffset += sizeof(ShaderMap) + (map->resourceCount * sizeof(ShaderResource));
	}

	return ret;
}

RenderInstance::~RenderInstance()
{
	free((void*)shaderGraphs);
	free((void*)descriptorResourceSet);

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
		dev->DestroyDescriptorLayout(i);
	}

	dev->DestroyDescriptorPool(descriptorPoolIndex);

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

	depthImages[index] = dev->CreateImage(width, height,
		1, depthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		sampleCount,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, attachmentsIndex);

	depthViews[index] = dev->CreateImageView(depthImages[index], 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	dev->TransitionImageLayout(
		depthImages[index], depthFormat,
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

		rpb.CreateAttachment(VKRenderPassBuilder::DEPTHSTENCILATTACH, depthFormat, sampleCount,
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

		rpb.CreateAttachment(VKRenderPassBuilder::DEPTHSTENCILATTACH, depthFormat, sampleCount,
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
	static int index = 0;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	std::array<DescriptorSetLayoutBuilder*, 5> descriptorBuilders{};

	for (int j = 0; j < graph->resourceSetCount; j++)
	{
		ShaderResourceSet* set = (ShaderResourceSet*)graph->GetSet(j);
		descriptorBuilders[j] = dev->CreateDescriptorSetLayoutBuilder(set->bindingCount);
	}

	int count = graph->resourceSetCount;

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
		descriptorLayouts[index++] = dev->CreateDescriptorSetLayout(descriptorBuilders[j]);
	}

}

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	std::array<ShaderResource, 10> resourcesArr = { {
		{ SHADERREAD, UNIFORM_BUFFER, 0, 0 },
		{ SHADERREAD, UNIFORM_BUFFER, 1, 0 },
		{ SHADERREAD, SAMPLER, 1, 1 },
		{ SHADERREAD, SAMPLER, 0, 0},
		{ SHADERREAD, CONSTANT_BUFFER, 0, ~0},
		{ SHADERREAD, STORAGE_BUFFER, 0, 0 },
		{ SHADERREAD, STORAGE_BUFFER, 0, 1 },
		{ SHADERWRITE, STORAGE_BUFFER, 0, 2 },
		{ SHADERREAD, UNIFORM_BUFFER, 0, 0},
		{ SHADERWRITE, IMAGESTORE2D, 0, 1}
	} };

	std::array shaders1 = { "3dtextured.vert.spv", "3dtextured.frag.spv", "text.vert.spv" , "text.frag.spv", "mesh_interpolate.comp.spv", "polynomialimage.comp.spv"};

	std::array shaderReferences = { 0U, 1U, 2U, 3U, 4U, 5U };
	std::array<ShaderStageType, 6> shaderTypes  = {
		ShaderStageTypeBits::VERTEXSHADERSTAGE, 
		ShaderStageTypeBits::FRAGMENTSHADERSTAGE, 
		ShaderStageTypeBits::VERTEXSHADERSTAGE, 
		ShaderStageTypeBits::FRAGMENTSHADERSTAGE, 
		ShaderStageTypeBits::COMPUTESHADERSTAGE,
		ShaderStageTypeBits::COMPUTESHADERSTAGE
	};
	std::array shaderResourceCounts = { 2U, 1U, 0U, 1U, 4U, 2U };

	std::array setSizes = { 2, 1, 1, 1 };

	std::array bindingIndex = { 0, 2, 3, 4 };

	std::array bindingCount = { 1, 2, 1, 4, 2 };

	shaderGraphPtrs[0] = (ShaderGraph*)AllocateShaderGraph(2, &shaderResourceCounts[0], &shaderTypes[0], &shaderReferences[0]);
	shaderGraphPtrs[1] = (ShaderGraph*)AllocateShaderGraph(2, &shaderResourceCounts[2], &shaderTypes[2], &shaderReferences[2]);
	shaderGraphPtrs[2] = (ShaderGraph*)AllocateShaderGraph(1, &shaderResourceCounts[4], &shaderTypes[4], &shaderReferences[4]);
	shaderGraphPtrs[3] = (ShaderGraph*)AllocateShaderGraph(1, &shaderResourceCounts[5], &shaderTypes[5], &shaderReferences[5]);

	uint32_t resourceCount = 0;

	for (int i = 0; i < 4; i++)
	{
		

		for (int z = 0; z < setSizes[i]; z++)
		{
			ShaderResourceSet* set = (ShaderResourceSet*)shaderGraphPtrs[i]->GetSet(z);
			set->bindingCount = bindingCount[bindingIndex[i]+z];
		}

		for (int j = 0; j < shaderGraphPtrs[i]->shaderMapCount; j++)
		{
			
			ShaderMap* map = (ShaderMap*)shaderGraphPtrs[i]->GetMap(j);
			for (int h = 0; h < map->resourceCount; h++)
			{
				ShaderResource* resource = (ShaderResource*)map->GetResource(h);
				resource->action = resourcesArr[resourceCount].action;
				resource->type = resourcesArr[resourceCount].type;
				resource->binding = resourcesArr[resourceCount].binding;
				resource->set = resourcesArr[resourceCount].set;
				resourceCount++;
			}
		}

		shaderGraphPtrs[i]->resourceSetCount = setSizes[i];

		CreateShaderResourceMap(shaderGraphPtrs[i]);
	}

	

	size_t counter = 0;

	for (size_t i = 0; i < shaders1.size(); i++)
	{
		std::string name = shaders1[i];
		std::vector<char> buffer;
		if (FileManager::FileExists(name)) {

			auto ret = FileManager::ReadFileInFull(name, buffer);
		}
		else
		{
			std::string uncompiled = name.substr(0, name.length() - 4);

			auto ret = FileManager::ReadFileInFull(uncompiled, buffer);

			if (buffer.back() != '\0') buffer.push_back('\0');
		}

		shaders[counter++] = dev->CreateShader(buffer.data(), buffer.size(), dev->ConvertShaderFlags(name));
	}

	auto computePipeline = dev->CreateComputePipelineBuilder(1, 1);

	computePipeline->AddPushConstantRange(0, sizeof(float), VK_SHADER_STAGE_COMPUTE_BIT, 0);

	pipelinesIdentifier[MESH_INTERPOLATE] = std::vector<EntryHandle>(1, computePipeline->CreateComputePipeline(&descriptorLayouts[3], 1, shaders[4]));

	auto polyPipeline = dev->CreateComputePipelineBuilder(1, 0);

	pipelinesIdentifier[POLY] = std::vector<EntryHandle>(1, computePipeline->CreateComputePipeline(&descriptorLayouts[4], 1, shaders[5]));

	std::vector<EntryHandle> l(maxMSAALevels);
	std::vector<EntryHandle> r(maxMSAALevels);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		auto genericBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 2, 2, 0);
		auto textBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 1, 2, 0);

		UsePipelineBuilders(genericBuilder, textBuilder, (VkSampleCountFlagBits)(1<<i));

		r[i] = genericBuilder->CreateGraphicsPipeline(&descriptorLayouts[0], 2, &shaders[0], 2);

		l[i] = textBuilder->CreateGraphicsPipeline(&descriptorLayouts[2], 1, &shaders[2], 2);

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
	size_t location = dev->GetMemoryFromBuffer(globalIndex, size, alignment);

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



void RenderInstance::AllocateVectorsForMSAA()
{
	swapchainRenderTargets.resize(maxMSAALevels);
	depthViews.resize(maxMSAALevels);
	colorViews.resize(maxMSAALevels-1);
	depthImages.resize(maxMSAALevels);
	colorImages.resize(maxMSAALevels-1);
	renderPasses.resize(maxMSAALevels);
}


int RenderInstance::AllocateDescriptorSet(uint32_t shaderGraphIndex, uint32_t targetSet, int index, int setCount)
{
	uintptr_t head = descriptorResourceSet + descriptorResourceOffset;
	uintptr_t ptr = head;
	DescriptorSet* set = (DescriptorSet*)ptr;
	ptr += sizeof(DescriptorSet);

	ShaderResourceSet* resourceSet = (ShaderResourceSet*)shaderGraphPtrs[shaderGraphIndex]->GetSet(targetSet);

	set->bindingCount = resourceSet->bindingCount;
	set->descriptorLayoutHandle = index;
	set->setCount = setCount;

	

	uintptr_t* offset = (uintptr_t*)ptr;

	ptr += sizeof(uintptr_t) * (set->bindingCount);

	for (int j = 0; j < shaderGraphPtrs[shaderGraphIndex]->shaderMapCount; j++)
	{

		ShaderMap* map = (ShaderMap*)shaderGraphPtrs[shaderGraphIndex]->GetMap(j);
		int constantCount = map->resourceCount-1;
		for (int h = 0; h < map->resourceCount; h++)
		{
			MemoryBarrierType memBarrierType = MEMORY_BARRIER;

			ShaderResource* resource = (ShaderResource*)map->GetResource(h);

			if (resource->set != targetSet) continue;

			DescriptorHeader* desc = (DescriptorHeader*)ptr;

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
				ptr += sizeof(DescriptorImage);
				memBarrierType = IMAGE_BARRIER;
				if (resource->action & SHADERWRITE)
				{
					ImageDescriptorBarrier* barriers = (ImageDescriptorBarrier*)ptr;
					barriers->dstStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->dstAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
					ptr += (sizeof(ImageDescriptorBarrier));

					ImageDescriptorBarrier* barriers2 = (ImageDescriptorBarrier*)ptr;
					barriers2->srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers2->srcAction = WRITE_SHADER_RESOURCE;
					barriers2->type = memBarrierType;
					ptr += (sizeof(ImageDescriptorBarrier));
				}
				break;
			}
			case SAMPLER:
			{
				ptr += sizeof(DescriptorImage);
				memBarrierType = IMAGE_BARRIER;
				if (resource->action & SHADERWRITE)
				{
					ImageDescriptorBarrier* barriers = (ImageDescriptorBarrier*)ptr;
					barriers->srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->srcAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
					ptr += (sizeof(ImageDescriptorBarrier));
				}
				break;
			}
			case CONSTANT_BUFFER:
			{
				DescriptorConstantBuffer* constants = (DescriptorConstantBuffer*)ptr;
				constants->size = 4;
				constants->offset = 0;
				constants->stage = map->type;
				ptr += sizeof(DescriptorConstantBuffer);
				break;
			}
			case STORAGE_BUFFER:
			case UNIFORM_BUFFER:
			{
				memBarrierType = BUFFER_BARRIER;
				ptr += sizeof(DescriptorBuffer);
				if (resource->action & SHADERWRITE)
				{
					DescriptorBarrier* barriers = (DescriptorBarrier*)ptr;
					barriers->srcStage = ConvertShaderStageToBarrierStage(map->type);
					barriers->srcAction = WRITE_SHADER_RESOURCE;
					barriers->type = memBarrierType;
					ptr += (sizeof(DescriptorBarrier));
				}
				break;
			}
			}

			
		}
	}


	

	int indexRet = descriptorSetIndex.fetch_add(1);

	descriptorSets[indexRet] = head;
	descriptorResourceOffset += (uint32_t)(ptr - head);

	return indexRet;
}


void RenderInstance::BindBufferToDescriptor(int descriptorSet, int allocationIndex, bool direct,  int bindingIndex)
{
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));

	DescriptorBuffer* header = (DescriptorBuffer*)offsets[bindingIndex];

	if (header->type != UNIFORM_BUFFER && header->type != STORAGE_BUFFER)
		return;

	header->direct = direct;
	header->allocation = allocationIndex;
}

void RenderInstance::BindSampledImageToDescriptor(int descriptorSet, EntryHandle index, int bindingIndex)
{
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));

	DescriptorImage* header = (DescriptorImage*)offsets[bindingIndex];

	if (header->type != SAMPLER && header->type != IMAGESTORE2D)
		return;

	header->textureHandle = index;
}

void RenderInstance::BindBarrier(int descriptorSet, int binding, BarrierStage stage, BarrierAction action)
{
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));


	head = offsets[binding];
	DescriptorHeader* desc = (DescriptorHeader*)offsets[binding];



	switch (desc->type)
	{
	case IMAGESTORE2D:
	case SAMPLER:
		head += sizeof(DescriptorImage);
		
		break;
	case STORAGE_BUFFER:
	case UNIFORM_BUFFER:
	
		head += sizeof(DescriptorBuffer);
		break;
	}

	DescriptorBarrier* barrier = (DescriptorBarrier*)head;

	barrier->dstAction = action;
	barrier->dstStage = stage;
}

void RenderInstance::BindImageBarrier(int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, VkImageLayout oldLayout, VkImageLayout dstLayout, bool location)
{
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));


	head = offsets[binding];
	DescriptorHeader* desc = (DescriptorHeader*)offsets[binding];

	if (desc->type != SAMPLER && desc->type != IMAGESTORE2D)
		return;

	switch (desc->type)
	{
	case IMAGESTORE2D:
	case SAMPLER:
		head += sizeof(DescriptorImage);

		break;
	case STORAGE_BUFFER:
	case UNIFORM_BUFFER:

		head += sizeof(DescriptorBuffer);
		break;
	}


	ImageDescriptorBarrier* imageBarrier = (ImageDescriptorBarrier*)head;


	imageBarrier[barrierIndex].imageType = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier[barrierIndex].dstResourceLayout = dstLayout;
	imageBarrier[barrierIndex].srcResourceLayout = oldLayout;

	if (location)
	{
		imageBarrier[barrierIndex].dstAction = action;
		imageBarrier[barrierIndex].dstStage = stage;
	}
	else {
		imageBarrier[barrierIndex].srcAction = action;
		imageBarrier[barrierIndex].srcStage = stage;
	}
}

void RenderInstance::UploadConstant(int descriptorset, void* data, int bufferLocation)
{
	DescriptorConstantBuffer* header = (DescriptorConstantBuffer*)GetConstantBuffer(descriptorset, bufferLocation);
	if (!header) return;
	header->data = data;
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
	currentMSAALevel = maxMSAALevels-1;

	AllocateVectorsForMSAA();

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

	depthFormat = VK::Utils::findSupportedFormat(majorDevice->gpu,
		formats.data(),
		formats.size(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice->CreateHostBuffer(64 * MB, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);


	
	swapChainIndex = majorDevice->CreateSwapChain(3, MAX_FRAMES_IN_FLIGHT, 1, 2, maxMSAALevels);
	

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
	
	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		CreateRenderPass(i, (VkSampleCountFlagBits)(1 << i));
	}

	CreateSwapChain(800, 600, false);
	


	DescriptorPoolBuilder builder = majorDevice->CreateDescriptorPoolBuilder(3);
	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddStoragePoolSize(MAX_FRAMES_IN_FLIGHT * 100);

	descriptorPoolIndex = majorDevice->CreateDesciptorPool(builder, MAX_FRAMES_IN_FLIGHT * 101);

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
	int mrpDescHandle = AllocateDescriptorSet(0, 0, 0, MAX_FRAMES_IN_FLIGHT);

	BindBufferToDescriptor(mrpDescHandle, perRenderPassStuff, false, 0);

	EntryHandle mrpDescId = CreateDescriptorSet(mrpDescHandle);

	std::array<uint32_t, 1> data = { (uint32_t)allocations[perRenderPassStuff].offset };

	for (uint32_t i = 0; i<maxMSAALevels; i++)

		majorDevice->UpdateRenderGraph(swapchainRenderTargets[i], data.data(), (uint32_t)data.size(), mrpDescId);
	

	return perRenderPassStuff;
}


void RenderInstance::SetResizeBool(bool set)
{
	resizeWindow = set;
}

uint32_t RenderInstance::GetCurrentFrame() const
{
	return currentFrame;
}

uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

EntryHandle RenderInstance::CreateDescriptorSet(int descriptorSet)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));

	int frames = set->setCount;

	DescriptorSetBuilder* builder = dev->CreateDescriptorSetBuilder(descriptorPoolIndex, descriptorLayouts[set->descriptorLayoutHandle], frames);

	int count = set->bindingCount;

	for (int i = 0; i < count; i++)
	{
		DescriptorHeader* header = (DescriptorHeader*)offsets[i];

		switch (header->type)
		{
			case IMAGESTORE2D:
			{
				DescriptorImage* image = (DescriptorImage*)offsets[i];
				builder->AddStorageImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case SAMPLER:
			{
				DescriptorImage* image = (DescriptorImage*)offsets[i];
				builder->AddPixelShaderImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), dev->GetSamplerByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case STORAGE_BUFFER:
			{
				DescriptorBuffer* buffer = (DescriptorBuffer*)offsets[i];
				auto alloc = allocations[buffer->allocation];
				if (!buffer->direct)
					builder->AddDynamicStorageBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.size / frames, i, frames, 0);
				else
					builder->AddDynamicStorageBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.size, i, frames, 0);
				break;
			}
			case UNIFORM_BUFFER:
			{
				DescriptorBuffer* buffer = (DescriptorBuffer*)offsets[i];
				auto alloc = allocations[buffer->allocation];
				if (!buffer->direct)
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

	EntryHandle descHandle = CreateDescriptorSet(info->descriptorsetid);

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
			DescriptorConstantBuffer* pushArgs = (DescriptorConstantBuffer*)GetConstantBuffer(info->descriptorsetid, i);

			if (pushArgs) {
				VKGraphicsPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
			}
		}

		auto graph = dev->GetRenderGraph(swapchainRenderTargets[i]);
		graph->AddObject(pipelineIndex);
	}

	return EntryHandle();
}

EntryHandle RenderInstance::CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info, int* offsets)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle descHandle = CreateDescriptorSet(info->descriptorsetid);

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
		DescriptorConstantBuffer* pushArgs = (DescriptorConstantBuffer*)GetConstantBuffer(info->descriptorsetid, i);

		if (pushArgs) {
			vkPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
		}
	}

	
		
	int popCounter = 0;
	DescriptorHeader* header = PopDescriptorBarrier(info->descriptorsetid, &popCounter);
	while (header)
	{
		switch (header->type)
		{
		case IMAGESTORE2D:
		{
			DescriptorImage* imageBarrier = (DescriptorImage*)header;
			ImageDescriptorBarrier* barrier = (ImageDescriptorBarrier*)(imageBarrier+1);
			ImageDescriptorBarrier* barrier2 = &barrier[1];
			
			VkImageSubresourceRange range{};
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.levelCount = 1;
			range.layerCount = 1;

			if (barrier->dstResourceLayout != barrier->srcResourceLayout)
			{


				vkPipelineObject->AddImageMemoryBarrier(dev,
					BEFORE,
					imageBarrier->textureHandle,
					ConvertResourceActionToVulkan(barrier->srcAction),
					ConvertResourceActionToVulkan(barrier->dstAction),
					ConvertResourceStageToVulkan(barrier->srcStage),
					ConvertResourceStageToVulkan(barrier->dstStage),
					barrier->srcResourceLayout,
					barrier->dstResourceLayout,
					range
				);
			}


			if (barrier2->dstResourceLayout != barrier2->srcResourceLayout)
			{


				vkPipelineObject->AddImageMemoryBarrier(dev,
					AFTER,
					imageBarrier->textureHandle,
					ConvertResourceActionToVulkan(barrier2->srcAction),
					ConvertResourceActionToVulkan(barrier2->dstAction),
					ConvertResourceStageToVulkan(barrier2->srcStage),
					ConvertResourceStageToVulkan(barrier2->dstStage),
					barrier2->srcResourceLayout,
					barrier2->dstResourceLayout,
					range
				);
			}

			break;
		}
		case SAMPLER:
		{
			DescriptorImage* imageBarrier = (DescriptorImage*)header;
			ImageDescriptorBarrier* barrier = (ImageDescriptorBarrier*)(imageBarrier+1);
			VkImageSubresourceRange range{};
			range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			range.levelCount = 1;
			range.layerCount = 1;
			vkPipelineObject->AddImageMemoryBarrier(dev,
				BEFORE,
				imageBarrier->textureHandle,
				ConvertResourceActionToVulkan(barrier->srcAction),
				ConvertResourceActionToVulkan(barrier->dstAction),
				ConvertResourceStageToVulkan(barrier->srcStage),
				ConvertResourceStageToVulkan(barrier->dstStage),
				barrier->srcResourceLayout,
				barrier->dstResourceLayout,
				range
			);

			break;
		}
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:
		{
			DescriptorBuffer* bufferBarrier = (DescriptorBuffer*)header;
			DescriptorBarrier* barrier = (DescriptorBarrier*)(bufferBarrier + 1);
			vkPipelineObject->AddBufferMemoryBarrier(
				dev, 
				AFTER,
				allocations[bufferBarrier->allocation].memIndex, 
				allocations[bufferBarrier->allocation].size, 
				allocations[bufferBarrier->allocation].offset,
				ConvertResourceActionToVulkan(barrier->srcAction),
				ConvertResourceActionToVulkan(barrier->dstAction),
				ConvertResourceStageToVulkan(barrier->srcStage),
				ConvertResourceStageToVulkan(barrier->dstStage)
			);
			break;
		}
		}

		header = PopDescriptorBarrier(info->descriptorsetid, &popCounter);
	}
	


	auto graph = dev->GetComputeGraph(computeGraphIndex);
	graph->AddObject(pipelineIndex);

	return pipelineIndex;
}


DescriptorHeader* RenderInstance::PopDescriptorBarrier(int descriptorSet, int* counter)
{
	int i = *counter;
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));
	while (i < set->bindingCount)
	{
		DescriptorHeader* header = (DescriptorHeader*)offsets[i];
		*counter = ++i;
		if (header->action & SHADERWRITE)
		{
			return (DescriptorHeader*)header;
		}
	}

	return nullptr;
}

DescriptorHeader* RenderInstance::GetConstantBuffer(int descriptorSet, int constantBuffer)
{
	uintptr_t head = descriptorSets[descriptorSet];
	DescriptorSet* set = (DescriptorSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(DescriptorSet));

	DescriptorHeader* ret = (DescriptorHeader*)(offsets[set->bindingCount - (constantBuffer+1)]);

	if (ret->type != CONSTANT_BUFFER) return nullptr;

	return ret;
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
