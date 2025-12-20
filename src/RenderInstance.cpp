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
		case ImageFormat::B8G8R8A8:
			vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
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
		case VK_FORMAT_B8G8R8A8_SRGB:
			format = ImageFormat::B8G8R8A8;
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
		case TRIFAN:
			top = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
			break;
		case PrimitiveType::POINTSLIST:
			top = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
	vulkanShaderGraphs.shaderGraphs = (uintptr_t)malloc(2 * KB);
	descriptorManager.hostResourceHeap = (uintptr_t)malloc(2 * KB);
	shaderDetailsData = (char*)malloc(2 * KB);

	if (!vulkanShaderGraphs.shaderGraphs || !descriptorManager.hostResourceHeap || !shaderDetailsData)
	{
		throw std::runtime_error("cannot allocate render instance");
	}
	
	vulkanShaderGraphs.shaderGraphOffset = 0;
	descriptorManager.hostResourceHead = 0;

};

RenderInstance::~RenderInstance()
{
	free(shaderDetailsData);
	free((void*)vulkanShaderGraphs.shaderGraphs);
	free((void*)descriptorManager.hostResourceHeap);

	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	for (size_t i = 0; i < currentCBIndex.size(); i++)
	{
		dev->DestroyCommandBuffer(currentCBIndex[i]);
	}

	for (size_t i = 0; i < vulkanShaderGraphs.shaders.size(); i++)
	{
		dev->DestroyShader(vulkanShaderGraphs.shaders[i]);
	}

	for (auto& i : vulkanDescriptorLayouts)
	{
		if (i != EntryHandle())
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
	
	for (int i = 0; i<imagePoolCounter; i++)
		dev->DestroyImagePool(imagePools[i]);
	
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	swapChain->DestroySwapChain();
	
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
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, imagePools[1]);

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

int RenderInstance::RecreateSwapChain() {

	int ret = 0;
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);
	
	windowMan->GetWindowSize(&width, &height);

	if (width && height) {

		swc->Wait();

		DestroySwapChainAttachments();

		CreateSwapChain(width, height, true);

		ret = 1;
	}

	return ret;
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




uint32_t RenderInstance::BeginFrame()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	int32_t res = dev->CommandBufferWaitOn(UINT64_MAX, currentCBIndex[currentFrame]);

	uint32_t imageIndex = dev->BeginFrameForSwapchain(swapChainIndex, currentFrame);

	if (imageIndex == ~0ui32)
	{
		int ret = RecreateSwapChain();
		if (ret) resizeWindow = false;
		return imageIndex;
	}

	dev->CommandBufferResetFence(currentCBIndex[currentFrame]);
	
	DrawScene(currentCBIndex[currentFrame], imageIndex);

	return imageIndex;
}

int RenderInstance::SubmitFrame(uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t res = dev->SubmitCommandsForSwapChain(swapChainIndex, currentFrame, imageIndex, currentCBIndex[currentFrame]);

	res = dev->PresentSwapChain(swapChainIndex, imageIndex, currentFrame, currentCBIndex[currentFrame]);

	if (!res || resizeWindow) {
	
		dev->CommandBufferWaitOn(UINT64_MAX, currentCBIndex[currentFrame]);
		int ret = RecreateSwapChain();
		if (ret) resizeWindow = false;
	}
	
	currentFrame = (currentFrame  + 1) % MAX_FRAMES_IN_FLIGHT;
	return 0;
}



