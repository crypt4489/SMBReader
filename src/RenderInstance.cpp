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
#include "VKOneTimeQueue.h"
#include "VertexTypes.h"
#include "WindowManager.h"


#include "ShaderResourceSet.h"

namespace GlobalRenderer {
	RenderInstance* gRenderInstance = nullptr;
}


namespace API {


	VkFormat ConvertComponentFormatTypeToVulkanFormat(ComponentFormatType type)
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		switch (type)
		{
		case ComponentFormatType::RAW_8BIT_BUFFER:
			format = VK_FORMAT_R8_UINT;
			break;
		case ComponentFormatType::R32_UINT:
			format = VK_FORMAT_R32_UINT;
			break;
		case ComponentFormatType::R32_SINT:
			format = VK_FORMAT_R32_SINT;
			break;
		case ComponentFormatType::R32G32B32A32_SFLOAT:
			format = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		case ComponentFormatType::R32G32B32_SFLOAT:
			format = VK_FORMAT_R32G32B32_SFLOAT;
			break;
		case ComponentFormatType::R32G32_SFLOAT:
			format = VK_FORMAT_R32G32_SFLOAT;
			break;
		case ComponentFormatType::R32_SFLOAT:
			format = VK_FORMAT_R32_SFLOAT;
			break;
		case ComponentFormatType::R32G32_SINT:
			format = VK_FORMAT_R32G32_SINT;
			break;
		case ComponentFormatType::R8G8_UINT:
			format = VK_FORMAT_R8G8_UINT;
			break;
		case ComponentFormatType::R16G16_SINT:
			format = VK_FORMAT_R16G16_SINT;
			break;
		case ComponentFormatType::R16G16B16_SINT:
			format = VK_FORMAT_R16G16B16_SINT;
			break;
		default:
			break;
		}

		return format;
	}

	VkCompareOp ConvertRasterizerTestToVulkanCompareOp(RasterizerTest testApp)
	{
		VkCompareOp ret = VK_COMPARE_OP_ALWAYS;

		switch (testApp)
		{
		case RasterizerTest::NEVER:
			ret = VK_COMPARE_OP_NEVER;
			break;

		case RasterizerTest::LESS:
			ret = VK_COMPARE_OP_LESS;
			break;

		case RasterizerTest::EQUAL:
			ret = VK_COMPARE_OP_EQUAL;
			break;

		case RasterizerTest::LESSEQUAL:
			ret = VK_COMPARE_OP_LESS_OR_EQUAL;
			break;

		case RasterizerTest::GREATER:
			ret = VK_COMPARE_OP_GREATER;
			break;

		case RasterizerTest::NOTEQUAL:
			ret = VK_COMPARE_OP_NOT_EQUAL;
			break;

		case RasterizerTest::GREATEREQUAL:
			ret = VK_COMPARE_OP_GREATER_OR_EQUAL;
			break;

		case RasterizerTest::ALLPASS:
			ret = VK_COMPARE_OP_ALWAYS;
			break;

		default:
			break;
		}

		return ret;
	}

	VkFormat ConvertImageFormatToVulkanFormat(ImageFormat format)
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
		case ImageFormat::B8G8R8A8_UNORM:
			vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
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

		case POINTSLIST:
			top = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;

		case LINELIST:
			top = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;

		case LINESTRIPS:
			top = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			break;

		default:
			break;
		}

		return top;
	}

	VkAccessFlags ConvertResourceActionToVulkanAccessFlags(BarrierAction action)
	{
		VkAccessFlags flags = 0;
		flags |= (VK_ACCESS_SHADER_WRITE_BIT) * ((action & WRITE_SHADER_RESOURCE) != 0);
		flags |= (VK_ACCESS_SHADER_READ_BIT) * ((action & READ_SHADER_RESOURCE) != 0);
		flags |= (VK_ACCESS_UNIFORM_READ_BIT) * ((action & READ_UNIFORM_BUFFER) != 0);
		flags |= (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) * ((action & READ_VERTEX_INPUT) != 0);
		flags |= (VK_ACCESS_INDIRECT_COMMAND_READ_BIT) & ((action & READ_INDIRECT_COMMAND) != 0);
		return flags;
	}

	VkPipelineStageFlags ConvertBarrierStageToVulkanPipelineStage(BarrierStage sourceStage)
	{
		VkPipelineStageFlags flags = 0;
		flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) * ((sourceStage & VERTEX_SHADER_BARRIER) != 0);
		flags |= (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) * ((sourceStage & VERTEX_INPUT_BARRIER) != 0);
		flags |= (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) * ((sourceStage & COMPUTE_BARRIER) != 0);
		flags |= (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) * ((sourceStage & BEGINNING_OF_PIPE) != 0);
		flags |= (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) * ((sourceStage & FRAGMENT_BARRIER) != 0);
		flags |= (VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT) * ((sourceStage & INDIRECT_DRAW_BARRIER) != 0);
		return flags;
	}

	VkShaderStageFlags ConvertShaderStageToVulkanShaderStage(ShaderStageType type)
	{
		VkShaderStageFlags flags = 0;
		flags |= (VK_SHADER_STAGE_VERTEX_BIT) * ((type & VERTEXSHADERSTAGE) != 0);
		flags |= (VK_SHADER_STAGE_FRAGMENT_BIT) * ((type & FRAGMENTSHADERSTAGE) != 0);
		flags |= (VK_SHADER_STAGE_COMPUTE_BIT) * ((type & COMPUTESHADERSTAGE) != 0);
		return flags;
	}

	VkImageLayout ConvertImageLayoutToVulkanImageLayout(ImageLayout layout)
	{
		VkImageLayout outLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		switch (layout)
		{
		case ImageLayout::UNDEFINED:

			break;
		case ImageLayout::WRITEABLE:
			outLayout = VK_IMAGE_LAYOUT_GENERAL;
			break;
		case ImageLayout::SHADERREADABLE:
			outLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;
		case ImageLayout::COLORATTACHMENT:
			outLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
		case ImageLayout::DEPTHSTENCILATTACHMENT:
			outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;
		}

		return outLayout;
	}

	void ConvertVertexInputToVKVertexAttrDescription(VertexInputDescription* inputDescs, int numInputDescs, int vertexBufferLocation, VkVertexInputAttributeDescription* attrs)
	{
		for (int i = 0; i < numInputDescs; i++)
		{
			VkVertexInputAttributeDescription& attr = attrs[i];

			attr.location = i;
			attr.format = ConvertComponentFormatTypeToVulkanFormat(inputDescs[i].format);
			attr.offset = inputDescs[i].byteoffset;
			attr.binding = vertexBufferLocation;
		}
	}

	VkFrontFace ConvertTriangleWinding(TriangleWinding winding)
	{
		VkFrontFace face = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		switch (winding)
		{
		case CW:
			face = VK_FRONT_FACE_CLOCKWISE;
			break;

		case CCW:
			face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			break;

		default:
			break;
		}

		return face;
	}

	VkCullModeFlags ConvertCullMode(CullMode mode)
	{
		VkCullModeFlags ret = VK_CULL_MODE_NONE;

		switch (mode)
		{
		case CullMode::CULL_NONE:
			ret = VK_CULL_MODE_NONE;
			break;

		case CullMode::CULL_BACK:
			ret = VK_CULL_MODE_BACK_BIT;
			break;

		case CullMode::CULL_FRONT:
			ret = VK_CULL_MODE_FRONT_BIT;
			break;

		default:
			break;
		}

		return ret;
	}

	VkStencilOp ConvertStencilOpToVulkan(StencilOp op)
	{
		switch (op)
		{
		case StencilOp::REPLACE:
			return VK_STENCIL_OP_REPLACE;

		case StencilOp::KEEP:
			return VK_STENCIL_OP_KEEP;

		case StencilOp::ZERO:
			return VK_STENCIL_OP_ZERO;

		default:
			return VK_STENCIL_OP_KEEP;
		}
	}

	VkStencilOpState ConvertFaceStencilDataToVulkan(const FaceStencilData& face)
	{
		VkStencilOpState state{};
		state.failOp = ConvertStencilOpToVulkan(face.failOp);
		state.passOp = ConvertStencilOpToVulkan(face.passOp);
		state.depthFailOp = ConvertStencilOpToVulkan(face.depthFailOp);
		state.compareOp = ConvertRasterizerTestToVulkanCompareOp(face.stencilCompare);

		state.compareMask = static_cast<uint32_t>(face.compareMask);
		state.writeMask = static_cast<uint32_t>(face.writeMask);
		state.reference = static_cast<uint32_t>(face.reference);

		return state;
	}

}


RenderInstance::RenderInstance(SlabAllocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator)
	:
	vulkanShaderGraphs{ instanceStorageAllocator->Allocate(5 * KiB), 5*KiB, instanceStorageAllocator->Allocate(5 * KiB), 5*KiB},
	descriptorManager{ instanceStorageAllocator->Allocate(5 * KiB), 5*KiB},
	minStorageAlignment(0), minUniformAlignment(0)
{
	vkInstance = new VKInstance();

	cacheAllocator = instanceCacheAllocator;
	storageAllocator = instanceStorageAllocator;

	updateCommandsCache = (RingAllocator*)instanceStorageAllocator->Allocate(sizeof(RingAllocator));
	std::construct_at(updateCommandsCache, instanceStorageAllocator->Allocate(64 * KiB), 64*KiB);

	for (uint32_t i = 0; i < 2; i++)
	{
		updateCommandBuffers[i] = (SlabAllocator*)instanceStorageAllocator->Allocate(sizeof(SlabAllocator));
		std::construct_at(updateCommandBuffers[i], instanceStorageAllocator->Allocate(16 * KiB), 16 * KiB);
	}

	int driverHostLinkedSize = driverHostMemoryUpdater.GetSize(800);
	int commandLinkedSize = transferCommandPool.GetSize(20);
	int resourceUpdateLinkedSize = descriptorUpdatePool.GetSize(30);
	int driverDeviceLinkedSize = driverDeviceMemoryUpdater.GetSize(60);
	int imageMemoryLinkedSize = imageMemoryUpdateManager.GetSize(30);

	driverHostMemoryUpdater.AllocateList(instanceStorageAllocator->Allocate(driverHostLinkedSize), driverHostLinkedSize);

	transferCommandPool.AllocateList(instanceStorageAllocator->Allocate(commandLinkedSize), commandLinkedSize);

	driverDeviceMemoryUpdater.AllocateList(instanceStorageAllocator->Allocate(driverDeviceLinkedSize), driverDeviceLinkedSize);

	imageMemoryUpdateManager.AllocateList(instanceStorageAllocator->Allocate(imageMemoryLinkedSize), imageMemoryLinkedSize);

	descriptorUpdatePool.AllocateList(instanceStorageAllocator->Allocate(resourceUpdateLinkedSize), resourceUpdateLinkedSize);
};

