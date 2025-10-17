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
{
	vkInstance = new VKInstance();
	shaderGraphs = (uintptr_t)malloc(1 * KB);
	shaderGraphOffset = sizeof(ShaderGraphsHolder);

	ShaderGraphsHolder* holder = (ShaderGraphsHolder*)shaderGraphs;
	holder->graphCount = 3;
};

uintptr_t RenderInstance::AllocateShaderGraph(uint32_t shaderMapCount, uint32_t *shaderResourceCount,  ShaderStageType *types, uint32_t *shaderReferences)
{
	uintptr_t ret = (shaderGraphs + shaderGraphOffset);
	ShaderGraph* graph = (ShaderGraph*)(ret);
	graph->shaderMapCount = shaderMapCount;
	shaderGraphOffset += sizeof(ShaderGraph);
	for (int i = 0; i < shaderMapCount; i++)
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
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

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
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

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

void RenderInstance::CreatePipelines()
{
	// Create Shaders

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);


	DescriptorSetLayoutBuilder* textDescriptor = dev->CreateDescriptorSetLayoutBuilder(1);
	DescriptorSetLayoutBuilder* globalBufferBuilder = dev->CreateDescriptorSetLayoutBuilder(1);
	DescriptorSetLayoutBuilder* genericObjectBuilder = dev->CreateDescriptorSetLayoutBuilder(2);
	DescriptorSetLayoutBuilder* computeBuilder = dev->CreateDescriptorSetLayoutBuilder(3);
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

	computeBuilder->AddDynamicStorageBufferLayout(0, VK_SHADER_STAGE_COMPUTE_BIT);

	computeBuilder->AddDynamicStorageBufferLayout(1, VK_SHADER_STAGE_COMPUTE_BIT);

	computeBuilder->AddDynamicStorageBufferLayout(2, VK_SHADER_STAGE_COMPUTE_BIT);

	descriptorLayouts["compute"] = dev->CreateDescriptorSetLayout(computeBuilder);


	std::array<ShaderResource, 7> resourcesArr = { {
		{ SHADERREAD, UNIFORM_BUFFER, 0, 0 },
		{ SHADERREAD, UNIFORM_BUFFER, 1, 0 },
		{ SHADERREAD, SAMPLER, 1, 1 },
		{ SHADERREAD, SAMPLER, 0, 0},
		{ SHADERREAD, STORAGE_BUFFER, 0, 0 },
		{ SHADERREAD, STORAGE_BUFFER, 0, 1 },
		{ SHADERWRITE, STORAGE_BUFFER, 0, 2 },
	} };

	std::vector<std::string> shaders1 = { "3dtextured.vert.spv", "3dtextured.frag.spv", "text.vert.spv" , "text.frag.spv", "mesh_interpolate.comp.spv" };

	std::array shaderReferences = { 0U, 1U, 2U, 3U, 4U };
	std::array shaderTypes = { ShaderStageType::VERTEXSTAGE, ShaderStageType::FRAGMENTSTAGE, ShaderStageType::VERTEXSTAGE, ShaderStageType::FRAGMENTSTAGE, ShaderStageType::COMPUTESTAGE };
	std::array shaderResourceCounts = { 2U, 1U, 0U, 1U, 3U };

	std::array<ShaderGraph*, 3> maps;
	maps[0] = (ShaderGraph*)AllocateShaderGraph(2, &shaderResourceCounts[0], &shaderTypes[0], &shaderReferences[0]);
	maps[1] = (ShaderGraph*)AllocateShaderGraph(2, &shaderResourceCounts[2], &shaderTypes[2], &shaderReferences[2]);
	maps[2] = (ShaderGraph*)AllocateShaderGraph(1, &shaderResourceCounts[4], &shaderTypes[4], &shaderReferences[4]);

	uint32_t resourceCount = 0;

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < maps[i]->shaderMapCount; j++)
		{
			ShaderMap* map = (ShaderMap*)maps[i]->GetMap(j);
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

	std::array compDescHandles = { descriptorLayouts["compute"] };

	computePipeline->AddPushConstantRange(0, sizeof(float), VK_SHADER_STAGE_COMPUTE_BIT, 0);

	pipelinesIdentifier[MESH_INTERPOLATE] = std::vector<EntryHandle>(1, computePipeline->CreateComputePipeline(compDescHandles.data(), 1, shaders[4]));

	std::vector<EntryHandle> l(maxMSAALevels);
	std::vector<EntryHandle> r(maxMSAALevels);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		auto genericBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 2, 2, 0);
		auto textBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 1, 2, 0);

		UsePipelineBuilders(genericBuilder, textBuilder, (VkSampleCountFlagBits)(1<<i));

		r[i] = genericBuilder->CreateGraphicsPipeline(rmcIDs.data(), rmcIDs.size(), &shaders[0], 2);

		l[i] = textBuilder->CreateGraphicsPipeline(tdsIDs.data(), tdsIDs.size(), &shaders[2], 2);

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

size_t RenderInstance::GetPageFromUniformBuffer(size_t size, uint32_t alignment)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	size_t location = dev->GetMemoryFromBuffer(globalIndex, size, alignment);

	size_t index = allocationsIndex.fetch_add(1);
	allocations.allocations[index].memIndex = globalIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].size = size;


	return index;
}