void RenderInstance::CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount) {

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	colorImages[index] = dev->CreateImage(width, height,
		1, swapChain->GetSwapChainFormat(), 1,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		sampleCount,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, imagePools[0]);

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
			
			swapchainRenderTargets[i] = dev->CreateRenderTargetData(swapChain->renderTargetIndex[i], 2, MAX_FRAMES_IN_FLIGHT);
			
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

	for (int j = 0; j < graph->resourceCount; j++)
	{
		ShaderResource* resource = (ShaderResource*)graph->GetResource(j);

		VkShaderStageFlags stageFlags = ConvertShaderStageToVKShaderStageFlags(resource->stages);

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
		case SAMPLERBINDLESS:
			descriptorBuilder->layoutFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			descriptorBuilder->AddBindlessSamplersLayout(resource->binding, stageFlags, resource->arrayCount);
			break;
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

	std::array<std::string, 4> layouts = {
		"C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\3DTexturedLayout.xml",
		"C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\TextLayout.xml",
		"C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\InterpolateMeshLayout.xml",
		"C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\shaders\\layouts\\PolynomialLayout.xml"
	};

	int detailsSize = 0, totalDetailSize = 0;

	int byteOffset = 0;

	for (int i = 0; i < 4; i++)
	{

		vulkanShaderGraphs.shaderGraphPtrs[i] = ShaderGraphReader::CreateShaderGraph(layouts[i], 
			vulkanShaderGraphs.shaderGraphs, &vulkanShaderGraphs.shaderGraphOffset, 
			shaderDetailsData + shaderDetailAlloc, &byteOffset, &detailsSize);

		CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[i]);

		shaderDetailAlloc += byteOffset;

		totalDetailSize += detailsSize;
	}

	ShaderDetails* deats = (ShaderDetails*)shaderDetailsData;

	int shaderGraph = 0;

	ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraph];

	int shaderMapCount = graph->shaderMapCount, mapIter = 0;

	for (int i = 0; i < totalDetailSize; i++)
	{
		char* str = deats->GetString();

		if (mapIter >= shaderMapCount)
		{
			++shaderGraph;
			graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraph];
			shaderMapCount = graph->shaderMapCount;
			mapIter = 0;
		}

		ShaderMap* map = (ShaderMap*)graph->GetMap(mapIter++);
		map->shaderReference = i;

		std::string shaderNameString = std::string(str);

		std::vector<char> buffer{};

		if (FileManager::FileExists(shaderNameString + ".spv")) {

			auto ret = FileManager::ReadFileInFull(shaderNameString + ".spv", buffer, std::ios::binary);
		}
		else
		{
			auto ret = FileManager::ReadFileInFull(shaderNameString, buffer, std::ios::binary);

			if (buffer.back() != '\0') buffer.push_back('\0');
		}

		vulkanShaderGraphs.shaders[i] = dev->CreateShader(buffer.data(), buffer.size(), dev->ConvertShaderFlags(shaderNameString));

		shaderDetails[i] = deats;

		deats = deats->GetNext();
	}

	auto computePipeline = dev->CreateComputePipelineBuilder(1, 1);

	computePipeline->AddPushConstantRange(0, sizeof(float), VK_SHADER_STAGE_COMPUTE_BIT, 0);

	pipelinesIdentifier[MESH_INTERPOLATE].push_back(CreateVulkanComputePipelineTemplate(computePipeline, vulkanShaderGraphs.shaderGraphPtrs[2]));

	auto polyPipeline = dev->CreateComputePipelineBuilder(1, 0);

	pipelinesIdentifier[POLY].push_back(CreateVulkanComputePipelineTemplate(polyPipeline, vulkanShaderGraphs.shaderGraphPtrs[3]));

	std::vector<EntryHandle> l(maxMSAALevels);
	std::vector<EntryHandle> r(maxMSAALevels);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		auto genericBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 2, 2, 0);
		auto textBuilder = dev->CreateGraphicsPipelineBuilder(renderPasses[i], 1, 1, 2, 0);

		UsePipelineBuilders(genericBuilder, textBuilder, (VkSampleCountFlagBits)(1<<i));

		r[i] = CreateVulkanGraphicPipelineTemplate(genericBuilder, vulkanShaderGraphs.shaderGraphPtrs[0]);

		l[i] = CreateVulkanGraphicPipelineTemplate(textBuilder, vulkanShaderGraphs.shaderGraphPtrs[1]);

	}

	pipelinesIdentifier[GENERIC] = r;
	
	pipelinesIdentifier[TEXT] = l;



}