RenderInstance::~RenderInstance()
{
	auto dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);


	/*
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
	*/

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

	VkFormat vkDepthFormat = API::ConvertImageFormatToVulkanFormat(depthFormat);

	depthImages[index] = dev->CreateImage(width, height,
		1, vkDepthFormat, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		sampleCount,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, imagePools[1]);

	depthViews[index] = dev->CreateImageView(depthImages[index], 1, 1, vkDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_VIEW_TYPE_2D);

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

	VkFormat vkDepthFormat = API::ConvertImageFormatToVulkanFormat(depthFormat);

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
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
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
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
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
	
		return imageIndex;
	}

	

	dev->CommandBufferResetFence(currentCBIndex[currentFrame]);

	return imageIndex;
}

int RenderInstance::SubmitFrame(uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t res = dev->SubmitCommandsForSwapChain(swapChainIndex, currentFrame, imageIndex, currentCBIndex[currentFrame]);

	res = dev->PresentSwapChain(swapChainIndex, imageIndex, currentFrame, currentCBIndex[currentFrame]);

	if (!res) {
		dev->CommandBufferWaitOn(UINT64_MAX, currentCBIndex[currentFrame]);
		int ret = RecreateSwapChain();
		return -1;
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
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, imagePools[0]);

	colorViews[index] = dev->CreateImageView(colorImages[index], 1, 1, swapChain->GetSwapChainFormat(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
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
			
			swapchainRenderTargets[i] = dev->CreateRenderTargetData(swapChain->renderTargetIndex[i], 2);
			
		}
	}
	
	EntryHandle* views = swapChain->CreateSwapchainViews();

	EntryHandle* attachmentViews = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * 3);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		CreateDepthImage(width, height, i, 1 << i);

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
			
		swapChain->CreateSwapChainElements(i, count, attachmentViews, views);
	}
}

void RenderInstance::CreateShaderResourceMap(ShaderGraph* graph)
{
	static int descriptorLayoutIndex = 0;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	DescriptorSetLayoutBuilder** descriptorBuilders = (DescriptorSetLayoutBuilder**)cacheAllocator->Allocate(sizeof(DescriptorSetLayoutBuilder*) * graph->resourceSetCount);

	int count = graph->resourceSetCount;

	for (int j = 0; j < graph->resourceSetCount; j++)
	{
		ShaderResourceSetTemplate* set = (ShaderResourceSetTemplate*)graph->GetSet(j);
		descriptorBuilders[j] = dev->CreateDescriptorSetLayoutBuilder(set->bindingCount);
		set->vulkanDescLayout = descriptorLayoutIndex+j;
	}

	for (int j = 0; j < graph->resourceCount; j++)
	{
		ShaderResource* resource = (ShaderResource*)graph->GetResource(j);

		VkShaderStageFlags stageFlags = API::ConvertShaderStageToVulkanShaderStage(resource->stages);

		if (resource->type == ShaderResourceType::CONSTANT_BUFFER) {
			descriptorBuilders[resource->set]->bindingCounts--;
			continue;
		}
			
		DescriptorSetLayoutBuilder* descriptorBuilder = descriptorBuilders[resource->set];

		int arrayCount = resource->arrayCount;

		VkDescriptorBindingFlags bindingFlags = 0;

		

		if (arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY)
		{
			bindingFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
		}

		static bool useUpdateAfterBind = true;

		arrayCount &= DESCRIPTOR_COUNT_MASK;

		switch (resource->type)
		{
		case ShaderResourceType::UNIFORM_BUFFER:
			descriptorBuilder->AddDynamicBufferLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::IMAGESTORE2D:
			descriptorBuilder->AddStorageImageLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::IMAGE2D:
			if (useUpdateAfterBind)
			{
				bindingFlags |=
					VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

				descriptorBuilder->layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			}
			descriptorBuilder->AddImageResourceLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::SAMPLERSTATE:
			if (useUpdateAfterBind)
			{
				bindingFlags |=
					VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

				descriptorBuilder->layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			}
			descriptorBuilder->AddSamplerStateLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			
			break;
		case ShaderResourceType::SAMPLER2D:
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLERCUBE:
			if (useUpdateAfterBind)
			{
				bindingFlags |=
					VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
					VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

				descriptorBuilder->layoutFlags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
			}
			descriptorBuilder->AddBindlessCombinedSamplersLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::STORAGE_BUFFER:
			descriptorBuilder->AddDynamicStorageBufferLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::BUFFER_VIEW:
			if (resource->action == ShaderResourceAction::SHADERREAD)
				descriptorBuilder->AddUniformBufferViewLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			else if (resource->action == ShaderResourceAction::SHADERWRITE)
				descriptorBuilder->AddStorageBufferViewLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		}
	


		
	}


	for (int j = 0; j < count; j++)
	{
		vulkanDescriptorLayouts[descriptorLayoutIndex++] = dev->CreateDescriptorSetLayout(descriptorBuilders[j]);
	}

}


void RenderInstance::CreatePipelines(std::string* shaderGraphLayouts, int shaderGraphLayoutsCount, std::string* pipelineDescriptions, int pipelineDescriptionsCount)
{
	static int pipelineInfoIndex = 0;

	for (int i = 0; i < pipelineDescriptionsCount; i++)
	{
		CreatePipelineDescription(pipelineDescriptions[i], &pipelineInfos[pipelineInfoIndex++]);
	}

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	int detailsSize = 0, totalDetailSize = 0;

	ShaderDetails* deats = (ShaderDetails*)vulkanShaderGraphs.shaderDetailsAllocator.Head();

	for (int i = 0; i < shaderGraphLayoutsCount; i++)
	{
		vulkanShaderGraphs.shaderGraphPtrs[i] = 
			CreateShaderGraph(shaderGraphLayouts[i],
							  cacheAllocator,
							  &vulkanShaderGraphs.graphAllocator,
							  &vulkanShaderGraphs.shaderDetailsAllocator, &detailsSize);

		CreateShaderResourceMap(vulkanShaderGraphs.shaderGraphPtrs[i]);

		totalDetailSize += detailsSize;
	} 

	int shaderGraph = 0;

	ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraph];

	int shaderMapCount = graph->shaderMapCount, mapIter = 0;

	int outputShaderIndexHead = vulkanShaderGraphs.shaderCount;

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

		char* shaderData;

		OSFileHandle* handle;

		int shaderLength = 0;

		if (FileManager::FileExists(shaderNameString + ".spv")) {

			FileID id = FileManager::OpenFile(shaderNameString + ".spv", READ);

			handle = FileManager::GetFile(id);

			shaderLength = handle->fileLength;

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength);

			OSReadFile(handle, shaderLength, shaderData);
		}
		else
		{

			FileID id = FileManager::OpenFile(shaderNameString, READ);

			handle = FileManager::GetFile(id);

			shaderLength = handle->fileLength;

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength+1);

			OSReadFile(handle, shaderLength, shaderData);

			if (shaderData[shaderLength-1] != '\0')
			{
				shaderData[shaderLength] = '\0';
				shaderLength++;
			}
		}

		vulkanShaderGraphs.shaders[outputShaderIndexHead+i] = dev->CreateShader(shaderData, shaderLength, dev->ConvertShaderFlags(shaderNameString));

		vulkanShaderGraphs.shaderDetails[outputShaderIndexHead+i] = deats;

		deats = deats->GetNext();

		OSCloseFile(handle);
	}

	int computeIter = 0;

	int pipelineDescription = 0;

	for (int i = 0; i < shaderGraphLayoutsCount; i++)
	{
		ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[i];
		ShaderMap* map = (ShaderMap*)graph->GetMap(0);
		if (map->type == COMPUTESHADERSTAGE)
		{
			EntryHandle* pipelineID = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle));

			*pipelineID = CreateVulkanComputePipelineTemplate(vulkanShaderGraphs.shaderGraphPtrs[i]);

			pipelinesIdentifier[i] = pipelineID;
		}
		else 
		{
			EntryHandle* pipelineHandles = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * maxMSAALevels);
			CreatePipelineFromGraphAndSpec(&pipelineInfos[pipelineDescription++], vulkanShaderGraphs.shaderGraphPtrs[i], pipelineHandles, 0);
			pipelinesIdentifier[i] = pipelineHandles;
		}
	}
}