size_t RenderInstance::GetPageFromDeviceBuffer(size_t size, uint32_t alignment)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	size_t location = dev->GetMemoryFromBuffer(globalDeviceBufIndex, size, alignment);

	size_t index = allocationsIndex.fetch_add(1);
	allocations.allocations[index].memIndex = globalDeviceBufIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].size = size;


	return index;
}

VkBuffer RenderInstance::GetDynamicUniformBuffer()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetBufferHandle(globalIndex);
}

VkBuffer RenderInstance::GetDeviceBufferHandle()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetBufferHandle(globalDeviceBufIndex);
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



void RenderInstance::AllocateVectorsForMSAA()
{
	swapchainRenderTargets.resize(maxMSAALevels);
	depthViews.resize(maxMSAALevels);
	colorViews.resize(maxMSAALevels-1);
	depthImages.resize(maxMSAALevels);
	colorImages.resize(maxMSAALevels-1);
	renderPasses.resize(maxMSAALevels);
}


#define KB 1024
#define MB 1024 * KB
#define GB 1024 * MB


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
		//ref.DrawMain();
		ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ThreadedRecordBuffer<MAX_FRAMES_IN_FLIGHT>::DrawLoop),
				&ref, std::placeholders::_1));
	}
}

size_t RenderInstance::CreateRenderGraph(size_t datasize, size_t alignment)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	DescriptorSetBuilder *dsb = CreateDescriptorSet(descriptorLayouts["mainrenderpass"], MAX_FRAMES_IN_FLIGHT);
	dsb->AddDynamicUniformBuffer(GetDynamicUniformBuffer(), sizeof(glm::mat4) * 2, 0, MAX_FRAMES_IN_FLIGHT, 0);
	EntryHandle mrpDescId =  dsb->AddDescriptorsToCache();

	size_t perRenderPassStuff = GetPageFromUniformBuffer(datasize, (uint32_t)alignment);
	std::array<uint32_t, 1> data = { (uint32_t)allocations[perRenderPassStuff].offset };

	for (uint32_t i = 0; i<maxMSAALevels; i++)

		majorDevice->UpdateRenderGraph(swapchainRenderTargets[i], data.data(), (uint32_t)data.size(), mrpDescId);
	


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

EntryHandle RenderInstance::CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo* info, size_t* offsets, std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>* pushArgs, ResourceGraphNode* node)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);


	VKGraphicsPipelineObjectCreateInfo create = {
			.drawType = info->drawType,
			.vertexBufferIndex = allocations[info->vertexBufferIndex].memIndex,
			.vertexBufferOffset = static_cast<uint32_t>(allocations[info->vertexBufferIndex].offset),
			.vertexCount = info->vertexCount,
			.indirectDrawBuffer = allocations[info->indirectDrawBuffer].memIndex,
			.indirectDrawOffset = static_cast<uint32_t>(allocations[info->indirectDrawBuffer].offset),
			.pipelinename = EntryHandle(),
			.descriptorsetid = info->descriptorsetid,
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

		for (uint32_t j = 0; j < info->pushRangeCount; j++)
		{
			VKGraphicsPipelineObject->AddPushConstant(std::get<0>(pushArgs[j]), std::get<1>(pushArgs[j]), std::get<2>(pushArgs[j]), j, std::get<3>(pushArgs[j]));
		}

		auto graph = dev->GetRenderGraph(swapchainRenderTargets[i]);
		graph->AddObject(pipelineIndex);
	}

	return EntryHandle();
}

EntryHandle RenderInstance::CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info, size_t* offsets, std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>* pushArgs, ResourceGraphNode* node)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);


	VKComputePipelineObjectCreateInfo create = {
		.x = info->x,
		.y = info->y,
		.z = info->z,
		.descriptorId = info->descriptorsetid,
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
		vkPipelineObject->AddPushConstant(std::get<0>(pushArgs[i]), std::get<1>(pushArgs[i]), std::get<2>(pushArgs[i]), i, std::get<3>(pushArgs[i]));
	}

	if (node)
	{
		uint32_t traverse = 0;

		BarrierHeader* header = node->GetBarrierInfo(&traverse);

		VKComputePipelineObject* obj = (VKComputePipelineObject*)vkPipelineObject;
		while (header)
		{
			switch (header->type)
			{
			case BUFFER_BARRIER:
			{
				BufferBarrier* bb = (BufferBarrier*)header;
				obj->AddBufferMemoryBarrier(dev, allocations[bb->allocationIndex].memIndex, bb->size, allocations[bb->allocationIndex].offset, 
					
					ConvertResourceActionToVulkan(bb->srcActions),
					ConvertResourceActionToVulkan(bb->dstActions),
					ConvertResourceStageToVulkan(bb->sourceStage),
					ConvertResourceStageToVulkan(bb->destinationStage)
				);
				break;
			}
			case IMAGE_BARRIER:
			{
				
				break;
			} 
			}

			header = node->GetBarrierInfo(&traverse);
		}
	}

	


	auto graph = dev->GetComputeGraph(computeGraphIndex);
	graph->AddObject(pipelineIndex);

	return pipelineIndex;
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
	//threadedRecordBuffers[i].DrawLoop();
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