EntryHandle RenderInstance::CreateVulkanGraphicPipelineTemplate(VKGraphicsPipelineBuilder* pipelineBuilder, ShaderGraph* graph)
{
	std::vector<EntryHandle> layoutHandles(graph->resourceSetCount);
	std::vector<EntryHandle> shaderHandle(graph->shaderMapCount);

	for (int i = 0; i < graph->shaderMapCount; i++)
	{
		ShaderMap* map = (ShaderMap*)graph->GetMap(i);
		shaderHandle[i] = vulkanShaderGraphs.shaders[map->shaderReference];
	}

	for (int i = 0; i < graph->resourceSetCount; i++)
	{
		ShaderSetLayout* resourceSet = (ShaderSetLayout*)graph->GetSet(i);
		layoutHandles[i] = vulkanDescriptorLayouts[resourceSet->vkDescriptorLayout];
	}

	return pipelineBuilder->CreateGraphicsPipeline(layoutHandles.data(), graph->resourceSetCount, shaderHandle.data(), graph->shaderMapCount);
}

EntryHandle RenderInstance::CreateVulkanComputePipelineTemplate(VKComputePipelineBuilder* pipelineBuilder, ShaderGraph* graph)
{
	std::vector<EntryHandle> layoutHandles(graph->resourceSetCount);
	EntryHandle shaderHandle;

	
	ShaderMap* map = (ShaderMap*)graph->GetMap(0);
	shaderHandle = vulkanShaderGraphs.shaders[map->shaderReference];
	

	for (int i = 0; i < graph->resourceSetCount; i++)
	{
		ShaderSetLayout* resourceSet = (ShaderSetLayout*)graph->GetSet(i);
		layoutHandles[i] = vulkanDescriptorLayouts[resourceSet->vkDescriptorLayout];
	}

	return pipelineBuilder->CreateComputePipeline(layoutHandles.data(), graph->resourceSetCount, shaderHandle);
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

	generic->CreateVertexInput(bindings1.data(), 0, ref1.data(), 0);
	text->CreateVertexInput(bindings.data(), 1, ref.data(), static_cast<uint32_t>(ref.size()));

	generic->CreateInputAssembly(API::ConvertTopology(TRISTRIPS), false);
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

void RenderInstance::UpdateAllocation(void* data, size_t handle, size_t size, size_t offset, size_t frame, int copies)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	
	size_t intSize = allocations[handle].requestedSize;
	size_t intOffset = allocations[handle].offset + offset;
	size_t stride = 0;

	if (size) 
	{
		intSize = size;
	}

	size_t rsize = allocations[handle].requestedSize;
	size_t align = allocations[handle].alignment;

	if (align - 1 & rsize) rsize = (rsize + (align - (align - 1 & rsize)));

	if (frame)
	{
		intOffset = allocations[handle].offset + (frame*rsize) + offset;
	}

	if (allocations[handle].allocType == PERFRAME && copies > 1)
	{
		stride = rsize;
	}

	EntryHandle index = allocations[handle].memIndex;

	if (index == globalIndex)
		dev->WriteToHostBuffer(index, data, intSize, intOffset, copies, stride);
	else if (index == globalDeviceBufIndex)
		dev->WriteToDeviceBuffer(index, stagingBufferIndex, data, intSize, intOffset, copies, stride);
}

int RenderInstance::GetAllocFromUniformBuffer(size_t size, uint32_t alignment, AllocationType allocType)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	size_t allocSize = size;

	if (minUniformAlignment - 1 & alignment)
		alignment = (alignment + (minUniformAlignment - ((minUniformAlignment - 1) & alignment)));
	
	if (minUniformAlignment - 1 & size)
		allocSize = (size + (minUniformAlignment - (minUniformAlignment - 1 & size)));

	switch (allocType)
	{
	case STATIC:
		break;
	case PERFRAME:
		allocSize*=MAX_FRAMES_IN_FLIGHT;
		break;
	case PERDRAW:
		break;
	}
	
	size_t location = dev->GetMemoryFromBuffer(globalIndex, allocSize, alignment);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = globalIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].deviceAllocSize = allocSize;
	allocations.allocations[index].requestedSize = size;
	allocations.allocations[index].alignment = alignment;

	return index;
}