void RenderInstance::CreatePipelineFromGraphAndSpec(GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle* layoutHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * graph->resourceSetCount);
	EntryHandle* shaderHandle = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * graph->shaderMapCount);

	for (int i = 0; i < graph->shaderMapCount; i++)
	{
		ShaderMap* map = (ShaderMap*)graph->GetMap(i);
		shaderHandle[i] = vulkanShaderGraphs.shaders[map->shaderReference];
	}

	int pushConstantRangeCount = 0;

	for (int i = 0; i < graph->resourceSetCount; i++)
	{
		ShaderResourceSetTemplate* resourceSet = (ShaderResourceSetTemplate*)graph->GetSet(i);
		layoutHandles[i] = vulkanDescriptorLayouts[resourceSet->vulkanDescLayout];
		pushConstantRangeCount += resourceSet->constantStageCount;
	}

	uint32_t* pushConstantsSizes = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * pushConstantRangeCount);
	VkShaderStageFlags* shaderStages = (VkShaderStageFlags*)cacheAllocator->CAllocate(sizeof(VkShaderStageFlags) * pushConstantRangeCount);

	for (int i = 0; i < graph->resourceCount; i++)
	{
		ShaderResource* resource = (ShaderResource*)graph->GetResource(i);
		if (resource->type == ShaderResourceType::CONSTANT_BUFFER)
		{
			int rangeIndex = resource->rangeIndex;
			pushConstantsSizes[rangeIndex] += resource->size;
			if (resource->stages & VERTEXSHADERSTAGE)
			{
				shaderStages[rangeIndex] |= VK_SHADER_STAGE_VERTEX_BIT;
			}
			if (resource->stages & FRAGMENTSHADERSTAGE)
			{
				shaderStages[rangeIndex] |= VK_SHADER_STAGE_FRAGMENT_BIT;
			}
		}
	}

	VKGraphicsPipelineBuilder* pipelineBuilder = dev->CreateGraphicsPipelineBuilder(EntryHandle(), 1, graph->resourceSetCount, 2, pushConstantRangeCount);

	uint32_t globalPushOffset = 0;

	for (int i = 0; i < pushConstantRangeCount; i++)
	{
		pipelineBuilder->AddPushConstantRange(globalPushOffset, pushConstantsSizes[i], shaderStages[i], i);
		globalPushOffset += pushConstantsSizes[i];
	}


	std::array<VkDynamicState, 2> dynamicStates = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
	};


	pipelineBuilder->CreateDynamicStateInfo(dynamicStates.data(), 2);

	VkVertexInputBindingDescription* bindingDescriptions = (VkVertexInputBindingDescription*)cacheAllocator->Allocate(sizeof(VkVertexInputBindingDescription) * (stateInfo->vertexBufferDescCount));

	int descCount = 0;

	for (int i = 0; i < stateInfo->vertexBufferDescCount; i++)
	{

		bindingDescriptions[i] =  VK::Utils::CreateVertexInputBindingDescription(i, stateInfo->vertexBufferDesc[i].perInputSize);

		descCount += stateInfo->vertexBufferDesc[i].descCount;
	}

	VkVertexInputAttributeDescription* vertexBufferInput = (VkVertexInputAttributeDescription*)cacheAllocator->Allocate(sizeof(VkVertexInputAttributeDescription) * (descCount));

	int iter = 0;
	
	for (int i = 0; i < stateInfo->vertexBufferDescCount; i++)
	{
	
		API::ConvertVertexInputToVKVertexAttrDescription(stateInfo->vertexBufferDesc->descriptions, stateInfo->vertexBufferDesc[i].descCount, i, &vertexBufferInput[iter]);

		iter += stateInfo->vertexBufferDesc[i].descCount;
	}

	
	pipelineBuilder->CreateVertexInput(bindingDescriptions, stateInfo->vertexBufferDescCount, vertexBufferInput, descCount);
	
	pipelineBuilder->CreateInputAssembly(API::ConvertTopology(stateInfo->primType), false);

	pipelineBuilder->CreateViewportState(1, 1);

	pipelineBuilder->CreateRasterizer(API::ConvertCullMode(stateInfo->cullMode), API::ConvertTriangleWinding(stateInfo->windingOrder), stateInfo->lineWidth);

	pipelineBuilder->CreateColorBlendAttachment(0, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	pipelineBuilder->CreateColorBlending(VK_LOGIC_OP_COPY);


	VkStencilOpState frontState = API::ConvertFaceStencilDataToVulkan(stateInfo->frontFace);
	VkStencilOpState backState = API::ConvertFaceStencilDataToVulkan(stateInfo->backFace);

	pipelineBuilder->CreateDepthStencil(API::ConvertRasterizerTestToVulkanCompareOp(stateInfo->depthTest), stateInfo->depthWrite, stateInfo->StencilEnable, &frontState, &backState);

	for (uint32_t i = 0; i < maxMSAALevels; i++)
	{
		int msaaLevel = (1 << i);
		if (msaaLevel > stateInfo->sampleCountHigh) break;

		pipelineBuilder->CreateMultiSampling((VkSampleCountFlagBits)msaaLevel);
		pipelineBuilder->renderPass = dev->GetRenderPass(renderPasses[i]);

		outHandles[outHandlePointer+i] = pipelineBuilder->CreateGraphicsPipeline(layoutHandles, graph->resourceSetCount, shaderHandle, graph->shaderMapCount);
	}

}

EntryHandle RenderInstance::CreateVulkanComputePipelineTemplate(ShaderGraph* graph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle* layoutHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * (graph->resourceSetCount));

	EntryHandle shaderHandle;

	ShaderMap* map = (ShaderMap*)graph->GetMap(0);

	shaderHandle = vulkanShaderGraphs.shaders[map->shaderReference];

	int pushRangeSize = 0;

	for (int a = 0; a < graph->resourceSetCount; a++)
	{
		ShaderResourceSetTemplate* setLayout = (ShaderResourceSetTemplate*)graph->GetSet(a);

		pushRangeSize += setLayout->constantStageCount;
	}

	int* pushConstantSize = (int*)cacheAllocator->CAllocate(sizeof(int) * pushRangeSize);

	for (int g = 0; g < graph->resourceCount; g++)
	{
		ShaderResource* resource = (ShaderResource*)graph->GetResource(g);

		if (resource->type == ShaderResourceType::CONSTANT_BUFFER)
		{
			int pushRangeIndex = resource->rangeIndex;

			pushConstantSize[pushRangeIndex] += resource->size;
		}
	}

	int descriptorCount = graph->resourceSetCount;

	VKComputePipelineBuilder* pipelineBuilder = dev->CreateComputePipelineBuilder(descriptorCount, pushRangeSize);

	uint32_t globalOffset = 0;

	for (int g = 0; g < pushRangeSize; g++)
	{
		pipelineBuilder->AddPushConstantRange(globalOffset, pushConstantSize[g], VK_SHADER_STAGE_COMPUTE_BIT, g);

		globalOffset += pushConstantSize[g];
	}
	
	for (int i = 0; i < graph->resourceSetCount; i++)
	{
		ShaderResourceSetTemplate* resourceSet = (ShaderResourceSetTemplate*)graph->GetSet(i);

		layoutHandles[i] = vulkanDescriptorLayouts[resourceSet->vulkanDescLayout];
	}

	return pipelineBuilder->CreateComputePipeline(layoutHandles, graph->resourceSetCount, shaderHandle);
}





void RenderInstance::UploadHostTransfers()
{
	
	int memCount = driverHostMemoryUpdater.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	BufferMemoryTransferRegion region;
	int link = driverHostMemoryUpdater.linkHead;
	int* linkprev = &driverHostMemoryUpdater.linkHead;
	
	EntryHandle previousBuffer = EntryHandle();
	size_t previousMin = 0;
	size_t previousMax = 0;
	size_t batchCounter = 0;


	void** batchAddresses = (void**)cacheAllocator->Allocate(sizeof(void*) * memCount);
	size_t* batchSizes = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * memCount);
	size_t* batchOffsets = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * memCount);

	while (link >= 0)
	{
		link = driverHostMemoryUpdater.PopLink(&region, link, &linkprev);

		int handle = region.allocationIndex;

		size_t intSize = region.size;
		
		size_t rsize = allocations[handle].requestedSize;
		size_t align = allocations[handle].alignment;

		rsize *= allocations[handle].structureCopies;

		rsize = (rsize + align-1) & ~(align - 1);

		size_t intOffset = allocations[handle].offset + (currentFrame * rsize) + region.allocoffset;

		EntryHandle index = allocations[handle].memIndex;

		void* data = region.data;

		if (index != previousBuffer)
		{
			if (previousBuffer != EntryHandle())
			{
				if (previousBuffer == globalIndex)
					dev->WriteToHostBufferBatch(globalIndex, batchAddresses, batchSizes, batchOffsets, previousMax - previousMin, previousMin, batchCounter);
			}

			previousBuffer = index;
			batchCounter = 0;
		}

		batchAddresses[batchCounter] = data;
		batchOffsets[batchCounter] = intOffset;
		batchSizes[batchCounter] = intSize;

		batchCounter++;

		previousMin = std::min(intOffset, previousMin);
		previousMax = std::max(intOffset + rsize, previousMax);
	}

	dev->WriteToHostBufferBatch(previousBuffer, batchAddresses, batchSizes, batchOffsets, previousMax - previousMin, previousMin, batchCounter);
}

void RenderInstance::UploadDescriptorsUpdates()
{
	int memCount = descriptorUpdatePool.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	
	int link = descriptorUpdatePool.linkHead;
	int* linkprev = &descriptorUpdatePool.linkHead;
	ShaderResourceUpdate region;

	EntryHandle previousBuffer = EntryHandle();

	DescriptorSetBuilder* builder = nullptr;

	while (link >= 0)
	{
		link = descriptorUpdatePool.PopLink(&region, link, &linkprev);

		EntryHandle index = descriptorManager.vkDescriptorSets[region.descriptorSet];

		void* data = region.data;

		if (index != previousBuffer)
		{
			builder = dev->UpdateDescriptorSet(index);
			previousBuffer = index;
		}
		
		switch (region.type)
		{
		case ShaderResourceType::SAMPLERSTATE:
		{

			ResourceArrayUpdate* update = (ResourceArrayUpdate*)region.data;
			builder->AddSamplerDescription(update->resourceHandles, update->resourceCount, update->resourceDstBegin, region.bindingIndex, currentFrame, 1);
			break;
		}
		case ShaderResourceType::IMAGE2D:
		{

			ResourceArrayUpdate* update = (ResourceArrayUpdate*)region.data;
			builder->AddImageResourceDescription(update->resourceHandles, update->resourceCount, update->resourceDstBegin, region.bindingIndex, currentFrame, 1);
			break;
		}
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLER2D:
		{
			ResourceArrayUpdate* update = (ResourceArrayUpdate*)region.data;
			builder->AddCombinedTextureArray(update->resourceHandles, update->resourceCount, update->resourceDstBegin, region.bindingIndex, currentFrame, 1);
			break;
		}

		}
	}
}

void RenderInstance::UploadImageMemoryTransfers(RecordingBufferObject* rbo)
{
	int memCount = imageMemoryUpdateManager.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	TextureMemoryRegion region;
	int link = imageMemoryUpdateManager.linkHead;

	while (link >= 0)
	{
		link = imageMemoryUpdateManager.PopLink(&region, link);

		EntryHandle handle = region.textureIndex;

		dev->UploadImageData(handle,
			(char*)region.data,
			region.totalSize,
			region.imageSizes,
			stagingBuffers[currentFrame],
			region.width,
			region.height,
			region.mipLevels,
			region.layers,
			API::ConvertImageFormatToVulkanFormat(region.format),
			rbo
		);
	}

	imageMemoryUpdateManager.ddsRegionAlloc = 0;
	imageMemoryUpdateManager.linkHead = -1;
}


void RenderInstance::UploadDeviceLocalTransfers(RecordingBufferObject* rbo)
{
	int memCount = driverDeviceMemoryUpdater.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	BufferMemoryTransferRegion region;
	int link = driverDeviceMemoryUpdater.linkHead;
	int* linkprev = &driverDeviceMemoryUpdater.linkHead;

	size_t* batchSizes = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * (memCount));
	size_t* batchOffsets = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * (memCount));
	void** batchData = (void**)cacheAllocator->Allocate(sizeof(void*) * (memCount));

	size_t batchCounter = 0;
	size_t cumulativeSize = 0;

	EntryHandle previousBuffer = EntryHandle();

	while (link >= 0)
	{
		link = driverDeviceMemoryUpdater.PopLink(&region, link, &linkprev);
		
		EntryHandle index = allocations.allocations[region.allocationIndex].memIndex;
	
		if (index != previousBuffer)
		{
			if (previousBuffer != EntryHandle())
			{
				if (previousBuffer == globalDeviceBufIndex)
					dev->WriteToDeviceBufferBatch(globalDeviceBufIndex, stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, batchCounter, rbo);
			}

			previousBuffer = index;
			batchCounter = 0;
			cumulativeSize = 0;
		}


		batchSizes[batchCounter] = region.size;
		batchData[batchCounter] = region.data;
		batchOffsets[batchCounter] = allocations.allocations[region.allocationIndex].offset + region.allocoffset;

		cumulativeSize += region.size;

		batchCounter++;
	}

	dev->WriteToDeviceBufferBatch(globalDeviceBufIndex, stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, batchCounter, rbo);
}

void RenderInstance::InvokeTransferCommands(RecordingBufferObject* rbo)
{
	int memCount = transferCommandPool.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	
	TransferCommand region;
	int link = transferCommandPool.linkHead;
	int* linkprev = &transferCommandPool.linkHead;

	while (link >= 0)
	{
		link = transferCommandPool.PopLink(&region, link, &linkprev);

		int handle = region.allocationIndex;

		size_t intSize = region.size;

		size_t rsize = allocations[handle].requestedSize;
		size_t align = allocations[handle].alignment;

		rsize *= allocations[handle].structureCopies;

		rsize = (rsize + align - 1) & ~(align - 1);

		size_t intOffset = allocations[handle].offset + (currentFrame * rsize) + region.offset;

		EntryHandle index = allocations[handle].memIndex;

		rbo->FillBuffer(index, intSize, intOffset, region.fillVal);

		VkBufferMemoryBarrier lbuffMemBarriers{};

		lbuffMemBarriers.buffer = dev->GetBufferHandle(index);
		lbuffMemBarriers.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		lbuffMemBarriers.dstAccessMask = API::ConvertResourceActionToVulkanAccessFlags(region.dstAction);
		lbuffMemBarriers.offset = intOffset;
		lbuffMemBarriers.size = intSize;
		lbuffMemBarriers.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		lbuffMemBarriers.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		lbuffMemBarriers.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

		RBOPipelineBarrierArgs args = {
			.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
			.dstStageMask = API::ConvertBarrierStageToVulkanPipelineStage(region.dstStage),
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &lbuffMemBarriers,
			.imageMemoryBarrierCount = 0,
			.pImageMemoryBarriers = nullptr
		};


		rbo->BindPipelineBarrierCommand(&args);
	}
}

int RenderInstance::GetAllocFromBuffer(size_t size, size_t copiesOfStructure, uint32_t alignment, AllocationType allocType, ComponentFormatType formatType, int storageLocation)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle handleDex = EntryHandle();

	switch (storageLocation)
	{
	case 0:
		alignment = (alignment + minUniformAlignment - 1) & ~((size_t)minUniformAlignment - 1);
		handleDex = globalIndex;
		break;
	case 1:
		alignment = (alignment + minStorageAlignment - 1) & ~((size_t)minStorageAlignment - 1);
	case 2:
		handleDex = globalDeviceBufIndex;
		break;
	}

	size_t allocSize = ((copiesOfStructure * size) + alignment - 1) & ~((size_t)alignment - 1);

	size_t copies = 1;

	switch (allocType)
	{
	case AllocationType::STATIC:
		break;
	case AllocationType::PERFRAME:
		copies = MAX_FRAMES_IN_FLIGHT;
		break;
	case AllocationType::PERDRAW:
		break;
	}

	size_t location = dev->GetMemoryFromBuffer(handleDex, allocSize * copies, alignment);

	int index = allocations.Allocate();
	allocations.allocations[index].memIndex = handleDex;
	allocations.allocations[index].offset = location;
	allocations.allocations[index].deviceAllocSize = allocSize * copies;
	allocations.allocations[index].requestedSize = size;
	allocations.allocations[index].alignment = alignment;
	allocations.allocations[index].allocType = allocType;
	allocations.allocations[index].formatType = formatType;
	allocations.allocations[index].structureCopies = copiesOfStructure;

	if (formatType != ComponentFormatType::NO_BUFFER_FORMAT && formatType != ComponentFormatType::RAW_8BIT_BUFFER)
	{
		allocations.allocations[index].viewIndex = dev->CreateBufferView(handleDex, API::ConvertComponentFormatTypeToVulkanFormat(formatType), allocSize, location, copies);
	}

	return index;
}


EntryHandle RenderInstance::CreateImageHandle(
	uint32_t blobSize,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat format, int poolIndex)
{

	assert(poolIndex != 0);

	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle textureIndex = majorDevice->CreateImageHandle(
		blobSize,
		width, height, 1,
		mipLevels, API::ConvertImageFormatToVulkanFormat(format),
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_TYPE_2D);

	majorDevice->AssignSamplerToTexture(textureIndex, samplerIndex);

	return textureIndex;
}

EntryHandle RenderInstance::CreateCubeImageHandle(
	uint32_t blobSize,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat format, int poolIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle textureIndex = majorDevice->CreateCubeMapedImageHandle(
		blobSize,
		width, height, 6,
		mipLevels, API::ConvertImageFormatToVulkanFormat(format),
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT);

	majorDevice->AssignSamplerToTexture(textureIndex, samplerIndex);

	return textureIndex;
}