int RenderInstance::GetAllocFromDeviceBuffer(size_t size, uint32_t alignment, AllocationType allocType)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	size_t allocSize = (size);

	switch (allocType)
	{
	case STATIC:
		break;
	case PERFRAME:
		allocSize *= MAX_FRAMES_IN_FLIGHT;
		break;
	case PERDRAW:
		break;
	}

	size_t location = dev->GetMemoryFromBuffer(globalDeviceBufIndex, size, alignment);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = globalDeviceBufIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].deviceAllocSize = allocSize;
	allocations.allocations[index].requestedSize = size;
	allocations.allocations[index].alignment = alignment;

	return index;
}

int RenderInstance::GetAllocFromDeviceStorageBuffer(size_t size, uint32_t alignment, AllocationType allocType)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	size_t allocSize = size;

	if (minStorageAlignment - 1 & alignment)
		alignment = (alignment + (minStorageAlignment - ((minStorageAlignment - 1) & alignment)));

	if (minStorageAlignment - 1 & size)
		allocSize = (size + (minStorageAlignment - (minStorageAlignment - 1 & size)));


	switch (allocType)
	{
	case STATIC:
		break;
	case PERFRAME:
		allocSize *= MAX_FRAMES_IN_FLIGHT;
		break;
	case PERDRAW:
		break;
	}



	size_t location = dev->GetMemoryFromBuffer(globalDeviceBufIndex, size, alignment);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = globalDeviceBufIndex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].deviceAllocSize = allocSize;
	allocations.allocations[index].requestedSize = size;
	allocations.allocations[index].alignment = alignment;

	return index;
}


EntryHandle RenderInstance::CreateImage(
	char* imageData,
	uint32_t* sizes,
	uint32_t blobSize,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type, int poolIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateSampledImage(
		imageData, sizes, blobSize,
		width, height,
		mipLevels, API::ConvertSMBToVkFormat(type),
		imagePools[poolIndex],
		stagingBufferIndex, VK_IMAGE_ASPECT_COLOR_BIT);
}

EntryHandle RenderInstance::CreateStorageImage(
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type, int poolIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateStorageImage(
		width, height,
		mipLevels, API::ConvertSMBToVkFormat(type),
		imagePools[poolIndex],
		stagingBufferIndex, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);
}


int RenderInstance::CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	VkFormat vkFormat = VK_FORMAT_MAX_ENUM;
	VkImageUsageFlags flags = 0;
	switch (format)
	{
	case ImageFormat::DXT1:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		break;
	case ImageFormat::DXT3:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	case ImageFormat::R8G8B8A8:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	case ImageFormat::R8G8B8A8_UNORM:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	case ImageFormat::B8G8R8A8:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_B8G8R8A8_SRGB;
		break;

	case ImageFormat::D24UNORMS8STENCIL:
		flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		break;
	case ImageFormat::D32FLOATS8STENCIL:
		flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		break;

	case ImageFormat::D32FLOAT:
		flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_D32_SFLOAT;
		break;
	default:
		throw std::runtime_error("Incorrect depth format");
	}

	if (!attachment)
	{
		flags = 0;
	}

	auto poolInfo = majorDevice->FindImageMemoryIndexForPool(maxWidth, maxHeight,
		1, vkFormat, 1,
		flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	int ret = imagePoolCounter++;

	imagePools[ret] = majorDevice->CreateImageMemoryPool(size, poolInfo.first);

	return ret;
}