EntryHandle RenderInstance::CreateStorageImage(
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat type, int poolIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	return majorDevice->CreateStorageImage(
		width, height,
		mipLevels, API::ConvertImageFormatToVulkanFormat(type),
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
	case ImageFormat::B8G8R8A8_UNORM:
		flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
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
    ShaderResourceSet* set = (ShaderResourceSet*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceSet));
   
    ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex];

    ShaderResourceSetTemplate* resourceSet = (ShaderResourceSetTemplate*)graph->GetSet(targetSet);

    set->bindingCount = resourceSet->bindingCount;
    set->layoutHandle = resourceSet->vulkanDescLayout;
    set->setCount = setCount;
	set->barrierCount = 0;
	set->samplerCount = resourceSet->samplerCount;
	set->viewCount = resourceSet->viewCount;
	set->constantsCount = resourceSet->constantsCount;

    uintptr_t* offset = (uintptr_t*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(uintptr_t) * (set->bindingCount));

	int constantIndex = set->bindingCount;

	for (int h = 0; h < set->bindingCount; h++)
	{
		MemoryBarrierType memBarrierType = MemoryBarrierType::MEMORY_BARRIER;

		ShaderResource* resource = (ShaderResource*)graph->GetResource(resourceSet->resourceStart+h);

		if (resource->set != targetSet) continue;

		ShaderResourceHeader* desc = (ShaderResourceHeader*)descriptorManager.shaderResourceInstAllocator.Head();;

		switch (resource->type)
		{
		case ShaderResourceType::SAMPLERSTATE:
		{
			ShaderResourceSampler* image = (ShaderResourceSampler*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceSampler));
			
			image->samplerHandles = nullptr;
			image->samplerCount = 0;
			image->firstSampler = 0;
			
			break;
		}
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		{
			ShaderResourceImage* image = (ShaderResourceImage*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceImage));
			
			image->textureHandles = nullptr;
			image->textureCount = 0;
			image->firstTexture = 0;


			memBarrierType = MemoryBarrierType::IMAGE_BARRIER;
			if (resource->action == ShaderResourceAction::SHADERWRITE || resource->action == ShaderResourceAction::SHADERREADWRITE)
			{
				ImageShaderResourceBarrier* barriers = (ImageShaderResourceBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ImageShaderResourceBarrier)*2);
				barriers[0].dstStage = ConvertShaderStageToBarrierStage(resource->stages);
				barriers[0].dstAction = WRITE_SHADER_RESOURCE;
				barriers[0].type = memBarrierType;

				barriers[1].srcStage = ConvertShaderStageToBarrierStage(resource->stages);
				barriers[1].srcAction = WRITE_SHADER_RESOURCE;
				barriers[1].type = memBarrierType;

				set->barrierCount += 2;
			}
			break;
		}
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLER2D:
		case ShaderResourceType::SAMPLERCUBE:
		{
			ShaderResourceImage* image = (ShaderResourceImage*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceImage));
			
			image->textureHandles = nullptr;
			image->textureCount = 0;
			image->firstTexture = 0;

			memBarrierType = MemoryBarrierType::IMAGE_BARRIER;

			if (resource->action == ShaderResourceAction::SHADERWRITE || resource->action == ShaderResourceAction::SHADERREADWRITE)
			{
				ImageShaderResourceBarrier* barriers = (ImageShaderResourceBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ImageShaderResourceBarrier));
				
				barriers->srcStage = ConvertShaderStageToBarrierStage(resource->stages);
				barriers->srcAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;
				
				set->barrierCount++;
			}
			break;
		}
		case ShaderResourceType::CONSTANT_BUFFER:
		{
			ShaderResourceConstantBuffer* constants = (ShaderResourceConstantBuffer*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceConstantBuffer));
			
			constants->size = resource->size;
			constants->offset = resource->offset;
			constants->stage = resource->stages;
			constants->rangeindex = resource->rangeIndex;

			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:
		{
			memBarrierType = MemoryBarrierType::BUFFER_BARRIER;
			
			ShaderResourceBuffer* bufferResource = (ShaderResourceBuffer*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceBuffer));

			bufferResource->firstBuffer = 0;
			bufferResource->offsets = nullptr;
			bufferResource->allocationIndex = nullptr;

			if (resource->action == ShaderResourceAction::SHADERWRITE || resource->action == ShaderResourceAction::SHADERREADWRITE)
			{
				ShaderResourceBarrier* barriers = (ShaderResourceBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceBarrier));
				
				barriers->srcStage = ConvertShaderStageToBarrierStage(resource->stages);
				barriers->srcAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;
			
				set->barrierCount++;
			}
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			ShaderResourceBufferView* bufferResource = (ShaderResourceBufferView*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceBufferView));

			bufferResource->firstView = 0;
			bufferResource->viewCount = 0;
			bufferResource->allocationIndex = nullptr;

			if (resource->action == ShaderResourceAction::SHADERWRITE || resource->action == ShaderResourceAction::SHADERREADWRITE)
			{
				ShaderResourceBarrier* barriers = (ShaderResourceBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceBarrier));

				barriers->srcStage = ConvertShaderStageToBarrierStage(resource->stages);
				barriers->srcAction = WRITE_SHADER_RESOURCE;
				barriers->type = memBarrierType;
			
				set->barrierCount++;
			}
			break;
		}
		}

		if (resource->binding != ~0)
			desc->binding = resource->binding;
		else
			desc->binding = --constantIndex;

		desc->type = resource->type;
		desc->action = resource->action;
		desc->arrayCount = resource->arrayCount;

		offset[desc->binding] = (uintptr_t)desc;
    }

	return descriptorManager.AddShaderToSets((uintptr_t)set);
}





void RenderInstance::CreateVulkanRenderer(WindowManager* window, std::string* shaderGraphLayouts, int shaderGraphLayoutsCount, std::string* pipelineDescriptions, int pipelineDescriptionsCount)
{
	this->windowMan = window;

	void* driverInstanceDataHead = storageAllocator->Allocate(64+(800 * KiB));
	void* instanceDataHead = storageAllocator->Allocate((256 * KiB + 512 * KiB));

	vkInstance->SetInstanceDataAndSize(driverInstanceDataHead, 64+(800 * KiB), 256 * KiB);
	vkInstance->CreateRenderInstance(WINDOWS, instanceDataHead, 512*KiB, 256*KiB);

	OSWindowInternalData data;

	windowMan->GetInternalData(&data);

	vkInstance->CreateWindowedSurface(data.inst, data.wnd);

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
	feature12.drawIndirectCount = VK_TRUE;
	feature12.runtimeDescriptorArray = VK_TRUE;
	//feature12.d

	VkPhysicalDeviceFeatures2 features2{};
	features2.pNext = &feature12;
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.features.geometryShader = VK_TRUE;
	features2.features.textureCompressionBC = VK_TRUE;
	features2.features.tessellationShader = VK_TRUE;
	features2.features.samplerAnisotropy = VK_TRUE;
	features2.features.multiDrawIndirect = VK_TRUE;
	features2.features.wideLines = VK_TRUE;

	void* driverDeviceDataHead = storageAllocator->Allocate((12 * MiB) + (16 * KiB));
	void* deviceDataHead = storageAllocator->Allocate(64 * KiB + (96 * KiB) + 64);

	majorDevice->CreateLogicalDevice(vkInstance->instanceLayers,
		vkInstance->instanceLayerCount,
		vkInstance->deviceExtensions,
		vkInstance->deviceExtCount,
		VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT,
		&features2, vkInstance->renderSurface,
		64 * KiB,
		8 * KiB,
		96 * KiB,
		12 * MiB,
		16 * KiB,
		driverDeviceDataHead,
		deviceDataHead
	);



	std::array<VkFormat, 3> formats = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,  };

	VkFormat depthFormatVK = VK::Utils::findSupportedFormat(majorDevice->gpu,
		formats.data(),
		formats.size(),
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);


	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		stagingBuffers[i] = majorDevice->CreateHostBuffer(128 * MiB, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	}



	swapChainIndex = majorDevice->CreateSwapChain(3, MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT, maxMSAALevels);


	VKSwapChain* swapChain = majorDevice->GetSwapChain(swapChainIndex);

	VkFormat swcFormat = swapChain->GetSwapChainFormat();



	depthFormat = API::ConvertVkFormatToAppFormat(depthFormatVK);

	colorFormat = API::ConvertVkFormatToAppFormat(swcFormat);

	CreateImagePool(128 * MiB, colorFormat, 4096, 4096, true);

	CreateImagePool(128 * MiB, depthFormat, 4096, 4096, true);

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
		128 * MiB, true,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	);

	globalDeviceBufIndex = majorDevice->CreateDeviceBuffer(64 * MiB,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	CreatePipelines(shaderGraphLayouts, shaderGraphLayoutsCount, pipelineDescriptions, pipelineDescriptionsCount);

	computeGraphIndex = majorDevice->CreateComputeGraph(0, 50, 0);

	graphicsOTQ = majorDevice->CreateGraphicsOneTimeQueue(50);

	computeOTQ = majorDevice->CreateComputeOneTimeQueue(50);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		EntryHandle* lprimaryCommandBuffers = majorDevice->CreateReusableCommandBuffers(1, true, COMPUTEQUEUE | TRANSFERQUEUE | GRAPHICSQUEUE, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		currentCBIndex[i] = *lprimaryCommandBuffers;
	}


	samplerIndex = majorDevice->CreateSampler(7);

	cacheAllocator->Reset();
}


#define MAX_DYNAMIC_VK_OFFSETS 100

void RenderInstance::CreateRenderTargetData(int* desc, int descCount)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	EntryHandle* descIDs = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * descCount);
	uint32_t* dynamicPerSet = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * descCount);

	uint32_t dynamicSize = 0;

	uint32_t* dynamicOffsets = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * MAX_DYNAMIC_VK_OFFSETS);

	for (int i = 0; i < descCount; i++)
	{
		descIDs[i] = CreateShaderResourceSet(desc[i]);
		uint32_t size = GetDynamicOffsetsForDescriptorSet(desc[i], dynamicOffsets, dynamicSize);
		dynamicPerSet[i] = size;
		dynamicSize += size;
	}


	for (uint32_t i = 0; i < maxMSAALevels; i++)
		majorDevice->UpdateRenderGraph(swapchainRenderTargets[i], dynamicOffsets, dynamicSize, descIDs, descCount, dynamicPerSet);
	
}


uint32_t RenderInstance::GetSwapChainHeight()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth()
{
	return vkInstance->GetLogicalDevice(physicalIndex, deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

uint32_t RenderInstance::GetDynamicOffsetsForDescriptorSet(int descriptorSet, uint32_t* dynamicOffsets, uint32_t topOfDynamicOffsets)
{
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	int count = set->bindingCount;

	uint32_t size = 0;

	int top = topOfDynamicOffsets;

	for (int i = 0; i < count; i++)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];

		int arrayCount = (header->arrayCount & DESCRIPTOR_COUNT_MASK);

		switch (header->type)
		{
		
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:
		{
			ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)offsets[i];

			size += arrayCount;
	
			for (int j = 0; j < arrayCount; j++)
			{
				if ((j < buffer->bufferCount) && buffer->allocationIndex)
				{
					auto alloc = allocations[buffer->allocationIndex[j]];
					int offset = (buffer->offsets ? buffer->offsets[j] : 0);
					dynamicOffsets[top] = static_cast<uint32_t>(alloc.offset + offset);
				} 
				else
				{
					dynamicOffsets[top] = 0;
				}
			}

			top += arrayCount;
			
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
	//TODO handle this better
	if (descriptorManager.vkDescriptorSets[descriptorSet] != EntryHandle())
	{
		return descriptorManager.vkDescriptorSets[descriptorSet];
	}

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	int frames = set->setCount;	

	uint32_t varCountRequested = 0;

	int bindingCount = set->bindingCount;

	ShaderResourceHeader* lastheader = (ShaderResourceHeader*)offsets[(set->viewCount + set->samplerCount)-1];

	if (lastheader->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY)
	{
		varCountRequested = (lastheader->arrayCount & DESCRIPTOR_COUNT_MASK);
	}

	DescriptorSetBuilder* builder = dev->CreateDescriptorSetBuilder(descriptorManager.deviceResourceHeap, vulkanDescriptorLayouts[set->layoutHandle], frames, varCountRequested);

	for (int i = 0; i < bindingCount; i++)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];

		switch (header->type)
		{
			case ShaderResourceType::SAMPLERSTATE:
			{
				ShaderResourceSampler* image = (ShaderResourceSampler*)header;
				if (image->samplerHandles)
					builder->AddSamplerDescription(image->samplerHandles, image->samplerCount, image->firstSampler, i, 0, frames);
				break;
			}
			case ShaderResourceType::IMAGE2D:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				if (image->textureHandles)
					builder->AddImageResourceDescription(image->textureHandles, image->textureCount, image->firstTexture, i, 0, frames);
				break;
			}
			case ShaderResourceType::IMAGESTORE2D:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				builder->AddStorageImageDescription(image->textureHandles, image->textureCount, image->firstTexture, i, 0, frames);
				break;
			}
			case ShaderResourceType::SAMPLER3D:
			case ShaderResourceType::SAMPLER2D:
			case ShaderResourceType::SAMPLERCUBE:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				if (image->textureHandles)
					builder->AddCombinedTextureArray(image->textureHandles, image->textureCount, image->firstTexture, i, 0, frames);
				
				break;
			}
			case ShaderResourceType::STORAGE_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)header;
				if (!buffer->allocationIndex) break;
				int arrayCount = buffer->bufferCount;
				int firstBuffer = buffer->firstBuffer;

				for (int j = 0; j < arrayCount; j++)
				{
					auto alloc = allocations[buffer->allocationIndex[j]];

					if (alloc.allocType == AllocationType::PERFRAME)
						builder->AddDynamicStorageBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize / frames, i, frames, 0, 0, firstBuffer + j);
					else
						builder->AddDynamicStorageBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize, i, frames, 0, 0, firstBuffer + j);
					
				}
				break;
			}
			case ShaderResourceType::UNIFORM_BUFFER:
			{
				ShaderResourceBuffer* buffer = (ShaderResourceBuffer*)header;

				if (!buffer->allocationIndex) break;
				int arrayCount = buffer->bufferCount;
				int firstBuffer = buffer->firstBuffer;

				for (int j = 0; j < arrayCount; j++)
				{
					auto alloc = allocations[buffer->allocationIndex[j]];

					if (alloc.allocType == AllocationType::PERFRAME)
						builder->AddDynamicUniformBuffer(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize / frames, i, frames, 0, 0, firstBuffer+j);
					else
						builder->AddDynamicUniformBufferDirect(dev->GetBufferHandle(alloc.memIndex), alloc.deviceAllocSize, i, frames, 0, 0, firstBuffer+j);
				}
				break;
			}
			case ShaderResourceType::BUFFER_VIEW:
			{
				ShaderResourceBufferView* bufferView = (ShaderResourceBufferView*)header;

				if (!bufferView->allocationIndex) break;
				
				int arrayCount = bufferView->viewCount;
				int firstView = bufferView->firstView;

				for (int j = 0; j < arrayCount; j++)
				{
					auto alloc = allocations.allocations[bufferView->allocationIndex[j]];

					int frameCount = (alloc.allocType == AllocationType::PERFRAME) ? MAX_FRAMES_IN_FLIGHT : 1;

					for (int g = 0; g < frameCount; g++)
					{
						VkBufferView handle = dev->GetBufferView(alloc.viewIndex, g);

						if (bufferView->action == ShaderResourceAction::SHADERREAD)
						{
							builder->AddUniformBufferView(handle, bufferView->binding, g, 1, j + firstView);
						}
						else if (bufferView->action == ShaderResourceAction::SHADERWRITE)
						{
							builder->AddStorageBufferView(handle, bufferView->binding, g, 1, j + firstView);
						}
					}
				}
				break;
				
			}
		}
	}

	EntryHandle handle = builder->AddDescriptorsToCache();

	descriptorManager.vkDescriptorSets[descriptorSet] = handle;

	return handle;
}

int RenderInstance::CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo* info, bool addToGraph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t dynamicNumber = 0;
	uint32_t pushRangeCount = 0;
	EntryHandle* descHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle)* (info->descCount));
	uint32_t* offsetsPerSet = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * (info->descCount));
	uint32_t* dynamicOffsets = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * MAX_DYNAMIC_VK_OFFSETS);

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		offsetsPerSet[i] = GetDynamicOffsetsForDescriptorSet(info->descriptorsetid[i], dynamicOffsets, dynamicNumber);
		dynamicNumber += offsetsPerSet[i];
		descHandles[i] = CreateShaderResourceSet(info->descriptorsetid[i]);
		pushRangeCount += descriptorManager.GetConstantBufferCount(info->descriptorsetid[i]);
	}

	EntryHandle indexBufferHandle = EntryHandle();
	uint32_t indexOffset = ~0ui32;

	EntryHandle vertexBufferHandle = EntryHandle();
	uint32_t vertexOffset = ~0ui32;

	EntryHandle indirectBufferHandle = EntryHandle();
	uint32_t indirectOffset = ~0ui32;
	uint32_t indirectBufferPerFrameSize = 0;

	EntryHandle indirectCountBufferHandle = EntryHandle();
	uint32_t indirectCountOffset = ~0ui32;
	uint32_t indirectCountBufferPerFrameSize = 0;

	if (info->indexBufferHandle != ~0)
	{
		indexBufferHandle = allocations[info->indexBufferHandle].memIndex;
		indexOffset = static_cast<uint32_t>(allocations[info->indexBufferHandle].offset) + info->indexOffset;
	}

	if (info->vertexBufferHandle != ~0)
	{
		vertexBufferHandle = allocations[info->vertexBufferHandle].memIndex;
		vertexOffset = static_cast<uint32_t>(allocations[info->vertexBufferHandle].offset) + info->vertexOffset;
	}

	if (info->indirectAllocation != ~0)
	{
		size_t align = allocations[info->indirectAllocation].alignment;
		uint32_t copiesOfstruct = static_cast<uint32_t>(allocations[info->indirectAllocation].structureCopies);
		indirectBufferHandle = allocations[info->indirectAllocation].memIndex;
		indirectOffset = static_cast<uint32_t>(allocations[info->indirectAllocation].offset);
		indirectBufferPerFrameSize = static_cast<uint32_t>(((allocations[info->indirectAllocation].requestedSize * copiesOfstruct) + align - 1) & ~(align - 1)) ;
	}

	if (info->indirectCountAllocation != ~0)
	{
		size_t align = allocations[info->indirectCountAllocation].alignment;
		uint32_t copiesOfstruct = static_cast<uint32_t>(allocations[info->indirectCountAllocation].structureCopies);
		indirectCountBufferHandle = allocations[info->indirectCountAllocation].memIndex;
		indirectCountOffset = static_cast<uint32_t>(allocations[info->indirectCountAllocation].offset);
		indirectCountBufferPerFrameSize = static_cast<uint32_t>(((allocations[info->indirectCountAllocation].requestedSize * copiesOfstruct) + align - 1) & ~(align - 1));
	}

	VKGraphicsPipelineObjectCreateInfo create = 
	{
			.vertexBufferIndex = vertexBufferHandle,
			.vertexBufferOffset = vertexOffset,
			.vertexCount = info->vertexCount,
			.pipelinename = EntryHandle(),
			.descCount = info->descCount,
			.descriptorsetid = descHandles,
			.dynamicPerSet = offsetsPerSet,
			.maxDynCap = dynamicNumber,
			.indexBufferHandle = indexBufferHandle,
			.indexBufferOffset = indexOffset,
			.indexCount = info->indexCount,
			.pushRangeCount = pushRangeCount,
			.instanceCount = info->instanceCount,
			.indexSize = info->indexSize,
			.indirectDrawCount = static_cast<uint32_t>(info->indirectDrawCount),
			.indirectBufferHandle = indirectBufferHandle,
			.indirectBufferOffset = indirectOffset,
			.indirectBufferFrames = indirectBufferPerFrameSize,
			.indirectCountBufferHandle = indirectCountBufferHandle,
			.indirectCountBufferOffset = indirectCountOffset,
			.indirectCountBufferStride = indirectCountBufferPerFrameSize
	};

	int renderStateCount = maxMSAALevels;

	auto pso = stateObjectHandles.Allocate();

	int ret = pso.first;
	PipelineHandle* posStruct = pso.second;

	posStruct->graphIndex = graphIndices.AllocateN(renderStateCount);
	posStruct->graphCount = renderStateCount;
	posStruct->numHandles = renderStateCount;
	posStruct->group = GRAPHICSO;
	posStruct->indexForHandles = renderStateObjects.AllocateN(renderStateCount);

	int pipeInsertIndex = posStruct->indexForHandles;
	int graphInsertIndex = posStruct->graphIndex;

	auto& ref = pipelinesIdentifier[info->pipelinename];

	for (uint32_t i = 0; i < maxMSAALevels; i++) {

		create.pipelinename = ref[i];

		EntryHandle pipelineIndex = dev->CreateGraphicsPipelineObject(&create);

		VKPipelineObject* vkPipelineObject = dev->GetPipelineObject(pipelineIndex);

		vkPipelineObject->SetPerObjectData(dynamicOffsets, dynamicNumber);

		for (uint32_t i = 0, j = 0, constantBufferPerSet = 0; i < pushRangeCount && j < info->descCount;)
		{
			ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid[j], constantBufferPerSet++);
			if (!pushArgs)
			{
				j++;
				constantBufferPerSet = 0;
				continue;
			}
		
			vkPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, API::ConvertShaderStageToVulkanShaderStage(pushArgs->stage));
			i++;
		}

		
		AddVulkanMemoryBarrier(vkPipelineObject, info->descriptorsetid, info->descCount);
		
		if (addToGraph)
		{
			auto graph = dev->GetRenderGraph(swapchainRenderTargets[i]);
			graphIndices.Update(graphInsertIndex++, (int)graph->AddObject(pipelineIndex));
		}
		
		renderStateObjects.Update(pipeInsertIndex++, pipelineIndex);
	}

	return ret;
}