int RenderInstance::AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount)
{
    uintptr_t head = descriptorManager.hostResourceHeap + descriptorManager.hostResourceHead;
    
    uintptr_t ptr = head;
    ShaderResourceSet* set = (ShaderResourceSet*)ptr;
    ptr += sizeof(ShaderResourceSet);

    ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex];

    ShaderSetLayout* resourceSet = (ShaderSetLayout*)graph->GetSet(targetSet);

    set->bindingCount = resourceSet->bindingCount;
    set->layoutHandle = resourceSet->vkDescriptorLayout;
    set->setCount = setCount;

    uintptr_t* offset = (uintptr_t*)ptr;

    ptr += sizeof(uintptr_t) * (set->bindingCount);


	int constantCount = set->bindingCount;
	for (int h = 0; h < set->bindingCount; h++)
	{
		MemoryBarrierType memBarrierType = MEMORY_BARRIER;

		ShaderResource* resource = (ShaderResource*)graph->GetResource(resourceSet->resourceStart+h);

		if (resource->set != targetSet) continue;

		ShaderResourceHeader* desc = (ShaderResourceHeader*)ptr;

		if (resource->binding != ~0)
			desc->binding = resource->binding;
		else
			desc->binding = constantCount--;

		desc->type = resource->type;
		desc->action = resource->action;
		desc->arrayCount = resource->arrayCount;

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
				barriers->dstStage = ConvertShaderStageToBarrierStage(resource->type);
				barriers->dstAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;

				barriers[1].srcStage = ConvertShaderStageToBarrierStage(resource->type);
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
				barriers->srcStage = ConvertShaderStageToBarrierStage(resource->type);
				barriers->srcAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;
				ptr += (sizeof(ImageShaderResourceBarrier));
			}
			break;
		}
		case SAMPLERBINDLESS:
		{
			memBarrierType = IMAGE_BARRIER;
			memset((void*)ptr, 0, sizeof(ShaderResourceSamplerBindless));
			ptr += sizeof(ShaderResourceSamplerBindless);
			break;
		}
		case CONSTANT_BUFFER:
		{
			ShaderResourceConstantBuffer* constants = (ShaderResourceConstantBuffer*)ptr;
			constants->size = 4;
			constants->offset = 0;
			constants->stage = resource->type;
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
				barriers->srcStage = ConvertShaderStageToBarrierStage(resource->type);
				barriers->srcAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;
				ptr += (sizeof(ShaderResourceBarrier));
			}
			break;
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

	minUniformAlignment = vkInstance->GetMinimumUniformBufferAlignment(physicalIndex);
	minStorageAlignment = vkInstance->GetMinimumStorageBufferAlignment(physicalIndex);

	maxMSAALevels = (uint32_t)log2((float)vkInstance->GetMaxMSAALevels(physicalIndex));
	maxMSAALevels += 1;
	currentMSAALevel = maxMSAALevels - 1;

	VkPhysicalDeviceVulkan12Features feature12{};
	feature12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	feature12.descriptorBindingPartiallyBound = VK_TRUE;
	feature12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	feature12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	feature12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	feature12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	feature12.storageBuffer8BitAccess = VK_TRUE;

	VkPhysicalDeviceFeatures2 features2{};
	features2.pNext = &feature12;
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.features.geometryShader = VK_TRUE;
	features2.features.textureCompressionBC = VK_TRUE;
	features2.features.tessellationShader = VK_TRUE;
	features2.features.samplerAnisotropy = VK_TRUE;
	features2.features.multiDrawIndirect = VK_TRUE;


	majorDevice->CreateLogicalDevice(vkInstance->instanceLayers,
		vkInstance->instanceLayerCount,
		vkInstance->deviceExtensions,
		vkInstance->deviceExtCount,
		VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT,
		&features2, vkInstance->renderSurface,
		64 * KB,
		8 * KB,
		96 * KB,
		12 * MB,
		16 * KB
	);



	std::array<VkFormat, 3> formats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };

	VkFormat depthFormatVK = VK::Utils::findSupportedFormat(majorDevice->gpu,
		formats.data(),
		formats.size(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	stagingBufferIndex = majorDevice->CreateHostBuffer(64 * MB, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);



	swapChainIndex = majorDevice->CreateSwapChain(3, MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT, maxMSAALevels);


	VKSwapChain* swapChain = majorDevice->GetSwapChain(swapChainIndex);

	VkFormat swcFormat = swapChain->GetSwapChainFormat();



	depthFormat = API::ConvertVkFormatToAppFormat(depthFormatVK);

	colorFormat = API::ConvertVkFormatToAppFormat(swcFormat);

	CreateImagePool(128 * MB, colorFormat, 4096, 4096, true);

	CreateImagePool(128 * MB, depthFormat, 4096, 4096, true);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		CreateRenderPass(i, (VkSampleCountFlagBits)(1 << i));
	}

	width = 800;
	height = 600;

	CreateSwapChain(width, height, false);



	DescriptorPoolBuilder builder = majorDevice->CreateDescriptorPoolBuilder(3, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddStoragePoolSize(MAX_FRAMES_IN_FLIGHT * 100);

	descriptorManager.deviceResourceHeap = majorDevice->CreateDesciptorPool(&builder, MAX_FRAMES_IN_FLIGHT * 101);

	globalIndex = majorDevice->CreateHostBuffer
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

	globalDeviceBufIndex = majorDevice->CreateDeviceBuffer(32'000'000,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	CreatePipelines();

	computeGraphIndex = majorDevice->CreateComputeGraph(0, 5, 0, MAX_FRAMES_IN_FLIGHT);

	

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		EntryHandle* lprimaryCommandBuffers = majorDevice->CreateReusableCommandBuffers(1, true, COMPUTE | TRANSFER | GRAPHICS, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		currentCBIndex[i] = *lprimaryCommandBuffers;
	}

}


void RenderInstance::CreateRenderTargetData(int* desc, int descCount)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	std::vector<EntryHandle> descIDs(descCount);
	std::vector<uint32_t> dynamicPerSet(descCount);

	uint32_t dynamicSize = 0;

	std::vector<uint32_t> data;

	for (int i = 0; i < descCount; i++)
	{
		descIDs[i] = CreateShaderResourceSet(desc[i]);
		uint32_t size = GetDynamicOffsetsForDescriptorSet(desc[i], data);
		dynamicPerSet[i] = size;
		dynamicSize += size;
	}


	for (uint32_t i = 0; i < maxMSAALevels; i++)
		majorDevice->UpdateRenderGraph(swapchainRenderTargets[i], data.data(), dynamicSize, descIDs.data(), descCount, dynamicPerSet.data());
	
}


uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

uint32_t RenderInstance::GetDynamicOffsetsForDescriptorSet(int descriptorSet, std::vector<uint32_t>& dynamicOffsets)
{
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	int count = set->bindingCount;

	uint32_t size = 0;

	for (int i = 0; i < count; i++)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];

		switch (header->type)
		{
		
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:
		{
			ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)offsets[i];
			auto alloc = allocations[buffer->allocation];
			dynamicOffsets.push_back(static_cast<uint32_t>(alloc.offset));
			size++;
			break;
		}
		default:
			break;

	
		}
	}

	return size;
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
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				builder->AddStorageImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case SAMPLER:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				builder->AddPixelShaderImageDescription(dev->GetImageViewByTexture(image->textureHandle, 0), dev->GetSamplerByTexture(image->textureHandle, 0), i, frames);
				break;
			}
			case STORAGE_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)header;
				auto alloc = allocations[buffer->allocation];
				if (alloc.allocType == PERFRAME)
					builder->AddDynamicStorageBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize / frames, i, frames, 0);
				else
					builder->AddDynamicStorageBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize, i, frames, 0);
				break;
			}
			case UNIFORM_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)header;
				auto alloc = allocations[buffer->allocation];
				if (alloc.allocType == PERFRAME)
					builder->AddDynamicUniformBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize / frames, i, frames, 0);
				else
					builder->AddDynamicUniformBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize, i, frames, 0);
				break;
			}

			case SAMPLERBINDLESS:
			{
				ShaderResourceSamplerBindless* samplers = (ShaderResourceSamplerBindless*)header;
				if (!samplers->textureHandles)
					builder->AddBindlessTextureArray(samplers->textureHandles, samplers->textureCount, 0, samplers->arrayCount, frames, i);
				break;
			}
		}
	}

	EntryHandle handle = builder->AddDescriptorsToCache();

	descriptorManager.vkDescriptorSets[descriptorSet] = handle;

	return handle;
}

void RenderInstance::UpdateSamplerBinding(int descriptorSet, int bindingIndex, EntryHandle* handles, uint32_t destinationArray, uint32_t texCount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;

	EntryHandle handle = descriptorManager.vkDescriptorSets[descriptorSet];

	int frames = set->setCount;

	DescriptorSetBuilder* builder = dev->UpdateDescriptorSet(handle);

	builder->AddBindlessTextureArray(handles, texCount, destinationArray, 0, frames, bindingIndex);
}

int RenderInstance::GetBufferAllocationViaDescriptor(int descriptorSet, int bindingIndex)
{
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[bindingIndex];

	if (header->type != UNIFORM_BUFFER && header->type != STORAGE_BUFFER) return -1;

	ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)header;

	return buffer->allocation;
}


EntryHandle RenderInstance::CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo* info)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t dynamicNumber = 0;
	std::vector<EntryHandle> descHandles(info->descCount);
	std::vector<uint32_t> offsetsPerSet(info->descCount);
	std::vector<uint32_t> dynamicOffsets;

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		offsetsPerSet[i] = GetDynamicOffsetsForDescriptorSet(info->descriptorsetid[i], dynamicOffsets);
		dynamicNumber += offsetsPerSet[i];
		descHandles[i] = CreateShaderResourceSet(info->descriptorsetid[i]);
	}

	EntryHandle indexBufferHandle = EntryHandle();
	uint32_t indexOffset = ~0ui32;

	if (info->indexBufferHandle)
	{
		indexBufferHandle = allocations[info->indexBufferHandle].memIndex;
		indexOffset = static_cast<uint32_t>(allocations[info->indexBufferHandle].offset);
	}

	VKGraphicsPipelineObjectCreateInfo create = {
			.vertexBufferIndex = allocations[info->vertexBufferIndex].memIndex,
			.vertexBufferOffset = static_cast<uint32_t>(allocations[info->vertexBufferIndex].offset),
			.vertexCount = info->vertexCount,
			.pipelinename = EntryHandle(),
			.descCount = 1,
			.descriptorsetid = descHandles.data(),
			.dynamicPerSet = offsetsPerSet.data(),
			.maxDynCap = dynamicNumber,
			.indexBufferHandle = indexBufferHandle,
			.indexBufferOffset = indexOffset,
			.indexCount = info->indexCount,
			.pushRangeCount = info->pushRangeCount,
			.instanceCount = info->instanceCount,
			.indexSize = info->indexSize
	};

	auto& ref = pipelinesIdentifier[info->pipelinename];

	for (uint32_t i = 0; i < maxMSAALevels; i++) {

		create.pipelinename = ref[i];

		EntryHandle pipelineIndex = dev->CreateGraphicsPipelineObject(&create);

		VKPipelineObject* VKGraphicsPipelineObject = dev->GetPipelineObject(pipelineIndex);

		for (uint32_t i = 0; i < dynamicNumber; i++)
		{
			VKGraphicsPipelineObject->SetPerObjectData(dynamicOffsets[i]);
		}

		int constantBufferPerSet = 0;

		for (uint32_t i = 0; i < info->pushRangeCount; i++)
		{
			ShaderResourceConstantBuffer* pushArgs = nullptr;
			uint32_t j = 0;
			do {
				pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid[j], constantBufferPerSet);
				constantBufferPerSet++;
			} while (!pushArgs && ((++j) < info->descCount) && !(constantBufferPerSet = 0));

			if (pushArgs) {
				VKGraphicsPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
			}
		}

		for (uint32_t i = 0; i < info->descCount; i++)
			AddVulkanMemoryBarrier(VKGraphicsPipelineObject, info->descriptorsetid[i]);

		auto graph = dev->GetRenderGraph(swapchainRenderTargets[i]);
		graph->AddObject(pipelineIndex);
	}

	return EntryHandle();
}