ShaderComputeLayout* RenderInstance::GetComputeLayout(int shaderGraphIndex)
{
	ShaderGraph* graph = vulkanShaderGraphs.shaderGraphPtrs[shaderGraphIndex];
	ShaderMap* map = (ShaderMap*)graph->GetMap(0);
	ShaderDetails* deats = vulkanShaderGraphs.shaderDetails[map->shaderReference];
	return (ShaderComputeLayout*)deats->GetShaderData();
}

void RenderInstance::SetActiveComputePipeline(uint32_t objectIndex, bool active)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	auto graph = dev->GetComputeGraph(computeGraphIndex);
	graph->SetActive(objectIndex, active);
}

int RenderInstance::CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	uint32_t dynamicNumber = 0;
	uint32_t barrierCount = 0;
	uint32_t pushRangeCount = 0;
	EntryHandle* descHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * (info->descCount));
	uint32_t* offsetsPerSet = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * (info->descCount));
	uint32_t* dynamicOffsets = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * MAX_DYNAMIC_VK_OFFSETS);

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		offsetsPerSet[i] = GetDynamicOffsetsForDescriptorSet(info->descriptorsetid[i], dynamicOffsets, dynamicNumber);
		dynamicNumber += offsetsPerSet[i];
		descHandles[i] = CreateShaderResourceSet(info->descriptorsetid[i]);
		barrierCount += descriptorManager.GetBarrierCount(info->descriptorsetid[i]);
		pushRangeCount += descriptorManager.GetConstantBufferCount(info->descriptorsetid[i]);
	}

	VKComputePipelineObjectCreateInfo create = {
		.x = info->x,
		.y = info->y,
		.z = info->z,
		.descCount = info->descCount,
		.descriptorId = descHandles,
		.dynamicPerSet = offsetsPerSet,
		.pipelineId = pipelinesIdentifier[info->pipelinename][0],
		.maxDynCap = dynamicNumber,
		.barrierCount = barrierCount,
		.pushRangeCount = pushRangeCount
	};

	auto pso = stateObjectHandles.Allocate();

	int ret = pso.first;
	PipelineHandle* posStruct = pso.second;

	EntryHandle pipelineIndex = dev->CreateComputePipelineObject(&create);

	
	posStruct->graphCount = 1;
	posStruct->numHandles = 1;
	posStruct->group = COMPUTESO;
	posStruct->indexForHandles = computeStateObjects.Allocate(pipelineIndex);

	VKPipelineObject* vkPipelineObject = dev->GetPipelineObject(pipelineIndex);

	vkPipelineObject->SetPerObjectData(dynamicOffsets, dynamicNumber);

	for (uint32_t i = 0, j = 0, constantBufferPerSet = 0; i < pushRangeCount && j < info->descCount;)
	{
		ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(info->descriptorsetid[j], constantBufferPerSet++);
		if (!pushArgs)
		{
			j++;
			constantBufferPerSet = 0;
			continue;
		}
		vkPipelineObject->AddPushConstant(pushArgs->data, pushArgs->size, pushArgs->offset, i, API::ConvertShaderStageToVulkanShaderStage(pushArgs->stage));
		i++;
	}
	
	
	AddVulkanMemoryBarrier(vkPipelineObject, info->descriptorsetid, info->descCount);
	
	auto graph = dev->GetComputeGraph(computeGraphIndex);

	posStruct->graphIndex = graphIndices.Allocate((int)graph->AddObject(pipelineIndex));

	return ret;
}

void RenderInstance::AddVulkanMemoryBarrier(VKPipelineObject *vkPipelineObject, int* descriptorid, int descriptorcount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	for (int i = 0; i<descriptorcount; i++)
	{ 
		uintptr_t head = descriptorManager.descriptorSets[descriptorid[i]];
		ShaderResourceSet* set = (ShaderResourceSet*)head;

		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		int counter = 0;

		while (counter < set->bindingCount)
		{
			ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[counter++];
			if (header->action == ShaderResourceAction::SHADERWRITE || header->action == ShaderResourceAction::SHADERREADWRITE)
			{
				switch (header->type)
				{
				case ShaderResourceType::IMAGESTORE2D:
				{
					ShaderResourceImage* imageBarrier = (ShaderResourceImage*)header;
					ImageShaderResourceBarrier* barrier = (ImageShaderResourceBarrier*)(imageBarrier + 1);
					ImageShaderResourceBarrier* barrier2 = &barrier[1];

					VkImageSubresourceRange range{};
					range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					range.levelCount = 1;
					range.layerCount = 1;

					VkAccessFlags srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->srcAction);
					VkAccessFlags dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->dstAction);
					VkShaderStageFlags srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->srcStage);
					VkShaderStageFlags dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->dstStage);

					if (barrier->dstResourceLayout != barrier->srcResourceLayout)
					{

						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier->dstResourceLayout), *imageBarrier->textureHandles, range);
						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							BEFORE,
							srcStage,
							dstStage
						);
					}


					srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier2->srcAction);
					dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier2->dstAction);
					srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier2->srcStage);
					dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier2->dstStage);

					if (barrier2->dstResourceLayout != barrier2->srcResourceLayout)
					{


						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier2->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier2->dstResourceLayout), *imageBarrier->textureHandles, range);

						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							AFTER,
							srcStage,
							dstStage
						);
					}

					break;
				}
				case ShaderResourceType::SAMPLER2D:
				{
					ShaderResourceImage* imageBarrier = (ShaderResourceImage*)header;

					ImageShaderResourceBarrier* barrier = (ImageShaderResourceBarrier*)(imageBarrier + 1);

					VkImageSubresourceRange range{};

					range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					range.levelCount = 1;
					range.layerCount = 1;

					VkAccessFlags srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->srcAction);
					VkAccessFlags dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->dstAction);
					VkShaderStageFlags srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->srcStage);
					VkShaderStageFlags dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->dstStage);

					if (barrier->dstResourceLayout != barrier->srcResourceLayout)
					{

						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier->dstResourceLayout), *imageBarrier->textureHandles, range);
						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							BEFORE,
							srcStage,
							dstStage
						);
					}

					break;
				}
				case ShaderResourceType::BUFFER_VIEW:
				{
					ShaderResourceBufferView* bufferBarrier = (ShaderResourceBufferView*)header;

					if (bufferBarrier->allocationIndex == nullptr) break;

					int arrayCount = bufferBarrier->arrayCount;

					ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)(bufferBarrier + 1);

					VkAccessFlags srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->srcAction);
					VkAccessFlags dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->dstAction);
					VkShaderStageFlags srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->srcStage);
					VkShaderStageFlags dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->dstStage);
					for (int g = 0; g < arrayCount; g++)
					{
						int index = bufferBarrier->allocationIndex[g];
						size_t size = allocations[index].deviceAllocSize;
						size_t copiesOfStruct = allocations[index].structureCopies;
						uint32_t pfo = 0;


						if (allocations[index].allocType == AllocationType::PERFRAME)
						{

							size = ((allocations[index].requestedSize * copiesOfStruct) + allocations[index].alignment - 1) & ~(allocations[index].alignment - 1);
							pfo = static_cast<uint32_t>(size);
						}



						EntryHandle barrierIndex = dev->CreateBufferMemoryBarrier(srcAction, dstAction, 0, 0,
							allocations[index].memIndex,
							allocations[index].offset,
							size
						);


						vkPipelineObject->AddBufferMemoryBarrier(
							barrierIndex,
							AFTER,
							srcStage,
							dstStage,
							pfo
						);
					}

					break;
				}
				case ShaderResourceType::STORAGE_BUFFER:
				case ShaderResourceType::UNIFORM_BUFFER:
				{
					ShaderResourceBuffer* bufferBarrier = (ShaderResourceBuffer*)header;

					if (bufferBarrier->allocationIndex == nullptr) break;

					int arrayCount = bufferBarrier->arrayCount;

					ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)(bufferBarrier + 1);

					VkAccessFlags srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->srcAction);
					VkAccessFlags dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier->dstAction);
					VkShaderStageFlags srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->srcStage);
					VkShaderStageFlags dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier->dstStage);


					for (int g = 0; g < arrayCount; g++)
					{
						int index = bufferBarrier->allocationIndex[g];
						size_t size = allocations[index].deviceAllocSize;
						size_t copiesOfStruct = allocations[index].structureCopies;
						uint32_t pfo = 0;
						VkDeviceSize offset = (bufferBarrier->offsets ? bufferBarrier->offsets[g] : 0);

						if (allocations[index].allocType == AllocationType::PERFRAME)
						{
							size = ((allocations[index].requestedSize * copiesOfStruct) + allocations[index].alignment - 1) & ~(allocations[index].alignment - 1);
							pfo = static_cast<uint32_t>(size);
						}
						else
						{
							size -= offset;
						}


						EntryHandle barrierIndex = dev->CreateBufferMemoryBarrier(srcAction, dstAction, 0, 0,
							allocations[index].memIndex,
							allocations[index].offset + offset,
							size
						);


						vkPipelineObject->AddBufferMemoryBarrier(
							barrierIndex,
							AFTER,
							srcStage,
							dstStage,
							pfo
						);
					}
					

					break;
				}
				}
			}
		}
	}
}