ShaderComputeLayout* RenderInstance::GetComputeLayout(int shaderGraphIndex)
{
	ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex];
	ShaderMap* map = (ShaderMap*)graph->GetMap(0);
	ShaderDetails* deats = shaderDetails[map->shaderReference];
	return (ShaderComputeLayout*)deats->GetShaderData();
}

void RenderInstance::SetActiveComputePipeline(uint32_t objectIndex, bool active)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	auto graph = dev->GetComputeGraph(computeGraphIndex);
	graph->SetActive(objectIndex, active);
}

uint32_t RenderInstance::CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t dynamicNumber = 0;
	std::vector<EntryHandle> descHandles(info->descCount);
	std::vector<uint32_t> offsetsPerSet(info->descCount);
	std::vector<uint32_t> dynamicOffsets;

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		offsetsPerSet[i] = GetDynamicOffsetsForDescriptorSet(info->descriptorsetid[i], dynamicOffsets);
		dynamicNumber += offsetsPerSet[i];
		descHandles[i] = CreateShaderResourceSet(info->descriptorsetid[i]);
	}

	VKComputePipelineObjectCreateInfo create = {
		.x = info->x,
		.y = info->y,
		.z = info->z,
		.descCount = info->descCount,
		.descriptorId = descHandles.data(),
		.dynamicPerSet = offsetsPerSet.data(),
		.pipelineId = pipelinesIdentifier[info->pipelinename][0],
		.maxDynCap = dynamicNumber,
		.barrierCount = info->barrierCount,
		.pushRangeCount = info->pushRangeCount
	};

	EntryHandle pipelineIndex = dev->CreateComputePipelineObject(&create);

	VKPipelineObject* vkPipelineObject = dev->GetPipelineObject(pipelineIndex);

	for (uint32_t i = 0; i < dynamicNumber; i++)
	{
		vkPipelineObject->SetPerObjectData(dynamicOffsets[i]);
	}

	int constantBufferPerSet = 0;

	for (uint32_t i = 0; i < info->pushRangeCount; i++)
	{
		ShaderResourceConstantBuffer* pushArgs = nullptr;
		uint32_t j = 0;
		do {
			pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid[j], constantBufferPerSet);
			constantBufferPerSet++;
		} while (!pushArgs && ((++j) < info->descCount) && !(constantBufferPerSet = 0));

		if (pushArgs) {
			vkPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, ConvertShaderStageToVKShaderStageFlags(pushArgs->stage));
		}
	}
	
	for (uint32_t i = 0; i < info->descCount; i++)
		AddVulkanMemoryBarrier(vkPipelineObject, info->descriptorsetid[i]);
	
	auto graph = dev->GetComputeGraph(computeGraphIndex);
	uint32_t ret = graph->AddObject(pipelineIndex);

	return ret;
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
				allocations[bufferBarrier->allocation].deviceAllocSize,
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

	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));
	while (*counter < set->bindingCount)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[*counter];
		(*counter)++;
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

	auto rcb = dev->GetRecordingBufferObject(cbindex);

	rcb.CommandBufferPool();

	rcb.BeginRecordingCommand(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cGraph->DispatchWork(&rcb, currentFrame);

	VkExtent2D* rect = &swc->swapChainExtent;

	rcb.BeginRenderPassCommand(swc->renderTargetIndex[currentMSAALevel], imageIndex, VK_SUBPASS_CONTENTS_INLINE, {{0, 0}, *rect});

	float x = static_cast<float>(rect->width), y = static_cast<float>(rect->height);

	rcb.SetViewportCommand(0, 0, x, y, 0.0f, 1.0f);

	rcb.SetScissorCommand(0, 0, rect->width, rect->height);

	graph->DrawScene(&rcb, currentFrame);

	rcb.EndRenderPassCommand();

	rcb.EndRecordingCommand();
}




void RenderInstance::IncreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);
	uint32_t next = currentMSAALevel + 1;
	if (next >= maxMSAALevels)
		return;
	currentMSAALevel = next;
}

void RenderInstance::DecreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);
	int32_t next = currentMSAALevel - 1;
	if (next < 0)
		return;
	currentMSAALevel = next;

}