void RenderInstance::DrawScene(uint32_t imageIndex)
{

	SwapUpdateCommands();

	UploadHostTransfers();

	UploadDescriptorsUpdates();

	EntryHandle cbindex = currentCBIndex[currentFrame];

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);

	auto graph = dev->GetRenderGraph(swapchainRenderTargets[currentMSAALevel]);
	auto cGraph = dev->GetComputeGraph(computeGraphIndex);

	auto rcb = dev->GetRecordingBufferObject(cbindex);

	VKComputeOneTimeQueue* queue2 = dev->GetComputeOTQ(computeOTQ);

	rcb.ResetCommandPoolForBuffer();

	dev->ResetBufferAllocator(stagingBuffers[currentFrame]);

	rcb.BeginRecordingCommand(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	UploadDeviceLocalTransfers(&rcb);

	InvokeTransferCommands(&rcb);

	UploadImageMemoryTransfers(&rcb);

//	cGraph->DispatchWork(&rcb, currentFrame);

	queue2->DispatchWork(&rcb, currentFrame);

	VkExtent2D* rect = &swc->swapChainExtent;

	rcb.BeginRenderPassCommand(swc->renderTargetIndex[currentMSAALevel], imageIndex, VK_SUBPASS_CONTENTS_INLINE, {{0, 0}, *rect});

	float x = static_cast<float>(rect->width), y = static_cast<float>(rect->height);

	rcb.SetViewportCommand(0, 0, x, y, 0.0f, 1.0f);

	rcb.SetScissorCommand(0, 0, rect->width, rect->height);

	VKGraphicsOneTimeQueue* queue = dev->GetGraphicsOTQ(graphicsOTQ);

	queue->DrawScene(&rcb, currentFrame);

	graph->DrawScene(&rcb, currentFrame);

	rcb.EndRenderPassCommand();

	rcb.EndRecordingCommand();
}




void RenderInstance::IncreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	uint32_t next = currentMSAALevel + 1;
	if (next >= maxMSAALevels)
		return;
	currentMSAALevel = next;
}

void RenderInstance::DecreaseMSAA()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);
	int32_t next = currentMSAALevel - 1;
	if (next < 0)
		return;
	currentMSAALevel = next;

}

void RenderInstance::EndFrame()
{
	EntryHandle cbindex = currentCBIndex[currentFrame];

	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	auto graph = dev->GetRenderGraph(swapchainRenderTargets[currentMSAALevel]);
	auto cGraph = dev->GetComputeGraph(computeGraphIndex);
	VKGraphicsOneTimeQueue* queue = dev->GetGraphicsOTQ(graphicsOTQ);
	VKComputeOneTimeQueue* queue2 = dev->GetComputeOTQ(computeOTQ);

	cGraph->UpdateLists();
	graph->UpdateLists();
	queue->UpdateQueue();
	queue2->UpdateQueue();

	cacheAllocator->Reset();
}


void RenderInstance::AddPipelineToMainQueue(int psoIndex, int computeorgraphics)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	PipelineHandle* handle = stateObjectHandles.Update(psoIndex);

	if (computeorgraphics)
	{
		EntryHandle ret = renderStateObjects.dataArray[handle->indexForHandles + currentMSAALevel];

		VKGraphicsOneTimeQueue* queue = dev->GetGraphicsOTQ(graphicsOTQ);

		queue->AddObject(ret);
	} 
	else
	{
		EntryHandle ret = computeStateObjects.dataArray[handle->indexForHandles];

		VKComputeOneTimeQueue* queue = dev->GetComputeOTQ(computeOTQ);

		queue->AddObject(ret);
	}


}

void RenderInstance::ReadData(int handle, void* dest, int size, int offset)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(physicalIndex, deviceIndex);

	
	size_t intOffset = allocations[handle].offset;
	

	EntryHandle index = allocations[handle].memIndex;

	if (index == globalIndex)
		dev->ReadHostBuffer(dest, index, size, intOffset+offset);

}



void RenderInstance::UpdateDriverMemory(void* data, int allocationIndex, int size, int allocOffset, TransferType transferType)
{
	void* outData = data;

	int copies = 1;

	if (transferType == TransferType::CACHED)
	{
		outData = updateCommandsCache->Allocate(size);
		memcpy(outData, data, size);
	}

	if (allocations[allocationIndex].allocType == AllocationType::PERFRAME)
	{
		copies = MAX_FRAMES_IN_FLIGHT;
	}

	RenderDriverUpdateCommandMemory* rducm = (RenderDriverUpdateCommandMemory*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandMemory));

	rducm->allocationIndex = allocationIndex;
	rducm->allocOffset = allocOffset;
	rducm->copiesWithin = copies;
	rducm->size = size;
	rducm->data = outData;
	rducm->updateType = DriverUpdateType::MEMORYUPDATE;
}

void RenderInstance::UpdateImageMemory(void* data, EntryHandle textureIndex, uint32_t* imageSizes, size_t totalSize, int width, int height, int mipLevels, int layers, ImageFormat format)
{
	uint32_t* cachedImageSizes = imageSizes;
	
	if (cachedImageSizes)
	{
		cachedImageSizes = (uint32_t*)updateCommandsCache->Allocate(mipLevels * sizeof(uint32_t));

		memcpy(cachedImageSizes, imageSizes, mipLevels * sizeof(uint32_t));
	}

	RenderDriverUpdateCommandImage* rduci = (RenderDriverUpdateCommandImage*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandImage));

	rduci->data = data;
	rduci->format = format;
	rduci->height = height;
	rduci->mipLevels = mipLevels;
	rduci->width = width;
	rduci->imageSizes = imageSizes;
	rduci->layers = layers;
	rduci->textureIndex = textureIndex;
	rduci->updateType = DriverUpdateType::IMAGEMEMORYUPDATE;
	rduci->totalSize = totalSize;
}

void RenderInstance::InsertTransferCommand(int allocationIndex, int size, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action)
{
	RenderDriverUpdateCommandFill* rducf = (RenderDriverUpdateCommandFill*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandFill));

	int copies = 1;

	if (allocations[allocationIndex].allocType == AllocationType::PERFRAME)
	{
		copies = MAX_FRAMES_IN_FLIGHT;
	}


	rducf->action = action;
	rducf->allocationIndex = allocationIndex;
	rducf->stage = stage;
	rducf->allocOffset = allocOffset;
	rducf->fillValue = fillValue;
	rducf->stage = stage;
	rducf->size = size;
	rducf->updateType = DriverUpdateType::TRANSFERCOMMAND;
	rducf->copiesWithin = copies;

}

void RenderInstance::UpdateShaderResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, ResourceArrayUpdate* resourceArrayData)
{
	RenderDriverUpdateCommandResource* rducr = (RenderDriverUpdateCommandResource*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandResource));

	int argSize = 0;

	void* argData = nullptr;

	int resCount = resourceArrayData->resourceCount;

	switch (type)
	{
	case ShaderResourceType::SAMPLER3D:
	case ShaderResourceType::SAMPLER2D:
	case ShaderResourceType::SAMPLERSTATE:
	case ShaderResourceType::IMAGE2D:
	{
		ResourceArrayUpdate* cachedUpdate = (ResourceArrayUpdate*)(updateCommandsCache->Allocate(sizeof(ResourceArrayUpdate)));
		
		cachedUpdate->resourceDstBegin = resourceArrayData->resourceDstBegin;
		cachedUpdate->resourceCount = resCount;
		cachedUpdate->resourceHandles = (EntryHandle*)(updateCommandsCache->Allocate(sizeof(EntryHandle) * resCount));
		
		memcpy(cachedUpdate->resourceHandles, resourceArrayData->resourceHandles, sizeof(EntryHandle) * resCount);
		
		argData = cachedUpdate;
		argSize = (sizeof(EntryHandle) * resCount) + sizeof(ResourceArrayUpdate);
		break;
	}
	}

	ShaderResourceSet* set = (ShaderResourceSet*)descriptorManager.descriptorSets[descriptorid];

	rducr->bindingindex = bindingindex;
	rducr->updateType = DriverUpdateType::RESOURCEUPDATE;
	rducr->descriptorid = descriptorid;
	rducr->type = type;
	rducr->cachedDataSize = argSize;
	rducr->data = argData;
	rducr->copies = set->setCount;

}

void RenderInstance::SwapUpdateCommands()
{
	int drainBuffer = currentUpdateCommandBuffer;

	currentUpdateCommandBuffer = (currentUpdateCommandBuffer ^ 1);

	RenderDriverUpdateCommandHeader* header = (RenderDriverUpdateCommandHeader*)updateCommandBuffers[drainBuffer]->dataHead;

	size_t currentSize = updateCommandBuffers[drainBuffer]->dataAllocator.load();

	while (currentSize)
	{
		switch (header->updateType)
		{
		case DriverUpdateType::RESOURCEUPDATE:
		{
			RenderDriverUpdateCommandResource* rducr = (RenderDriverUpdateCommandResource*)header;
			descriptorUpdatePool.Create(rducr->descriptorid, rducr->bindingindex, rducr->type, rducr->data, rducr->cachedDataSize, rducr->copies);
			header = rducr->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandResource);
			break;
		}
		case DriverUpdateType::TRANSFERCOMMAND:
		{
			RenderDriverUpdateCommandFill* rducf = (RenderDriverUpdateCommandFill*)header;
			transferCommandPool.Create(rducf->allocationIndex, rducf->size, rducf->allocOffset, rducf->fillValue, rducf->stage, rducf->action, rducf->copiesWithin);
			header = rducf->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandFill);
			break;
		}
		case DriverUpdateType::IMAGEMEMORYUPDATE:
		{
			RenderDriverUpdateCommandImage* rduci = (RenderDriverUpdateCommandImage*)header;
			imageMemoryUpdateManager.Create(rduci->data, rduci->textureIndex, rduci->imageSizes, rduci->totalSize, rduci->width, rduci->height, rduci->mipLevels, rduci->layers, rduci->format);
			header = rduci->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandImage);
			break;
		}
		case DriverUpdateType::MEMORYUPDATE:
		{
			RenderDriverUpdateCommandMemory* rducm = (RenderDriverUpdateCommandMemory*)header;
			

			if (allocations.allocations[rducm->allocationIndex].memIndex == globalIndex)
			{
				driverHostMemoryUpdater.Create(rducm->data, rducm->size, rducm->allocationIndex, rducm->allocOffset, rducm->copiesWithin);
			}
			else if (allocations.allocations[rducm->allocationIndex].memIndex == globalDeviceBufIndex)
			{
				driverDeviceMemoryUpdater.Create(rducm->data, rducm->size, rducm->allocationIndex, rducm->allocOffset, rducm->copiesWithin);
			}

			header = rducm->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandMemory);

			break;
		}
		}
	}

	updateCommandBuffers[drainBuffer]->Reset();
}

