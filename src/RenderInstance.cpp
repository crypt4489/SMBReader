#include "RenderInstance.h"

#include <algorithm>
#include <bit>
#include <limits>

#include "FileManager.h"
#include "ShaderResourceSet.h"
#include "ThreadManager.h"

#include "VKInstance.h"
#include "VKDevice.h"
#include "VKDescriptorLayoutBuilder.h"
#include "VKDescriptorSetBuilder.h"
#include "VKRenderPassBuilder.h"
#include "VKSwapChain.h"
#include "VKPipelineBuilder.h"

namespace GlobalRenderer 
{
	RenderInstance gRenderInstance;
}

namespace API 
{
	VkImageUsageFlags ConvertAttachmentResourceUsageToVkImageUsage(AttachmentResourceInstanceUsage usage)
	{
		VkImageUsageFlags flags = 0;

		switch (usage)
		{
		case AttachmentResourceInstanceUsage::COLOR_ATTACHMENT_USAGE:
			flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			break;

		case AttachmentResourceInstanceUsage::DEPTH_ATTACHMENT_USAGE:
			flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;

		case AttachmentResourceInstanceUsage::STENCIL_ATTACHMENT_USAGE:
			flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;

		case AttachmentResourceInstanceUsage::RESOLVE_ATTACHMENT_USAGE:
			flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			break;

		case AttachmentResourceInstanceUsage::PRESERVE_ATTACHMENT_USAGE:
			flags = 0; // no direct Vulkan usage flag
			break;

		case AttachmentResourceInstanceUsage::INPUT_ATTACHMENT_USAGE:
			flags = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			break;

		default:
			flags = 0;
			break;
		}

		return flags;
	}

	VkAttachmentLoadOp ConvertAttachLoadOpToVulkanLoadOp(AttachmentLoadUsage loadOp)
	{
		VkAttachmentLoadOp ret = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		switch (loadOp)
		{
		case AttachmentLoadUsage::ATTACHNOCARE:
			break;
		case AttachmentLoadUsage::ATTACHCLEAR:
			ret = VK_ATTACHMENT_LOAD_OP_CLEAR;
			break;
		}
		return ret;
	}

	VkAttachmentStoreOp ConvertAttachStoreOpToVulkanStoreOp(AttachmentStoreUsage storeOp)
	{
		VkAttachmentStoreOp ret = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		switch (storeOp)
		{
		case AttachmentStoreUsage::ATTACHDISCARD:
			break;
		case AttachmentStoreUsage::ATTACHSTORE:
			ret = VK_ATTACHMENT_STORE_OP_STORE;
			break;
		}
		return ret;
	}

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
		flags |= (VK_ACCESS_INDIRECT_COMMAND_READ_BIT) * ((action & READ_INDIRECT_COMMAND) != 0);
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
		case ImageLayout::PRESENT:
			outLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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

	void ConvertGPUFeatureRequestToVkPhysicalDeviceProperties(
		const GPUFeatureRequest* request,
		VkPhysicalDeviceFeatures2* features2,
		VkPhysicalDeviceVulkan12Features* features12)
	{
	
		features12->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12->pNext = nullptr;

		features12->descriptorBindingPartiallyBound =
			request->requireDescriptorBindingPartiallyBound ? VK_TRUE : VK_FALSE;
		features12->descriptorBindingSampledImageUpdateAfterBind =
			request->requireDescriptorBindingSampledImageUpdateAfterBind ? VK_TRUE : VK_FALSE;
		features12->descriptorBindingUpdateUnusedWhilePending =
			request->requireDescriptorBindingUpdateUnusedWhilePending ? VK_TRUE : VK_FALSE;
		features12->descriptorBindingVariableDescriptorCount =
			request->requireDescriptorBindingVariableDescriptorCount ? VK_TRUE : VK_FALSE;
		features12->shaderSampledImageArrayNonUniformIndexing =
			request->requireShaderSampledImageArrayNonUniformIndexing ? VK_TRUE : VK_FALSE;
		features12->storageBuffer8BitAccess =
			request->requireStorageBuffer8BitAccess ? VK_TRUE : VK_FALSE;
		features12->drawIndirectCount =
			request->requireDrawIndirectCount ? VK_TRUE : VK_FALSE;
		features12->runtimeDescriptorArray =
			request->requireRuntimeDescriptorArray ? VK_TRUE : VK_FALSE;
		features12->timelineSemaphore = request->requireTimelineSemaphores ? VK_TRUE : VK_FALSE;

		features2->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2->pNext = features12;

		features2->features.geometryShader =
			request->requireGeometryShader ? VK_TRUE : VK_FALSE;
		features2->features.textureCompressionBC =
			request->requireTextureCompressionBC ? VK_TRUE : VK_FALSE;
		features2->features.tessellationShader =
			request->requireTessellationShader ? VK_TRUE : VK_FALSE;
		features2->features.samplerAnisotropy =
			request->requireSamplerAnisotropy ? VK_TRUE : VK_FALSE;
		features2->features.multiDrawIndirect =
			request->requireMultiDrawIndirect ? VK_TRUE : VK_FALSE;
		features2->features.wideLines =
			request->requireWideLines ? VK_TRUE : VK_FALSE;
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		return VK_FALSE;
	}

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}


void RenderInstance::CreateRenderInstance(RenderInstanceCreateInfo* info, SlabAllocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator)
{
	minStorageAlignment = 0;
	minUniformAlignment = 0;

	vkInstance = (VKInstance*)instanceStorageAllocator->Allocate(sizeof(VKInstance), alignof(VKInstance));

	cacheAllocator = instanceCacheAllocator;
	storageAllocator = instanceStorageAllocator;

	updateCommandsCache = (RingAllocator*)instanceStorageAllocator->Allocate(sizeof(RingAllocator), alignof(RingAllocator));
	std::construct_at(updateCommandsCache, instanceStorageAllocator->Allocate(info->commandsCacheSize, 64), info->commandsCacheSize);

	for (uint32_t i = 0; i < 2; i++)
	{
		updateCommandBuffers[i] = (SlabAllocator*)instanceStorageAllocator->Allocate(sizeof(SlabAllocator));
		std::construct_at(updateCommandBuffers[i], instanceStorageAllocator->Allocate(info->commandBuffersSize, 32), info->commandBuffersSize);
	}

	int driverHostLinkedSize = driverHostMemoryUpdater.GetSize(info->numberOfDriverHostAllocations);
	int commandLinkedSize = transferCommandPool.GetSize(info->numberOfTransferCommandAllocations);
	int resourceUpdateLinkedSize = descriptorUpdatePool.GetSize(info->numberOfResourceUpdateAllocations);
	int driverDeviceLinkedSize = driverDeviceMemoryUpdater.GetSize(info->numberOfDriverDeviceAllocations);
	int imageMemoryLinkedSize = imageMemoryUpdateManager.GetSize(info->numberOfImageMemoryAllocations);

	driverHostMemoryUpdater.AllocateList(instanceStorageAllocator->Allocate(driverHostLinkedSize, alignof(uintptr_t)), driverHostLinkedSize);
	transferCommandPool.AllocateList(instanceStorageAllocator->Allocate(commandLinkedSize, alignof(uintptr_t)), commandLinkedSize);
	driverDeviceMemoryUpdater.AllocateList(instanceStorageAllocator->Allocate(driverDeviceLinkedSize, alignof(uintptr_t)), driverDeviceLinkedSize);
	imageMemoryUpdateManager.AllocateList(instanceStorageAllocator->Allocate(imageMemoryLinkedSize, alignof(uintptr_t)), imageMemoryLinkedSize);
	descriptorUpdatePool.AllocateList(instanceStorageAllocator->Allocate(resourceUpdateLinkedSize, alignof(uintptr_t)), resourceUpdateLinkedSize);

	attachmentGraphsInstances.Create(instanceStorageAllocator, info->maxAttachmentGraphInstances);
	attachmentGraphs.Create(instanceStorageAllocator, info->maxAttachmentGraphTemplates);
	
	internalRendererLogger = (Logger*)instanceStorageAllocator->Allocate(sizeof(Logger), alignof(Logger));
	internalRendererLogger->InitLogger((char*)instanceStorageAllocator->Allocate(info->internalLoggerRingSize, 64), info->internalLoggerRingSize);

	bufferHandles.Create(instanceStorageAllocator, info->maxBufferPoolsCount);
	bufferTypes = (BufferType*)instanceStorageAllocator->Allocate(sizeof(BufferType) * info->maxBufferPoolsCount, alignof(BufferType));
	bufferToResourceStatus = (int*)instanceStorageAllocator->Allocate(sizeof(int) * info->maxBufferPoolsCount, alignof(int));

	imagePools.Create(storageAllocator, info->maxImagePoolsCount);

	pipelineInstancesIdentifier.Create(instanceStorageAllocator, info->maxPipelineInstances);
	pipelinesInstancesInfo = (PipelineInstanceData*)instanceStorageAllocator->Allocate(sizeof(PipelineInstanceData) * info->maxPipelineInstances, alignof(PipelineInstanceData));

	pipelineHandles.Create(instanceStorageAllocator, info->maxPipelineHandles);

	gpuCommands = (GPUCommand*)instanceStorageAllocator->Allocate(sizeof(GPUCommand) * info->maxGPUCommands, alignof(GPUCommand));
	maxGPUCommandCount = info->maxGPUCommands;

	renderTargetQueues.Create(instanceStorageAllocator, info->maxRenderQueues);

	computeQueues.Create(instanceStorageAllocator, info->maxComputeQueues);

	samplerResourceHandles.Create(instanceStorageAllocator, info->maxSamplerHandles);

	textureResourceHandles.Create(instanceStorageAllocator, info->maxTextureHandles);

	textureToResourceStatus = (int*)instanceStorageAllocator->Allocate(sizeof(int) * info->maxTextureHandles, alignof(int));

	resourceStatuses.Create(instanceStorageAllocator, info->maxResourceStatuses);

	pipelineInfos.Create(instanceStorageAllocator, info->maxPipelineTemplates);

	mainRenderTargets.Create(instanceStorageAllocator, info->maxRenderTargets);

	renderPasses.Create(instanceStorageAllocator, info->maxRenderTargets);

	shaderResourceTemplates.Create(instanceStorageAllocator, info->maxShaderResourceTemplates);

	allocations.Create(instanceStorageAllocator, info->maxAllocations);

	descriptorManager.Create(instanceStorageAllocator, info->maxShaderResourceSetSlabAllocator, info->maxShaderResourceSets);

	shaderGraphs.Create(instanceStorageAllocator, info->maxShaderGraphs, info->maxShaderHandles);

	queryResults = (uint32_t*)instanceStorageAllocator->Allocate(sizeof(uint32_t) * info->maxQueries);

	maxQueryResults = info->maxQueries;

	return;
}

RenderInstance::~RenderInstance()
{
	if (vkInstance) vkInstance->~VKInstance();
};

void RenderInstance::DestoryTexture(EntryHandle handle)
{
	auto dev = vkInstance->GetLogicalDevice(deviceIndex);
	dev->DestroyTexture(handle);
}

void RenderInstance::DestroySwapChainAttachments(EntryHandle swapChainIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);

	for (uint32_t a = 0; a < attachmentGraphsInstances.count; a++) 
	{
		AttachmentGraphInstance* graph = attachmentGraphsInstances.Get(a);
		AttachmentResourceInstance* rescs = graph->resources;
		AttachmentResource* descs = graph->graphLayout->resources;
		for (uint32_t c = 0; c < graph->graphLayout->resourceCount; c++)
		{
			AttachmentResourceInstance* inst = &rescs[c];
			AttachmentResource* desc = &descs[c];

			if (desc->viewType == AttachmentViewType::SWAPCHAIN)
				continue;

			int sampLo = inst->sampLo;
			int sampHi = inst->sampHi;

			int sampIndex = 0;

			while (sampLo <= sampHi)
			{
				for (uint32_t d = 0; d < inst->imageCount; d++)
				{
					dev->DestroyImage(inst->attachmentImage[sampIndex][d]);
					dev->DestroyImageView(inst->attachmentImageView[sampIndex][d]);
				}

				sampLo <<= 1;

				sampIndex++;
			}
		}
	}
}

int RenderInstance::RecreateSwapChain(EntryHandle swapChainIndex) {

	int ret = 0;

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	VKSwapChain* swc = dev->GetSwapChain(swapChainIndex);

	int width = 0, height = 0;
	
	windowMan->GetWindowSize(&width, &height);

	if (width && height) 
	{
		swc->Wait();

		DestroySwapChainAttachments(swapChainIndex);

		CreateSwapChainData(swapChainIndex, width, height, true);

		ret = 1;
	}

	return ret;
}

int RenderInstance::CreateFrameGraphInstance(AttachmentGraph* graph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int attachmentInstanceIndex = attachmentGraphsInstances.Allocate();

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(attachmentInstanceIndex);

	graphInstance->graphLayout = graph;

	AttachmentResource* resources = graph->resources;

	graphInstance->passes = (AttachmentRenderPassInstance*)storageAllocator->Allocate(sizeof(AttachmentRenderPassInstance) * graph->passesCount);

	graphInstance->resources = (AttachmentResourceInstance*)storageAllocator->Allocate(sizeof(AttachmentResourceInstance) * graph->resourceCount);

	int totalRenderTargetsCreated = 0;

	for (int b = 0; b < graph->passesCount; b++)
	{
		AttachmentRenderPass* currentPassDesc = &graph->holders[b];

		int attachmentCount = currentPassDesc->attachmentCount;

		AttachmentRenderPassInstance* rpInst = &graphInstance->passes[b];

		AttachmentInstance* currentPassInstance = rpInst->attachInst = (AttachmentInstance*)storageAllocator->Allocate(sizeof(AttachmentInstance) * attachmentCount);

		rpInst->attachInstCount = attachmentCount;

		rpInst->currentSampleCount = 0;

		rpInst->graphicsOTQIndex = -1;

		int sampLo = 1, sampHi = 1;

		for (int c = 0; c < attachmentCount; c++)
		{
			AttachmentDescription* desc = &currentPassDesc->descs[c];

			AttachmentResource* resDesc = &graph->resources[desc->resourceIndex];

			AttachmentResourceInstance* currResource = &graphInstance->resources[desc->resourceIndex];

			int sampleCountLo = (resDesc->msaa ? 2 : 1);

			int sampleCountHi = (resDesc->msaa ? (1 << maxMSAALevels) : 1);

			sampHi = std::max(sampleCountHi, sampHi);

			sampLo = std::max(sampleCountLo, sampLo);

			currResource->attachmentImage = currResource->attachmentImageView = nullptr;

			switch (desc->attachType)
			{
			case AttachmentDescriptionType::COLORATTACH:
				currResource->usage = AttachmentResourceInstanceUsage::COLOR_ATTACHMENT_USAGE;
				break;
			case AttachmentDescriptionType::RESOLVEATTACH:
				currResource->usage = AttachmentResourceInstanceUsage::RESOLVE_ATTACHMENT_USAGE;
				break;
			case AttachmentDescriptionType::DEPTHATTACH:
				currResource->usage = AttachmentResourceInstanceUsage::DEPTH_ATTACHMENT_USAGE;
				break;
			case AttachmentDescriptionType::DEPTHSTENCILATTACH:
				currResource->usage = AttachmentResourceInstanceUsage::DEPTH_STENCIL_ATTACHMENT_USAGE;			
				break;
			case AttachmentDescriptionType::STENCILATTACH:
				currResource->usage = AttachmentResourceInstanceUsage::STENCIL_ATTACHMENT_USAGE;
				break;
			}

			currResource->sampLo = sampleCountLo;
			currResource->sampHi = sampleCountHi;
		}

	
		int renderPassSampleCount = std::max(std::bit_width((unsigned)sampHi)-1, 1);

		rpInst->maxSampleCount = renderPassSampleCount;

		rpInst->baseRenderTargetData = totalRenderTargetsCreated;

		totalRenderTargetsCreated += renderPassSampleCount;
	}

	graphInstance->consecutiveRenderTargetsBase = mainRenderTargets.Allocate(totalRenderTargetsCreated);

	return attachmentInstanceIndex;
}

int RenderInstance::CreateRenderPass(AttachmentGraphInstance* graphInstance)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	graphInstance->consecutiveRenderPassBase = -1;

	AttachmentGraph* graph = graphInstance->graphLayout;

	AttachmentResource* resources = graph->resources;

	int totalRenderPassesCreated = 0;

	for (int b = 0; b < graph->passesCount; b++)
	{
		AttachmentRenderPass* currentPassDesc = &graph->holders[b];

		int attachmentCount = currentPassDesc->attachmentCount;

		VKRenderPassBuilder rpb = dev->CreateRenderPassBuilder(attachmentCount, 1, 1);

		uint32_t currDepthStencil = 0;

		uint32_t currResolve = 0;

		uint32_t currPreserve = 0;

		uint32_t currInput = 0;

		uint32_t currColor = 0;

		AttachmentRenderPassInstance* rpInst = &graphInstance->passes[b];

		AttachmentInstance* currentPassInstance = rpInst->attachInst;

		int sampLo = 1;

		for (int c = 0; c < attachmentCount; c++)
		{
			VkImageLayout referenceLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			AttachmentDescription* desc = &currentPassDesc->descs[c];

			AttachmentResource* resDesc = &graph->resources[desc->resourceIndex];

			VkFormat attachFormat = API::ConvertImageFormatToVulkanFormat(resources[desc->resourceIndex].format);

			VkAttachmentLoadOp stencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE, dsvrtvLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			VkAttachmentStoreOp stencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE, dsvrtvStore = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			VkSampleCountFlags vkSampleCountLo = (resDesc->msaa ? VK_SAMPLE_COUNT_2_BIT : VK_SAMPLE_COUNT_1_BIT);

			sampLo = std::max((int)vkSampleCountLo, sampLo);

			uint32_t vkRenderPassMappedIdx = 0;

			AttachmentResourceInstance* currResource = &graphInstance->resources[desc->resourceIndex];

			currResource->attachmentImage = currResource->attachmentImageView = nullptr;

			switch (desc->attachType)
			{
			case AttachmentDescriptionType::COLORATTACH:
				referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				currResource->usage = AttachmentResourceInstanceUsage::COLOR_ATTACHMENT_USAGE;
				dsvrtvLoad = API::ConvertAttachLoadOpToVulkanLoadOp(desc->loadOp);
				dsvrtvStore = API::ConvertAttachStoreOpToVulkanStoreOp(desc->storeOp);
				vkRenderPassMappedIdx = currColor++;
				break;
			case AttachmentDescriptionType::RESOLVEATTACH:
				referenceLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				currResource->usage = AttachmentResourceInstanceUsage::RESOLVE_ATTACHMENT_USAGE;
				dsvrtvLoad = API::ConvertAttachLoadOpToVulkanLoadOp(desc->loadOp);
				dsvrtvStore = API::ConvertAttachStoreOpToVulkanStoreOp(desc->storeOp);
				vkRenderPassMappedIdx = (currentPassDesc->colorCount + currentPassDesc->depthStencilCount) + currResolve++;
				break;
			case AttachmentDescriptionType::DEPTHATTACH:
				referenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				currResource->usage = AttachmentResourceInstanceUsage::DEPTH_ATTACHMENT_USAGE;
				dsvrtvLoad = API::ConvertAttachLoadOpToVulkanLoadOp(desc->loadOp);
				dsvrtvStore = API::ConvertAttachStoreOpToVulkanStoreOp(desc->storeOp);
				vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
				break;
			case AttachmentDescriptionType::DEPTHSTENCILATTACH:
				referenceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				currResource->usage = AttachmentResourceInstanceUsage::DEPTH_STENCIL_ATTACHMENT_USAGE;
				stencilLoad = dsvrtvLoad = API::ConvertAttachLoadOpToVulkanLoadOp(desc->loadOp);
				stencilStore = dsvrtvStore = API::ConvertAttachStoreOpToVulkanStoreOp(desc->storeOp);
				vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
				break;
			case AttachmentDescriptionType::STENCILATTACH:
				referenceLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
				currResource->usage = AttachmentResourceInstanceUsage::STENCIL_ATTACHMENT_USAGE;
				stencilLoad = API::ConvertAttachLoadOpToVulkanLoadOp(desc->loadOp);
				stencilStore = API::ConvertAttachStoreOpToVulkanStoreOp(desc->storeOp);
				vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
				break;
			}

			VkImageLayout srcLayout = API::ConvertImageLayoutToVulkanImageLayout(desc->srcLayout),
				dstLayout = API::ConvertImageLayoutToVulkanImageLayout(desc->dstLayout);

			rpb.CreateAttachment(
				referenceLayout,
				attachFormat, (VkSampleCountFlagBits)vkSampleCountLo,
				dsvrtvLoad, dsvrtvStore,
				stencilLoad, stencilStore,
				srcLayout, dstLayout, vkRenderPassMappedIdx, vkRenderPassMappedIdx
			);

			currentPassInstance[vkRenderPassMappedIdx].descLayout = desc;
			currentPassInstance[vkRenderPassMappedIdx].attachmentResource = desc->resourceIndex;
		}

		rpb.CreateSubPassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, currentPassDesc->colorCount, currentPassDesc->resolveCount, currentPassDesc->depthStencilCount);

		rpb.CreateSubPassDependency(VK_SUBPASS_EXTERNAL, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		rpb.CreateInfo();

		int renderPassSampleCount = 0;

		int sampleCount = rpInst->maxSampleCount;

		while (sampleCount--)
		{
			int index = renderPasses.Allocate();

			renderPasses.pool[index] = dev->CreateRenderPasses(rpb);

			if (graphInstance->consecutiveRenderPassBase < 0)
				graphInstance->consecutiveRenderPassBase = index;

			sampLo <<= 1;

			currDepthStencil = 0;

			currResolve = 0;

			currPreserve = 0;

			currInput = 0;

			currColor = 0;

			for (int c = 0; c < attachmentCount; c++)
			{
				AttachmentDescription* desc = &currentPassDesc->descs[c];

				AttachmentResource* resDesc = &graph->resources[desc->resourceIndex];

				VkSampleCountFlags sampleCount = VK_SAMPLE_COUNT_1_BIT;

				if (resDesc->msaa)
				{
					sampleCount = sampLo;
				}

				uint32_t vkRenderPassMappedIdx = 0;

				switch (desc->attachType)
				{
				case AttachmentDescriptionType::COLORATTACH:
					vkRenderPassMappedIdx = currColor++;
					break;
				case AttachmentDescriptionType::RESOLVEATTACH:	
					vkRenderPassMappedIdx = (currentPassDesc->colorCount + currentPassDesc->depthStencilCount) + currResolve++;
					break;
				case AttachmentDescriptionType::DEPTHATTACH:
					vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
					break;
				case AttachmentDescriptionType::DEPTHSTENCILATTACH:
					vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
					break;
				case AttachmentDescriptionType::STENCILATTACH:
					vkRenderPassMappedIdx = (currentPassDesc->colorCount) + currDepthStencil++;
					break;
				}
				
				rpb.SetSampleCount(vkRenderPassMappedIdx, (VkSampleCountFlagBits)sampleCount);

			}

			renderPassSampleCount++;
		}

		rpInst->baseRenderPassData = totalRenderPassesCreated;

		totalRenderPassesCreated += renderPassSampleCount;
	}

	return totalRenderPassesCreated;
}

uint32_t RenderInstance::BeginFrame(EntryHandle swapChainIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int32_t res = dev->CommandBufferWaitOn(UINT64_MAX, currentCBIndex[currentFrame]);

	uint32_t imageIndex = dev->BeginFrameForSwapchain(swapChainIndex, rendererWaitSemaphores[currentFrame], currentFrame);

	if (imageIndex == ~0ui32)
	{
		int ret = RecreateSwapChain(swapChainIndex);
	
		return imageIndex;
	}

	dev->CommandBufferResetFence(currentCBIndex[currentFrame]);

	return imageIndex;
}

int RenderInstance::SubmitFrame(EntryHandle swapChainIndex, uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	std::array<VkPipelineStageFlags, 2> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	int res = -1;
	
	if (rendererTimelineSyncObject.driverTimelineObject == EntryHandle())
	{
		res = dev->SubmitCommandBuffer(&rendererWaitSemaphores[currentFrame], &waitStages[0], &rendererFinishedSemaphores[imageIndex], 1, 1, currentCBIndex[currentFrame]);
	}
	else
	{
		std::array<uint64_t, 2> waitCount = { 0, rendererTimelineSyncObject.currentValue };

		std::array<uint64_t, 2> signalCount = {0, rendererTimelineSyncObject.currentValue + 1 };

		res = dev->SubmitCommandBuffer(
			&rendererWaitSemaphores[currentFrame], waitStages.data(), 1,
			&rendererTimelineSyncObject.driverTimelineObject, 1, waitCount.data(),
			&rendererFinishedSemaphores[imageIndex], 1,
			&rendererTimelineSyncObject.driverTimelineObject, 
			1,
			signalCount.data(),
			currentCBIndex[currentFrame]
		);
		
		rendererTimelineSyncObject.currentValue++;
	}

	if (res)
		return -1;

	if (presentQueue != graphicsComputeTransfer)
	{
		res = dev->PresentSwapChainSeparatePresentQueue(swapChainIndex, &rendererFinishedSemaphores[imageIndex], 1, imageIndex, currentFrame, presentQueue);
	}
	else
	{
		res = dev->PresentSwapChainCommandBufferInline(swapChainIndex, &rendererFinishedSemaphores[imageIndex], 1, imageIndex, currentFrame, currentCBIndex[currentFrame]);
	}

	if (res) 
	{
		dev->CommandBufferWaitOn(UINT64_MAX, currentCBIndex[currentFrame]);
		int ret = RecreateSwapChain(swapChainIndex);
		return -1;
	}

	return 0;
}

void RenderInstance::WaitOnRender()
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);
	dev->WaitOnDevice();
}

int RenderInstance::CreateSwapChainAttachment(EntryHandle swapChainIndex, int graphIndex, int renderPassIndex, AttachmentClear* clears, DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	EntryHandle* backBuffers = swapChain->imageViews;

	return CreateAttachmentResources(graphIndex, renderPassIndex, swapChain->imageCount, backBuffers, swapChain->GetSwapChainWidth(), swapChain->GetSwapChainHeight(), RenderPassType::SWAPCHAIN_IMAGE_COUNT, clears, rtvAllocator, dsvAllocator, rtvPoolIndex, dsvPoolIndex);
}

int RenderInstance::CreatePerFrameAttachment(int graphIndex, int renderPassIndex, int imageCount, uint32_t width, uint32_t height, AttachmentClear* clears, DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	return CreateAttachmentResources(graphIndex, renderPassIndex, imageCount, nullptr, width, height, RenderPassType::PER_FRAME_IMAGE_COUNT, clears, rtvAllocator, dsvAllocator, rtvPoolIndex, dsvPoolIndex);
}

int RenderInstance::CreateAttachmentResources(
	int graphIndex, int renderPassIndex, int imageCount, 
	EntryHandle* backBufferViews, uint32_t width, uint32_t height, 
	RenderPassType rpType, AttachmentClear* clears,
	DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(graphIndex);

	int baseRenderTarget = graphInstance->consecutiveRenderTargetsBase;

	int baseRenderPass = graphInstance->consecutiveRenderPassBase;

	AttachmentRenderPassInstance* currentRenderPass = &graphInstance->passes[renderPassIndex];

	int attachmentCount = currentRenderPass->attachInstCount;

	EntryHandle* attachmentViews = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * attachmentCount);

	int basePassRenderTarget = currentRenderPass->baseRenderTargetData;

	int basePassRenderPass = currentRenderPass->baseRenderPassData;

	AttachmentInstance* attachInsts = currentRenderPass->attachInst;

	currentRenderPass->rpType = rpType;

	for (int b = 0; b < attachmentCount; b++)
	{
		AttachmentInstance* attachDesc = &attachInsts[b];

		int resourceIndex = attachDesc->attachmentResource;

		AttachmentResourceInstance* resourceInst = &graphInstance->resources[resourceIndex];
		
		AttachmentResource* resourceTempl = &graphInstance->graphLayout->resources[resourceIndex];

		int sampHi = resourceInst->sampHi;
		int sampLo = resourceInst->sampLo;

		int sampleCount = std::max(std::bit_width((unsigned)sampHi)-1, 1);

		int imageWidth = width;
		int imageHeight = height;

		if (clears)
		{
			attachDesc->clear = clears[resourceIndex];
		}

		if (resourceTempl->viewType == AttachmentViewType::SWAPCHAIN)
		{
			resourceInst->attachmentImageView = (EntryHandle**)storageAllocator->Allocate(sizeof(EntryHandle*) * 1);
			resourceInst->attachmentImageView[0] = backBufferViews;
		}
		else
		{
			if (!resourceInst->attachmentImage)
			{
				resourceInst->attachmentImage = (EntryHandle**)storageAllocator->Allocate(sizeof(EntryHandle*) * sampleCount);

				for (int c = 0; c<sampleCount; c++)
				{ 
					resourceInst->attachmentImage[c] = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * imageCount);
				}
			}

			if (!resourceInst->attachmentImageView)
			{

				resourceInst->attachmentImageView = (EntryHandle**)storageAllocator->Allocate(sizeof(EntryHandle*) * sampleCount);

				for (int c = 0; c < sampleCount; c++)
				{
					resourceInst->attachmentImageView[c] = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * imageCount);
				}
			}

			VkFormat attachmentFormat = API::ConvertImageFormatToVulkanFormat(resourceTempl->format);

			resourceInst->imageCount = imageCount;

			size_t actualImageSize = 0, actualImageAlignment = 0, actualMemAddr = 0;

			switch (resourceInst->usage)
			{

			case AttachmentResourceInstanceUsage::COLOR_ATTACHMENT_USAGE:

				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{

						dev->GetImageMemorySizeAndAlignment(imageWidth, imageHeight,
							1, attachmentFormat, 1,
							VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
							sampLo,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_TILING_OPTIMAL, 0,
							VK_IMAGE_TYPE_2D, &actualImageSize, &actualImageAlignment);

						actualMemAddr = rtvAllocator->Allocate(actualImageSize, actualImageAlignment);

						resourceInst->attachmentImage[v][g] = dev->CreateImage(
							imageWidth, imageHeight,
							1, attachmentFormat, 1,
							VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
							sampLo, actualMemAddr,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_TILING_OPTIMAL, 0,
							VK_IMAGE_TYPE_2D, imagePools[rtvPoolIndex]
						);

						resourceInst->attachmentImageView[v][g]
							=
							dev->CreateImageView(resourceInst->attachmentImage[v][g], 1, 1, attachmentFormat, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
					}

					sampLo <<= 1;
				}
				break;
			case AttachmentResourceInstanceUsage::DEPTH_STENCIL_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						dev->GetImageMemorySizeAndAlignment(
							imageWidth, imageHeight,
							1, attachmentFormat, 1,
							VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
							sampLo,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 
							VK_IMAGE_TILING_OPTIMAL, 0, 
							VK_IMAGE_TYPE_2D, 
							&actualImageSize, &actualImageAlignment
						);

						actualMemAddr = dsvAllocator->Allocate(actualImageSize, actualImageAlignment);

						resourceInst->attachmentImage[v][g] =
							dev->CreateImage(imageWidth, imageHeight,
								1, attachmentFormat, 1,
								VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
								sampLo, actualMemAddr,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, imagePools[dsvPoolIndex]);

						resourceInst->attachmentImageView[v][g] = dev->CreateImageView(resourceInst->attachmentImage[v][g], 1, 1, attachmentFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_VIEW_TYPE_2D);
					}

					sampLo <<= 1;
				}
				break;
			case AttachmentResourceInstanceUsage::DEPTH_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						dev->GetImageMemorySizeAndAlignment(
							imageWidth, imageHeight,
							1, attachmentFormat, 1,
							VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
							sampLo,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D,
							&actualImageSize, &actualImageAlignment
						);

						actualMemAddr = dsvAllocator->Allocate(actualImageSize, actualImageAlignment);

						resourceInst->attachmentImage[v][g] =
							dev->CreateImage(imageWidth, imageHeight,
								1, attachmentFormat, 1,
								VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
								sampLo, actualMemAddr,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, imagePools[dsvPoolIndex]);

						resourceInst->attachmentImageView[v][g] = dev->CreateImageView(resourceInst->attachmentImage[v][g], 1, 1, attachmentFormat, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D);
					}
					sampLo <<= 1;

				}
				break;

			case AttachmentResourceInstanceUsage::STENCIL_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						dev->GetImageMemorySizeAndAlignment(
							imageWidth, imageHeight,
							1, attachmentFormat, 1,
							VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
							sampLo,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D,
							&actualImageSize, &actualImageAlignment
						);

						actualMemAddr = dsvAllocator->Allocate(actualImageSize, actualImageAlignment);

						resourceInst->attachmentImage[v][g] =
							dev->CreateImage(imageWidth, imageHeight,
								1, attachmentFormat, 1,
								VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
								sampLo, actualMemAddr,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, imagePools[dsvPoolIndex]);

						resourceInst->attachmentImageView[v][g] = dev->CreateImageView(resourceInst->attachmentImage[v][g], 1, 1, attachmentFormat, VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_VIEW_TYPE_2D);
					}
					sampLo <<= 1;

				}
				break;

			case AttachmentResourceInstanceUsage::RESOLVE_ATTACHMENT_USAGE:
				for (int g = 0; g < imageCount; g++)
				{

					dev->GetImageMemorySizeAndAlignment(
						imageWidth, imageHeight,
						1, attachmentFormat, 1,
						VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
						sampLo,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_TILING_OPTIMAL, 0,
						VK_IMAGE_TYPE_2D,
						&actualImageSize, &actualImageAlignment
					);

					actualMemAddr = rtvAllocator->Allocate(actualImageSize, actualImageAlignment);

					resourceInst->attachmentImage[0][g] = dev->CreateImage(
						imageWidth, imageHeight,
						1, attachmentFormat, 1,
						VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
						sampLo, actualMemAddr,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_TILING_OPTIMAL, 0,
						VK_IMAGE_TYPE_2D, imagePools[rtvPoolIndex]
					);

					resourceInst->attachmentImageView[0][g]
						=
						dev->CreateImageView(resourceInst->attachmentImage[0][g], 1, 1, attachmentFormat, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D);
				}
				break;

			case AttachmentResourceInstanceUsage::PRESERVE_ATTACHMENT_USAGE:
				//flags = 0; // no direct Vulkan usage flag
				break;

			case AttachmentResourceInstanceUsage::INPUT_ATTACHMENT_USAGE:
				//flags = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
				break;
			}
		}
	}

	int rtWidth =  width;
	int rtHeight = height;

	for (int sampleCount = 0; sampleCount < currentRenderPass->maxSampleCount; sampleCount++)
	{
		int absoluteRTIndex = baseRenderTarget + basePassRenderTarget + sampleCount;

		int absoluteRPIndex = baseRenderPass + basePassRenderPass + sampleCount;

		if (mainRenderTargets[absoluteRTIndex] != EntryHandle())
		{
			dev->DestroyRenderTarget(mainRenderTargets[absoluteRTIndex]);
		}

		mainRenderTargets.pool[absoluteRTIndex] = dev->CreateRenderTarget(renderPasses[absoluteRTIndex], imageCount, rtWidth, rtHeight, 0, 0);

		RenderTarget* renderTarget = dev->GetRenderTarget(mainRenderTargets[absoluteRTIndex]);

		for (int d = 0; d < imageCount; d++)
		{
			for (int e = 0; e < attachmentCount; e++)
			{
				AttachmentInstance* attachInst = &attachInsts[e];

				AttachmentResourceInstance* resourceInst = &graphInstance->resources[attachInst->attachmentResource];

				int sampleIndex = sampleCount;

				if (resourceInst->sampHi < (1 << sampleCount))
				{
					sampleIndex = std::bit_width((unsigned)resourceInst->sampHi) - 1;
				}

				attachmentViews[e] = resourceInst->attachmentImageView[sampleIndex][d];
			}

			renderTarget->framebufferIndices[d] =
				dev->CreateFrameBuffer(
					attachmentViews,
					attachmentCount,
					renderPasses[absoluteRPIndex],
					{ width, height }
				);
		}
	}
	
	return 0;
}

void RenderInstance::CreateSwapChainData(EntryHandle swapChainIndex, uint32_t width, uint32_t height, bool recreate)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	if (recreate)
	{
		swapChain->ResetSwapChain();
		swapChain->RecreateSwapChain(width, height);
	}
	else
	{
		swapChain->CreateSwapChain(width, height, graphicsComputeTransfer, presentQueue);
	}

	swapChain->CreateSwapchainViews();
}

void RenderInstance::CreateShaderResourceMap(ShaderGraph* graph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int ResourceSetCount = graph->resourceSetCount;

	DescriptorSetLayoutBuilder** descriptorBuilders = (DescriptorSetLayoutBuilder**)cacheAllocator->Allocate(sizeof(DescriptorSetLayoutBuilder*) * ResourceSetCount);

	
	
	for (int j = 0; j < ResourceSetCount; j++)
	{
		ShaderResourceSetTemplate* set = &graph->shaderResourceSetTemplates[j];
		descriptorBuilders[j] = dev->CreateDescriptorSetLayoutBuilder(set->bindingCount);
	}

	for (int j = 0; j < graph->resourceCount; j++)
	{
		ShaderResource* resource = &graph->shaderResources[j];

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

		static bool useUpdateAfterBind = false;

		arrayCount &= DESCRIPTOR_COUNT_MASK;

		switch (resource->type)
		{
		case ShaderResourceType::UNIFORM_BUFFER:
			descriptorBuilder->AddBufferLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
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
			descriptorBuilder->AddStorageBufferLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		case ShaderResourceType::BUFFER_VIEW:
			if (resource->action == ShaderResourceAction::SHADERREAD)
				descriptorBuilder->AddUniformBufferViewLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			else if (resource->action == ShaderResourceAction::SHADERWRITE)
				descriptorBuilder->AddStorageBufferViewLayout(resource->binding, stageFlags, arrayCount, bindingFlags);
			break;
		}
	}

	for (int j = 0; j < ResourceSetCount; j++)
	{
		ShaderResourceSetTemplate* set = &graph->shaderResourceSetTemplates[j];
		
		int descriptorLayoutIndex = shaderResourceTemplates.Allocate();

		shaderResourceTemplates.pool[descriptorLayoutIndex] = dev->CreateDescriptorSetLayout(descriptorBuilders[j]);
		
		set->vulkanDescLayout = descriptorLayoutIndex;
	}
}

void RenderInstance::CreateShaderGraphs(StringView* shaderGraphLayouts, int shaderGraphLayoutsCount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int totalDetailSize = 0;

	int shaderDetailsHead = shaderGraphs.shaders.count;

	int shaderGraphHead = shaderGraphs.shaderGraphPtrs.count;

	ShaderDetails* details = &shaderGraphs.shaderDetails[shaderDetailsHead];
	
	for (int i = 0; i < shaderGraphLayoutsCount; i++)
	{
		int detailsSize = 0;

		int detailsIndex = shaderGraphs.shaders.Allocate();

		int shaderGraphIndex = shaderGraphs.shaderGraphPtrs.Allocate();

		ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);

		CreateShaderGraph
		(
			shaderGraphLayouts[i],
			cacheAllocator,
			graph,
			details, 
			&detailsSize, 
			internalRendererLogger
		);

		CreateShaderResourceMap(shaderGraphs.shaderGraphPtrs.Get(i));

		totalDetailSize += detailsSize;

		details = &shaderGraphs.shaderDetails[shaderDetailsHead + totalDetailSize];
	}

	int shaderGraph = shaderGraphHead;

	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraph);

	int shaderMapCount = graph->shaderMapCount, mapIter = 0;

	for (int i = 0; i < totalDetailSize; i++)
	{
		ShaderDetails* details = &shaderGraphs.shaderDetails[shaderDetailsHead + i];

		char* str = details->glslShaderName;

		int shaderNameLength = details->glslShaderNameSize;

		if (mapIter >= shaderMapCount)
		{
			++shaderGraph;
			graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraph);
			shaderMapCount = graph->shaderMapCount;
			mapIter = 0;
		}

		ShaderMap* map = &graph->shaderMaps[mapIter++];

		map->shaderReference = i;

		char* shaderData;

		OSFileHandle handle;

		int shaderLength = 0;

		char* stringBuffer = (char*)cacheAllocator->Allocate(shaderNameLength + 4);

		memcpy(stringBuffer, str, shaderNameLength);

		memcpy(stringBuffer + (shaderNameLength-1), ".spv", 5);

		StringView nameView{ stringBuffer, shaderNameLength + 3 };

		VkShaderStageFlags shaderFlags = API::ConvertShaderStageToVulkanShaderStage(map->type);

		if (FileManager::FileExists(&nameView)) {

			OSOpenFile(nameView.stringData, nameView.charCount, READ, &handle);

			shaderLength = handle.fileLength;

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength);

			OSReadFile(&handle, shaderLength, shaderData);
		}
		else
		{
			nameView.charCount -= 4;

			OSOpenFile(nameView.stringData, nameView.charCount, READ, &handle);

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength + 1);

			OSReadFile(&handle, shaderLength, shaderData);

			if (shaderData[shaderLength - 1] != '\0')
			{
				shaderData[shaderLength] = '\0';
				shaderLength++;
			}
		}

		shaderGraphs.shaders.pool[shaderDetailsHead + i] = dev->CreateShader(shaderData, shaderLength, shaderFlags);

		OSCloseFile(&handle);
	}
}

int RenderInstance::CreateGraphicRenderStateObject(int shaderGraphIndex, int pipelineDescriptionIndex, int* frameGraphAttachments, int* perFrameRenderPassSelection, int frameGraphCount)
{
	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);

	ShaderMap* map = &graph->shaderMaps[0];

	if (map->type != COMPUTESHADERSTAGE)
	{
		uint32_t pipelineVariationsCounter = 0;

		uint32_t totalPiplineVariations = 0;

		PipelineInstanceData* instData = &pipelinesInstancesInfo[shaderGraphIndex];

		instData->frameGraphCount = frameGraphCount;

		for (int i = 0; i < frameGraphCount; i++)
		{
			totalPiplineVariations += attachmentGraphsInstances[frameGraphAttachments[i]].passes[perFrameRenderPassSelection[i]].maxSampleCount;
			instData->frameGraphIndices[i] = frameGraphAttachments[i];
			instData->frameGraphRenderPasses[i] = perFrameRenderPassSelection[i];
		}

		EntryHandle* pipelineHandles = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * totalPiplineVariations);

		instData->pipelineCount = totalPiplineVariations;

		for (int i = 0; i < frameGraphCount; i++)
		{
			instData->frameGraphPipelineIndices[i] = pipelineVariationsCounter;

			pipelineVariationsCounter += CreatePipelineFromGraphAndSpec(
				pipelineInfos.Get(pipelineDescriptionIndex), shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex), 
				pipelineHandles, pipelineVariationsCounter, 
				attachmentGraphsInstances.Get(frameGraphAttachments[i]), perFrameRenderPassSelection[i]
			);
		}
		
		pipelineInstancesIdentifier.pool[shaderGraphIndex] = pipelineHandles;
	}

	return 0;
}

int RenderInstance::CreateComputePipelineStateObject(int shaderGraphIndex)
{
	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);
	
	ShaderMap* map = &graph->shaderMaps[0];
	
	if (map->type == COMPUTESHADERSTAGE)
	{
		EntryHandle* pipelineID = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle));

		*pipelineID = CreateVulkanComputePipelineTemplate(graph);

		pipelineInstancesIdentifier.pool[shaderGraphIndex] = pipelineID;
	}

	return 0;
}

void RenderInstance::CreatePipelines(StringView* pipelineDescriptions, int pipelineDescriptionsCount)
{
	for (int i = 0; i < pipelineDescriptionsCount; i++)
	{
		int stateInfoIndex = pipelineInfos.Allocate();

		GenericPipelineStateInfo* stateInfo = pipelineInfos.Get(stateInfoIndex);

		CreatePipelineDescription(pipelineDescriptions[i], stateInfo, cacheAllocator, internalRendererLogger);
	}
}

int RenderInstance::CreatePipelineFromGraphAndSpec(GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer, AttachmentGraphInstance* graphInstance, uint32_t graphRenderPassIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	EntryHandle* layoutHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * graph->resourceSetCount);
	EntryHandle* shaderHandle = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * graph->shaderMapCount);

	for (int i = 0; i < graph->shaderMapCount; i++)
	{
		ShaderMap* map = &graph->shaderMaps[i];
		shaderHandle[i] = shaderGraphs.shaders[map->shaderReference];
	}

	int pushConstantRangeCount = 0;

	for (int i = 0; i < graph->resourceSetCount; i++)
	{
		ShaderResourceSetTemplate* resourceSet = &graph->shaderResourceSetTemplates[i];
		layoutHandles[i] = shaderResourceTemplates[resourceSet->vulkanDescLayout];
		pushConstantRangeCount += resourceSet->constantStageCount;
	}

	uint32_t* pushConstantsSizes = (uint32_t*)cacheAllocator->CAllocate(sizeof(uint32_t) * pushConstantRangeCount);
	VkShaderStageFlags* shaderStages = (VkShaderStageFlags*)cacheAllocator->CAllocate(sizeof(VkShaderStageFlags) * pushConstantRangeCount);

	for (int i = 0; i < graph->resourceCount; i++)
	{
		ShaderResource* resource = &graph->shaderResources[i];
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

	pipelineBuilder->CreateDepthStencil(API::ConvertRasterizerTestToVulkanCompareOp(stateInfo->depthTest), stateInfo->depthEnable, stateInfo->depthWrite, stateInfo->StencilEnable, &frontState, &backState);

	AttachmentRenderPassInstance* rendPassInst = &graphInstance->passes[graphRenderPassIndex];

	uint32_t sampleCount = (uint32_t)rendPassInst->maxSampleCount;

	uint32_t lowSample = (sampleCount > 1) ? 1 : 0;

	uint32_t vkRenderPassIndex = graphInstance->consecutiveRenderPassBase + graphInstance->passes[graphRenderPassIndex].baseRenderPassData;
	
	uint32_t pipelinesCreated = 0;

	for (; pipelinesCreated < sampleCount; pipelinesCreated++)
	{
		int msaaLevel = (1 << (lowSample + pipelinesCreated));
		if (msaaLevel > stateInfo->sampleCountHigh) break;

		pipelineBuilder->CreateMultiSampling((VkSampleCountFlagBits)msaaLevel);
		pipelineBuilder->renderPass = dev->GetRenderPass(renderPasses[vkRenderPassIndex + pipelinesCreated]);

		outHandles[outHandlePointer + pipelinesCreated] = pipelineBuilder->CreateGraphicsPipeline(layoutHandles, graph->resourceSetCount, shaderHandle, graph->shaderMapCount);
	}

	return pipelinesCreated;
}

EntryHandle RenderInstance::CreateVulkanComputePipelineTemplate(ShaderGraph* graph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	EntryHandle* layoutHandles = (EntryHandle*)cacheAllocator->Allocate(sizeof(EntryHandle) * (graph->resourceSetCount));

	EntryHandle shaderHandle;

	ShaderMap* map = &graph->shaderMaps[0];

	shaderHandle = shaderGraphs.shaders[map->shaderReference];

	int pushRangeSize = 0;

	for (int a = 0; a < graph->resourceSetCount; a++)
	{
		ShaderResourceSetTemplate* setLayout = &graph->shaderResourceSetTemplates[a];

		pushRangeSize += setLayout->constantStageCount;
	}

	int* pushConstantSize = (int*)cacheAllocator->CAllocate(sizeof(int) * pushRangeSize);

	for (int g = 0; g < graph->resourceCount; g++)
	{
		ShaderResource* resource = &graph->shaderResources[g];

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
		ShaderResourceSetTemplate* resourceSet = &graph->shaderResourceSetTemplates[i];

		layoutHandles[i] = shaderResourceTemplates[resourceSet->vulkanDescLayout];
	}

	return pipelineBuilder->CreateComputePipeline(layoutHandles, graph->resourceSetCount, shaderHandle);
}

void RenderInstance::UploadHostTransfers()
{
	int memCount = driverHostMemoryUpdater.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

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

		EntryHandle index = bufferHandles[allocations[handle].memIndex];

		void* data = region.data;

		if (index != previousBuffer)
		{
			if (previousBuffer != EntryHandle())
			{
				dev->WriteToHostBufferBatch(previousBuffer, batchAddresses, batchSizes, batchOffsets, previousMax - previousMin, previousMin, batchCounter);
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

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int link = descriptorUpdatePool.linkHead;
	int* linkprev = &descriptorUpdatePool.linkHead;
	ShaderResourceUpdate region;

	EntryHandle previousBuffer = EntryHandle();

	DescriptorSetBuilder* builder = nullptr;

	ShaderResourceSet* set = nullptr;

	uintptr_t* offsets = nullptr;

	while (link >= 0)
	{
		link = descriptorUpdatePool.PopLink(&region, link, &linkprev);

		EntryHandle index = descriptorManager.descriptorSetHandles[region.descriptorSet];

		void* data = region.data;

		if (index != previousBuffer)
		{
			builder = dev->UpdateDescriptorSet(index);
			previousBuffer = index;

			uintptr_t head = descriptorManager.descriptorSets[region.descriptorSet];
			set = (ShaderResourceSet*)head;
			offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));
		}

		switch (region.type)
		{
		case ShaderResourceType::SAMPLERSTATE:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				builder->AddSamplerDescription(samplerResourceHandles[update->resourceHandles[iter]], update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);
			}
			break;
		}
		case ShaderResourceType::IMAGE2D:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				builder->AddImageResourceDescription(textureResourceHandles[update->resourceHandles[iter]], update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);
			}
			break;
		}
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLER2D:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				builder->AddCombinedTextureArray(textureResourceHandles[update->resourceHandles[iter]], update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);
			}
			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;
			if (!update->allocationIndices) break;

			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				auto alloc = allocations[update->allocationIndices[j]];

				if (alloc.allocType == AllocationType::PERFRAME)
					builder->AddStorageBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize / MAX_FRAMES_IN_FLIGHT, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
				else
					builder->AddStorageBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);

			}
			break;
		}
		case ShaderResourceType::UNIFORM_BUFFER:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;
			if (!update->allocationIndices) break;

			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				auto alloc = allocations[update->allocationIndices[j]];

				if (alloc.allocType == AllocationType::PERFRAME)
					builder->AddUniformBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize / MAX_FRAMES_IN_FLIGHT, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
				else
					builder->AddUniformBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
			}
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;
			
			if (!update->allocationIndices) break;

			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				RenderAllocation* alloc = allocations.Get(update->allocationIndices[j]);

				int viewGrab = (alloc->allocType == AllocationType::PERFRAME) ? currentFrame : 0;

				VkBufferView handle = dev->GetBufferView(alloc->viewIndex, viewGrab);

				ShaderResourceHeader* resource = (ShaderResourceHeader*)offsets[region.bindingIndex];

				if (resource->action == ShaderResourceAction::SHADERREAD)
				{
					builder->AddUniformBufferView(handle, region.bindingIndex, currentFrame, 1, j + firstBuffer);
				}
				else if (resource->action == ShaderResourceAction::SHADERWRITE)
				{
					builder->AddStorageBufferView(handle, region.bindingIndex, currentFrame, 1, j + firstBuffer);
				}
				
			}
			break;
		}
		}
	}
}

void RenderInstance::UploadImageMemoryTransfers(RecordingBufferObject* rbo)
{
	int memCount = imageMemoryUpdateManager.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	TextureMemoryRegion region;
	int link = imageMemoryUpdateManager.linkHead;

	DeviceSlabAllocator* stagingAlloc = &stagingBufferAllocators[currentFrame];

	while (link >= 0)
	{
		link = imageMemoryUpdateManager.PopLink(&region, link);

		EntryHandle handle = textureResourceHandles[region.textureIndex];

		size_t currentImageOffsetInUploadArena = stagingAlloc->Allocate(region.totalSize, 256);

		dev->UploadImageData(handle,
			(char*)region.data,
			region.totalSize,
			stagingBuffers[currentFrame],
			region.width,
			region.height,
			region.mipLevels,
			region.layers,
			API::ConvertImageFormatToVulkanFormat(region.format),
			currentImageOffsetInUploadArena,
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

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	BufferMemoryTransferRegion region;
	int link = driverDeviceMemoryUpdater.linkHead;
	int* linkprev = &driverDeviceMemoryUpdater.linkHead;

	size_t* batchSizes = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * (memCount));
	size_t* batchOffsets = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * (memCount));
	void** batchData = (void**)cacheAllocator->Allocate(sizeof(void*) * (memCount));
	size_t* uploadArenaOffset = (size_t*)cacheAllocator->Allocate(sizeof(size_t) * (memCount));

	size_t batchCounter = 0;
	size_t cumulativeSize = 0;

	EntryHandle previousBuffer = EntryHandle();

	DeviceSlabAllocator* stagingAlloc = &stagingBufferAllocators[currentFrame];

	while (link >= 0)
	{
		link = driverDeviceMemoryUpdater.PopLink(&region, link, &linkprev);
		
		EntryHandle index = bufferHandles[allocations.Get(region.allocationIndex)->memIndex];
	
		if (index != previousBuffer)
		{
			if (previousBuffer != EntryHandle())
			{
				cumulativeSize = (uploadArenaOffset[batchCounter - 1] - uploadArenaOffset[0]) + batchSizes[batchCounter - 1];
				dev->WriteToDeviceBufferBatch(previousBuffer, stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, uploadArenaOffset, batchCounter, rbo);
			}

			previousBuffer = index;
			batchCounter = 0;
			cumulativeSize = 0;
		}

		uploadArenaOffset[batchCounter] = stagingAlloc->Allocate(region.size, 64);
		batchSizes[batchCounter] = region.size;
		batchData[batchCounter] = region.data;
		batchOffsets[batchCounter] = allocations.Get(region.allocationIndex)->offset + region.allocoffset;

		batchCounter++;
	}

	cumulativeSize = (uploadArenaOffset[batchCounter - 1] - uploadArenaOffset[0]) + batchSizes[batchCounter - 1];

	dev->WriteToDeviceBufferBatch(previousBuffer, stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, uploadArenaOffset, batchCounter, rbo);
}

void RenderInstance::InvokeTransferCommands(RecordingBufferObject* rbo)
{
	int memCount = transferCommandPool.linkCount;

	if (!memCount) return;

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);
	
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

		EntryHandle index = bufferHandles[allocations[handle].memIndex];

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

int RenderInstance::GetAllocFromBuffer(int bufferHandle, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	switch (bufferAlignmentType)
	{
	case BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT:
		alignment = (alignment + minUniformAlignment - 1) & ~((size_t)minUniformAlignment - 1);
		break;
	case BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT:
		alignment = (alignment + minStorageAlignment - 1) & ~((size_t)minStorageAlignment - 1);
		break;
	}

	size_t allocSize = ((copiesOfStructure * structureSize) + alignment - 1) & ~(alignment - 1);

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

	size_t location = allocator->Allocate(allocSize * copies, alignment);

	int index = allocations.Allocate();

	RenderAllocation* alloc = allocations.Get(index);

	alloc->memIndex = bufferHandle;
	alloc->offset = location;
	alloc->deviceAllocSize = allocSize * copies;
	alloc->requestedSize = structureSize;
	alloc->alignment = alignment;
	alloc->allocType = allocType;
	alloc->formatType = formatType;
	alloc->structureCopies = copiesOfStructure;

	if (formatType != ComponentFormatType::NO_BUFFER_FORMAT && formatType != ComponentFormatType::RAW_8BIT_BUFFER)
	{
		alloc->viewIndex = dev->CreateBufferView(bufferHandles[bufferHandle], API::ConvertComponentFormatTypeToVulkanFormat(formatType), allocSize, location, copies);
	}

	return index;
}

int RenderInstance::CreateImageHandle(
	size_t gpuMemAddress,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat format, int poolIndex, int samplerIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	VkFormat actualFormat = API::ConvertImageFormatToVulkanFormat(format);

	EntryHandle textureHandle = majorDevice->CreateImageHandle(
		width, height, 1,
		mipLevels, gpuMemAddress, actualFormat,
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_TYPE_2D, 
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, 0);

	if (samplerIndex >= 0)
		majorDevice->AssignSamplerToTexture(textureHandle, samplerResourceHandles[samplerIndex]);

	int textureIndex = textureResourceHandles.Allocate();

	textureResourceHandles.pool[textureIndex] = textureHandle;;

	textureToResourceStatus[textureIndex] = resourceStatuses.Allocate();

	return textureIndex;
}

int RenderInstance::CreateStorageImage(
	size_t gpuMemAddress,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat format, int poolIndex, int samplerIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	EntryHandle textureHandle = majorDevice->CreateImageHandle(
		width, height, 1,
		mipLevels, gpuMemAddress, API::ConvertImageFormatToVulkanFormat(format),
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT, 
		VK_IMAGE_TYPE_2D,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT | 
		VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, 0
	);

	int textureIndex = textureResourceHandles.Allocate();

	textureResourceHandles.pool[textureIndex] = textureHandle;

	if (samplerIndex >= 0)
		majorDevice->AssignSamplerToTexture(textureHandle, samplerResourceHandles[samplerIndex]);

	return textureIndex;
}

int RenderInstance::CreateCubeImageHandle(
	size_t gpuMemAddress,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, ImageFormat format, int poolIndex, int samplerIndex)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	VkFormat actualFormat = API::ConvertImageFormatToVulkanFormat(format);

	EntryHandle textureHandle = majorDevice->CreateImageHandle(
		width, height, 6,
		mipLevels, gpuMemAddress, actualFormat,
		imagePools[poolIndex],
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_TYPE_2D,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	);

	if (samplerIndex >= 0)
		majorDevice->AssignSamplerToTexture(textureHandle, samplerResourceHandles[samplerIndex]);

	int textureIndex = textureResourceHandles.Allocate();

	textureResourceHandles.pool[textureIndex] = textureHandle;

	return textureIndex;
}

void RenderInstance::GetGPURequestedImageSizeAndAlignment(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers, ImageFormat type, size_t* actualImageSize, size_t* actualAlignment)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	VkFormat actualFormat = API::ConvertImageFormatToVulkanFormat(type);

	majorDevice->GetImageMemorySizeAndAlignment(
		width, height,
		mipLevels, actualFormat, layers,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D,
		actualImageSize, actualAlignment
	);
}



int RenderInstance::CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

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
		return -1;
	}

	if (!attachment)
	{
		flags = 0;
	}

	auto poolInfo = majorDevice->FindImageMemoryIndexForPool(maxWidth, maxHeight,
		1, vkFormat, 1,
		flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	int ret = imagePools.Allocate();

	imagePools.pool[ret] = majorDevice->CreateImageMemoryPool(size, poolInfo.first);

	return ret;
}

int RenderInstance::CreateRSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight)
{
	return CreateImagePool(size, format, maxWidth, maxHeight, true);
}

int RenderInstance::CreateDSVMemoryPool(size_t size, ImageFormat format, int maxWidth, int maxHeight)
{
	return CreateImagePool(size, format, maxWidth, maxHeight, true);
}

int RenderInstance::AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount)
{ 
    ShaderResourceSet* set = (ShaderResourceSet*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceSet));
   
    ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);

    ShaderResourceSetTemplate* resourceSet = &graph->shaderResourceSetTemplates[targetSet];

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

		ShaderResource* resource = &graph->shaderResources[resourceSet->resourceStart+h];

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
				ShaderResourceImageBarrier* barriers = (ShaderResourceImageBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceImageBarrier)*2);
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
				ShaderResourceImageBarrier* barriers = (ShaderResourceImageBarrier*)descriptorManager.shaderResourceInstAllocator.Allocate(sizeof(ShaderResourceImageBarrier));
				
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

int RenderInstance::CreateAttachmentGraph(StringView* attachmentLayout, int* subAttachCount)
{
	int attachmentGraphTemplateIndex = attachmentGraphs.Allocate();

	AttachmentGraph* graph = attachmentGraphs.Get(attachmentGraphTemplateIndex);

	CreateAttachmentGraphFromFile(*attachmentLayout, graph, cacheAllocator, internalRendererLogger);

	int currentGraphInstance = CreateFrameGraphInstance(graph);
	
	int currentRenderPassCount = CreateRenderPass(attachmentGraphsInstances.Get(currentGraphInstance));

	if (subAttachCount)
		*subAttachCount = currentRenderPassCount;

	return currentGraphInstance;
}

void RenderInstance::CreateVulkanRenderer(WindowManager* window)
{
	this->windowMan = window;

	void* driverInstanceDataHead = storageAllocator->Allocate((2 * MiB));
	void* instanceDataHead = storageAllocator->Allocate((256 * KiB + 16 * KiB));

	vkInstance->SetInstanceDataAndSize(driverInstanceDataHead, (2 * MiB), 256 * KiB);

	VKInstanceDebugData vkDebugData{};

	vkDebugData.userCallback = vulkanDebugCallback;
	vkDebugData.userData = nullptr;
	vkDebugData.flags = 0;
	vkDebugData.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	vkDebugData.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
	vkDebugData.enables[0] = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
	vkDebugData.enablesFeaturesCount = 0;

	VKInstanceDebugData* vkDebugDataTemp = &vkDebugData;

	RenderingInstanceFeatures instanceFeaturesRequest{};

	instanceFeaturesRequest.useSurface = true;
	instanceFeaturesRequest.useSwapChainMaintenance = true;
	instanceFeaturesRequest.useValidation = true;
	instanceFeaturesRequest.useDebugExt = true;
	instanceFeaturesRequest.windowManagementType = WindowManagementType::WINDOWS32;

	vkInstance->CreateRenderInstance(instanceDataHead, 16*KiB, 256*KiB, vkDebugDataTemp, &instanceFeaturesRequest);

	OSWindowInternalData data;

	windowMan->GetInternalData(&data);

	GPUFeatureRequest request{};

	request.desiredMaxImageWidth = 4096;
	request.desiredMaxImageHeight = 4096;
	request.deviceType = DISCRETE | INTEGRATED;
	request.requireDescriptorBindingPartiallyBound = true;
	request.requireDescriptorBindingSampledImageUpdateAfterBind = true;
	request.requireDescriptorBindingUpdateUnusedWhilePending = true;
	request.requireDescriptorBindingVariableDescriptorCount = true;
	request.requireShaderSampledImageArrayNonUniformIndexing = true;
	request.requireStorageBuffer8BitAccess = true;
	request.requireDrawIndirectCount = true;
	request.requireRuntimeDescriptorArray = true;
	request.requireGeometryShader = false;
	request.requireTextureCompressionBC = true;
	request.requireTessellationShader = false;
	request.requireSamplerAnisotropy = true;
	request.requireMultiDrawIndirect = true;
	request.requireWideLines = true;
	request.requireTimelineSemaphores = true;

	LogicalDeviceFeatures deviceFeatures{};

	deviceFeatures.useSPVDebugInfo = true;
	deviceFeatures.useSPVDrawParameters = true;
	deviceFeatures.useSwapChain = true;
	deviceFeatures.useSwapChainMaintenance = true;

	uint32_t deviceExtNameCount = vkInstance->GetLogicalDeviceExtensionsCount(&deviceFeatures);

	const char** deviceFeatureNames = (const char**)cacheAllocator->Allocate(sizeof(char*) * deviceExtNameCount);

	vkInstance->GetLogicalDeviceExtensions(&deviceFeatures, deviceFeatureNames);

	VkPhysicalDeviceVulkan12Features features12{};
	VkPhysicalDeviceFeatures2 features2{};

	API::ConvertGPUFeatureRequestToVkPhysicalDeviceProperties(&request, &features2, &features12);

	renderSurfaceIndex = vkInstance->CreateWindowedSurface(data.inst, data.wnd);
	physicalIndex = vkInstance->CreatePhysicalDevice(renderSurfaceIndex, &request, deviceFeatureNames, deviceExtNameCount);
	deviceIndex = vkInstance->CreateLogicalDevice(physicalIndex);

	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	minUniformAlignment = vkInstance->GetMinimumUniformBufferAlignment(physicalIndex);
	minStorageAlignment = vkInstance->GetMinimumStorageBufferAlignment(physicalIndex);

	maxMSAALevels = std::max(std::bit_width((uint32_t)vkInstance->GetMaxMSAALevels(physicalIndex))-1, 1);

	uint32_t totalQueueFamilyCount = majorDevice->QueueFamilyDetailsCount();

	VkQueueFamilyProperties* famPropsContainer = (VkQueueFamilyProperties*)cacheAllocator->CAllocate(totalQueueFamilyCount * sizeof(VkQueueFamilyProperties));

	std::array<QueueIndex, 2> queueIndices{};
	std::array<uint32_t, 2> queueCounts{};

	uint32_t queueCount = 0, totalQueuePrios = 0;

	int queueSuccessful = majorDevice->GetQueueByMask(&queueIndices[0], &queueCounts[0], VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, famPropsContainer);

	if (queueSuccessful)
	{
		//TO-DO implement fallback
	}

	queueSuccessful = majorDevice->GetPresentQueue(&queueIndices[1], &queueCounts[1], vkInstance->GetRenderSurface(renderSurfaceIndex), famPropsContainer);

	if (queueSuccessful)
	{
		//TO-DO implement error handling
	}

	if (queueIndices[0] == queueIndices[1])
	{
		queueCount = 1;
		totalQueuePrios = queueCounts[0];
	}
	else
	{
		queueCount = 2;
		totalQueuePrios = queueCounts[0] + queueCounts[1];
	}

	float* queuePriorites = reinterpret_cast<float*>(cacheAllocator->Allocate(sizeof(float) * totalQueuePrios));

	for (int i = 0; i < totalQueuePrios; i++)
	{
		queuePriorites[i] = 1.0f;
	}

	void* driverDeviceDataHead = storageAllocator->Allocate((12 * MiB) + (16 * KiB));
	void* deviceDataHead = storageAllocator->Allocate(64 * KiB + (96 * KiB) + (16 * KiB) + 64);

	majorDevice->CreateLogicalDevice(
		deviceFeatureNames, 
		deviceExtNameCount,
		&features2,
		queueIndices.data(),
		queueCounts.data(),
		queuePriorites,
		queueCount, 
		64 * KiB,
		16 * KiB,
		96 * KiB,
		12 * MiB,
		16 * KiB,
		driverDeviceDataHead,
		deviceDataHead
	);

	graphicsComputeTransfer = majorDevice->CreateQueueManager(queueIndices[0], queueCounts[0], VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, (queueCount == 1) ? true : false);

	if (queueCount > 1)
	{
		presentQueue = majorDevice->CreateQueueManager(queueIndices[1], queueCounts[1], 0, true);
	}
	else
	{
		presentQueue = graphicsComputeTransfer;
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		stagingBuffers[i] = majorDevice->CreateHostBuffer(128 * MiB, true, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		stagingBufferAllocators[i].dataSize = 128 * MiB;
		stagingBufferAllocators[i].dataAllocator = 0;
	}

	DescriptorPoolBuilder builder = majorDevice->CreateDescriptorPoolBuilder(3, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
	builder.AddStoragePoolSize(MAX_FRAMES_IN_FLIGHT * 100);

	descriptorManager.deviceResourceHeap = majorDevice->CreateDesciptorPool(&builder, MAX_FRAMES_IN_FLIGHT * 100);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		EntryHandle* lprimaryCommandBuffers = majorDevice->CreateReusableCommandBuffers(graphicsComputeTransfer, 1, true, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		currentCBIndex[i] = *lprimaryCommandBuffers;
	}

	cacheAllocator->Reset();

	queryPool = majorDevice->CreateQueryPool(VK_QUERY_TYPE_TIMESTAMP, MAX_FRAMES_IN_FLIGHT * 5 * 2);

	deviceTimeStampPeriodNS = vkInstance->GetTimeStampPeriod(physicalIndex);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		rendererWaitSemaphores[i] = *majorDevice->CreateSemaphores(1);
		rendererFinishedSemaphores[i] = *majorDevice->CreateSemaphores(1);
	}

	rendererTimelineSyncObject.currentValue = 0;
	rendererTimelineSyncObject.driverTimelineObject = *majorDevice->CreateTimelineSemaphores(1, rendererTimelineSyncObject.currentValue);
}

EntryHandle RenderInstance::CreateSwapChainHandle(ImageFormat mainBackBufferColorFormat, uint32_t _width, uint32_t _height)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	EntryHandle swapChainIndex = majorDevice->CreateSwapChain(MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT, API::ConvertImageFormatToVulkanFormat(mainBackBufferColorFormat), renderSurfaceIndex);

	CreateSwapChainData(swapChainIndex, _width, _height, false);

	return swapChainIndex;
}

ImageFormat RenderInstance::FindSupportedBackBufferColorFormat(ImageFormat* requestedFormats, uint32_t requestSize)
{
	for (uint32_t i = 0; i < requestSize; i++)
	{
		bool ret = vkInstance->ValidateSwapChainFormatSupport(physicalIndex, API::ConvertImageFormatToVulkanFormat(requestedFormats[i]), renderSurfaceIndex);

		if (ret)
		{
			return requestedFormats[i];
		}
	}

	return ImageFormat::IMAGE_UNKNOWN;
}

ImageFormat RenderInstance::FindSupportedDepthFormat(ImageFormat* requestedFormats, uint32_t requestSize)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	VkFormat format;

	for (uint32_t i = 0; i < requestSize; i++)
	{
		format = API::ConvertImageFormatToVulkanFormat(requestedFormats[i]);

		format = VK::Utils::findSupportedFormat(majorDevice->gpu,
			&format,
			1,
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		if (format != VK_FORMAT_UNDEFINED)
		{
			return requestedFormats[i];
		}
	}

	return ImageFormat::IMAGE_UNKNOWN;
}

int RenderInstance::CreateSampler(uint32_t maxMipsLevel)
{
	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	EntryHandle samplerHandle = majorDevice->CreateSampler(maxMipsLevel);

	int samplerIndex = samplerResourceHandles.Allocate();

	samplerResourceHandles.pool[samplerIndex] = samplerHandle;

	return samplerIndex;
}

uint32_t RenderInstance::GetSwapChainHeight(EntryHandle swapChainIndex)
{
	return vkInstance->GetLogicalDevice(deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainHeight();
}

uint32_t RenderInstance::GetSwapChainWidth(EntryHandle swapChainIndex)
{
	return vkInstance->GetLogicalDevice(deviceIndex)->GetSwapChain(swapChainIndex)->GetSwapChainWidth();
}

EntryHandle RenderInstance::CreateShaderResourceSet(int descriptorSet)
{
	//TODO handle this better
	if (descriptorManager.descriptorSetHandles[descriptorSet] != EntryHandle())
	{
		return descriptorManager.descriptorSetHandles[descriptorSet];
	}

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);
	uintptr_t head = descriptorManager.descriptorSets[descriptorSet];
	ShaderResourceSet* set = (ShaderResourceSet*)head;
	uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

	int frames = set->setCount;	

	uint32_t varCountRequested = 0;

	int bindingCount = set->bindingCount;

	int lastBinding = std::max(bindingCount - set->constantsCount - 1, 0);

	ShaderResourceHeader* lastheader = (ShaderResourceHeader*)offsets[lastBinding];

	if (lastheader->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY)
	{
		varCountRequested = (lastheader->arrayCount & DESCRIPTOR_COUNT_MASK);
	}

	DescriptorSetBuilder* builder = dev->CreateDescriptorSetBuilder(descriptorManager.deviceResourceHeap, shaderResourceTemplates[set->layoutHandle], frames, varCountRequested);

	for (int i = 0; i < bindingCount; i++)
	{
		ShaderResourceHeader* header = (ShaderResourceHeader*)offsets[i];

		switch (header->type)
		{
			case ShaderResourceType::SAMPLERSTATE:
			{
			ShaderResourceSampler* image = (ShaderResourceSampler*)header;
			if (!image->samplerHandles) break;
			for (int sampler = 0; sampler < image->samplerCount; sampler++)
			{
				builder->AddSamplerDescription(samplerResourceHandles[image->samplerHandles[sampler]], image->firstSampler + sampler, i, 0, frames);
			}
			break;
			}
			case ShaderResourceType::IMAGE2D:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				if (!image->textureHandles) break;

				for (int imageIndex = 0; imageIndex < image->textureCount; imageIndex++)
				{
					builder->AddImageResourceDescription(textureResourceHandles[image->textureHandles[imageIndex]], image->firstTexture + imageIndex, i, 0, frames);
				}

			
				break;
			}
			case ShaderResourceType::IMAGESTORE2D:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				//builder->AddStorageImageDescription(image->textureHandles, image->textureCount, image->firstTexture, i, 0, frames);
				break;
			}
			case ShaderResourceType::SAMPLER3D:
			case ShaderResourceType::SAMPLER2D:
			case ShaderResourceType::SAMPLERCUBE:
			{
				ShaderResourceImage* image = (ShaderResourceImage*)header;
				if (!image->textureHandles) break;

				for (int imageIndex = 0; imageIndex < image->textureCount; imageIndex++)
				{
					builder->AddCombinedTextureArray(textureResourceHandles[image->textureHandles[imageIndex]], image->firstTexture + imageIndex, i, 0, frames);
				}
				
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
						builder->AddStorageBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize / frames, i, frames, alloc.offset, 0, firstBuffer + j);
					else
						builder->AddStorageBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize, i, frames, alloc.offset, 0, firstBuffer + j);
					
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
						builder->AddUniformBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize / frames, i, frames, alloc.offset, 0, firstBuffer+j);
					else
						builder->AddUniformBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex]), alloc.deviceAllocSize, i, frames, alloc.offset, 0, firstBuffer+j);
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
					RenderAllocation* alloc = allocations.Get(bufferView->allocationIndex[j]);

					int frameCount = (alloc->allocType == AllocationType::PERFRAME) ? MAX_FRAMES_IN_FLIGHT : 1;

					for (int g = 0; g < frameCount; g++)
					{
						VkBufferView handle = dev->GetBufferView(alloc->viewIndex, g);

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

	descriptorManager.descriptorSetHandles.pool[descriptorSet] = handle;

	return handle;
}

int RenderInstance::CreateGraphicsPipelineObject(GraphicsIntermediaryPipelineInfo* info, bool addToGraph)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineInstanceData* pid = &pipelinesInstancesInfo[info->pipelinename];

	int ret = pipelineHandles.Allocate();

	PipelineHandle* posStruct = pipelineHandles.Get(ret);
	
	posStruct->group = GRAPHICSO;
	posStruct->pipelineIdentifierGroup = info->pipelinename;
	posStruct->resourceSetCount = info->descCount;

	uint32_t pushRangeCount = 0;

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		posStruct->resourceSets[i] = info->descriptorsetid[i];
		CreateShaderResourceSet(info->descriptorsetid[i]);
		pushRangeCount += descriptorManager.GetConstantBufferCount(info->descriptorsetid[i]);
	}

	posStruct->pushRangeCount = pushRangeCount;

	uint32_t indexOffset = ~0ui32;

	uint32_t vertexOffset = ~0ui32;

	uint32_t indirectOffset = ~0ui32;
	uint32_t indirectBufferPerFrameSize = 0;

	uint32_t indirectCountOffset = ~0ui32;
	uint32_t indirectCountBufferPerFrameSize = 0;

	if (info->indexBufferHandle != ~0)
	{
		indexOffset = static_cast<uint32_t>(allocations[info->indexBufferHandle].offset) + info->indexOffset;
	}

	if (info->vertexBufferHandle != ~0)
	{
		vertexOffset = static_cast<uint32_t>(allocations[info->vertexBufferHandle].offset) + info->vertexOffset;
	}

	if (info->indirectAllocation != ~0)
	{
		size_t align = allocations[info->indirectAllocation].alignment;
		uint32_t copiesOfstruct = static_cast<uint32_t>(allocations[info->indirectAllocation].structureCopies);
		indirectOffset = static_cast<uint32_t>(allocations[info->indirectAllocation].offset);
		indirectBufferPerFrameSize = static_cast<uint32_t>(((allocations[info->indirectAllocation].requestedSize * copiesOfstruct) + align - 1) & ~(align - 1)) ;
	}

	if (info->indirectCountAllocation != ~0)
	{
		size_t align = allocations[info->indirectCountAllocation].alignment;
		uint32_t copiesOfstruct = static_cast<uint32_t>(allocations[info->indirectCountAllocation].structureCopies);
		indirectCountOffset = static_cast<uint32_t>(allocations[info->indirectCountAllocation].offset);
		indirectCountBufferPerFrameSize = static_cast<uint32_t>(((allocations[info->indirectCountAllocation].requestedSize * copiesOfstruct) + align - 1) & ~(align - 1));
	}

	posStruct->indexBufferHandle = info->indexBufferHandle;
	posStruct->indexBufferOffset = indexOffset;
	posStruct->indexCount = info->indexCount;
	posStruct->vertexBufferIndex = info->vertexBufferHandle;
	posStruct->vertexBufferOffset = vertexOffset;
	posStruct->vertexCount = info->vertexCount;
	posStruct->indirectBufferFrames = indirectBufferPerFrameSize;
	posStruct->indirectBufferHandle = info->indirectAllocation;
	posStruct->indirectBufferOffset = indirectOffset;
	posStruct->indirectCountBufferHandle = info->indirectCountAllocation;
	posStruct->indirectCountBufferOffset = indirectCountOffset;
	posStruct->indirectCountBufferStride = indirectCountBufferPerFrameSize;
	posStruct->instanceCount = info->instanceCount;
	posStruct->indexSize = info->indexSize;
	posStruct->indirectDrawCount = info->indirectDrawCount;

	int renderStateCount = 0;

	for (uint32_t a = 0; a < pid->frameGraphCount; a++)
	{
		AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(pid->frameGraphIndices[a]);

		AttachmentRenderPassInstance* renderPassInstance = &graphInstance->passes[pid->frameGraphRenderPasses[a]];

		renderStateCount += renderPassInstance->maxSampleCount;
	}

	posStruct->numHandles = renderStateCount;

	return ret;
}

ShaderComputeLayout* RenderInstance::GetComputeLayout(int shaderGraphIndex)
{
	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);
	ShaderMap* map = &graph->shaderMaps[0];
	ShaderDetails* details = &shaderGraphs.shaderDetails[map->shaderReference];
	return &details->computeLayout;
}

int RenderInstance::CreateComputePipelineObject(ComputeIntermediaryPipelineInfo* info)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int ret = pipelineHandles.Allocate();

	PipelineHandle* posStruct = pipelineHandles.Get(ret);
	
	posStruct->numHandles = 1;
	posStruct->group = COMPUTESO;
	posStruct->pipelineIdentifierGroup = info->pipelinename;
	posStruct->resourceSetCount = info->descCount;
	posStruct->x = info->x;
	posStruct->y = info->y;
	posStruct->z = info->z;
	
	uint32_t pushRangeCount = 0;

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		posStruct->resourceSets[i] = info->descriptorsetid[i];
		CreateShaderResourceSet(info->descriptorsetid[i]);
		pushRangeCount += descriptorManager.GetConstantBufferCount(info->descriptorsetid[i]);
	}

	posStruct->pushRangeCount = pushRangeCount;

	return ret;
}

void RenderInstance::DrawScene(uint32_t imageIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	stagingBufferAllocators[currentFrame].dataAllocator = 0;

	EntryHandle cbindex = currentCBIndex[currentFrame];
	RecordingBufferObject rcb = dev->GetRecordingBufferObject(cbindex);
	rcb.ResetCommandPoolForBuffer();

	SwapUpdateCommands();

	UploadHostTransfers();

	UploadDescriptorsUpdates();

	rcb.BeginRecordingCommand(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	rcb.ResetQueries(queryPool, 5 * 2 * currentFrame, 5 * 2);

	UploadDeviceLocalTransfers(&rcb);

	InvokeTransferCommands(&rcb);

	UploadImageMemoryTransfers(&rcb);

	int commandCountIter = 0;

	int queryCountBaseIndex = 5 * 2 * currentFrame;

	int queryCountIndex = queryCountBaseIndex;

	while (commandCountIter <= gpuCommandCount)
	{
		GPUCommand* command = &gpuCommands[commandCountIter];

		if (command->streamType == GPUCommandStreamType::COMPUTE_QUEUE_COMMANDS)
		{
			rcb.WriteTimestamp(queryPool, queryCountIndex, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			
			ComputeQueue* queue = computeQueues.Get(command->indexForStreamType);

			for (uint32_t pipeInst = 0; pipeInst < queue->queueCount; pipeInst++)
			{
				PipelineHandle* handle = pipelineHandles.Get(queue->pipelines[pipeInst]);

				EntryHandle pipelineTemp = pipelineInstancesIdentifier[handle->pipelineIdentifierGroup][0];

				rcb.BindComputePipeline(pipelineTemp);

				for (uint32_t ii = 0; ii < handle->resourceSetCount; ii++)
				{
					rcb.BindComputeDescriptorSets(descriptorManager.descriptorSetHandles[handle->resourceSets[ii]], currentFrame, 1, ii, 0, nullptr);
				}

				for (uint32_t ii = 0, jj = 0, constantBufferPerSet = 0; ii < handle->pushRangeCount && jj < handle->resourceSetCount;)
				{
					ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(handle->resourceSets[jj], constantBufferPerSet++);
					if (!pushArgs)
					{
						jj++;
						constantBufferPerSet = 0;
						continue;
					}

					rcb.PushConstantsCommand(pushArgs->offset, pushArgs->size, API::ConvertShaderStageToVulkanShaderStage(pushArgs->stage), pushArgs->data);

					ii++;
				}

				rcb.DispatchCommand(handle->x, handle->y, handle->z);

				AddVulkanMemoryBarrier(&rcb, handle->resourceSets, handle->resourceSetCount);
			}
			
			rcb.WriteTimestamp(queryPool, queryCountIndex+1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			queryCountIndex += 2;
		}
		else if (command->streamType == GPUCommandStreamType::ATTACHMENT_COMMANDS)
		{
			AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(command->indexForStreamType);

			for (int i = 0; i < currentGraphInstance->graphLayout->passesCount; i++)
			{
				rcb.WriteTimestamp(queryPool, queryCountIndex, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

				AttachmentRenderPassInstance* rpInst = &currentGraphInstance->passes[i];

				int SubRenderTargetSelection = rpInst->rpType == RenderPassType::SWAPCHAIN_IMAGE_COUNT ? imageIndex : currentFrame;

				int sampleLevelForRenderPass = rpInst->currentSampleCount;

				int possibleQueueIndex = rpInst->graphicsOTQIndex;

				int renderTargetPerPassBase = rpInst->baseRenderTargetData;

				int absoluteRenderTargetIndex = currentGraphInstance->consecutiveRenderTargetsBase + renderTargetPerPassBase + sampleLevelForRenderPass;

				RenderTarget* renderTarget = dev->GetRenderTarget(mainRenderTargets[absoluteRenderTargetIndex]);

				VkClearValue* clears = (VkClearValue*)cacheAllocator->CAllocate(sizeof(VkClearValue) * rpInst->attachInstCount);

				AttachmentInstance* instances = rpInst->attachInst;

				uint32_t clearCount = 0;

				for (int g = 0; g < rpInst->attachInstCount; g++)
				{
					VkClearValue* currClear = &clears[clearCount];
					switch (instances[g].clear.type)
					{
					case NOCLEAR:
						break;
					case CLEARCOLOR:
						currClear->color.float32[0] = instances[g].clear.val.cdata[0];
						currClear->color.float32[1] = instances[g].clear.val.cdata[1];
						currClear->color.float32[2] = instances[g].clear.val.cdata[2];
						currClear->color.float32[3] = instances[g].clear.val.cdata[3];
						clearCount++;
						break;
					case CLEARDEPTH:
						currClear->depthStencil.depth = instances[g].clear.val.ddata;
						currClear->depthStencil.stencil = instances[g].clear.val.sdata;
						clearCount++;
						break;
					}
				}

				rcb.BeginRenderPassCommand(mainRenderTargets[absoluteRenderTargetIndex], SubRenderTargetSelection, VK_SUBPASS_CONTENTS_INLINE, { {0, 0}, {renderTarget->width, renderTarget->height} }, clears, clearCount);

				float x = static_cast<float>(renderTarget->width), y = static_cast<float>(renderTarget->height);

				float xOff = static_cast<float>(renderTarget->wOffset), yOff = static_cast<float>(renderTarget->hOffset);

				rcb.SetViewportCommand(xOff, yOff, x, y, 0.0f, 1.0f);

				rcb.SetScissorCommand(renderTarget->wOffset, renderTarget->hOffset, renderTarget->width, renderTarget->height);

				if (possibleQueueIndex >= 0)
				{
					RenderQueue* queue = renderTargetQueues.Get(possibleQueueIndex);

					for (uint32_t pipeInst = 0; pipeInst < queue->queueCount; pipeInst++)
					{
						PipelineHandle* handle = pipelineHandles.Get(queue->pipelines[pipeInst]);

						PipelineInstanceData* pid = &pipelinesInstancesInfo[handle->pipelineIdentifierGroup];

						uint32_t pipelineOffset = 0;

						for (int i = 0; i < pid->frameGraphCount; i++)
						{
							if (command->indexForStreamType == pid->frameGraphIndices[i])
							{
								pipelineOffset = pid->frameGraphPipelineIndices[i];
								break;
							}
						}

						EntryHandle pipelineTemp = pipelineInstancesIdentifier[handle->pipelineIdentifierGroup][pipelineOffset + sampleLevelForRenderPass];

						uint32_t drawSize = handle->vertexCount;

						uint32_t perFrameCommandOffset = handle->indirectBufferFrames;

						uint32_t perFrameCountOffset = handle->indirectCountBufferStride;

						RenderAllocation vertexAlloc = allocations[handle->vertexBufferIndex];

						RenderAllocation indexAlloc = allocations[handle->indexBufferHandle];

						RenderAllocation indirectBufferAlloc = allocations[handle->indirectBufferHandle];

						RenderAllocation indirectCountBufferAlloc = allocations[handle->indirectCountBufferHandle];

						rcb.BindGraphicsPipeline(pipelineTemp);

						for (uint32_t ii = 0; ii < handle->resourceSetCount; ii++)
						{
							rcb.BindGraphicsDescriptorSets(descriptorManager.descriptorSetHandles[handle->resourceSets[ii]], currentFrame, 1, ii, 0, nullptr);
						}

						if (handle->vertexBufferIndex != -1)
						{
							size_t vertexOffset = handle->vertexBufferOffset;
							rcb.BindVertexBuffer(bufferHandles[vertexAlloc.memIndex], 0, 1, &vertexOffset);
						}

						for (uint32_t ii = 0, jj = 0, constantBufferPerSet = 0; ii < handle->pushRangeCount && jj < handle->resourceSetCount;)
						{
							ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager.GetConstantBuffer(handle->resourceSets[jj], constantBufferPerSet++);
							if (!pushArgs)
							{
								jj++;
								constantBufferPerSet = 0;
								continue;
							}
					
							rcb.PushConstantsCommand(pushArgs->offset, pushArgs->size, API::ConvertShaderStageToVulkanShaderStage(pushArgs->stage), pushArgs->data);

							ii++;
						}

						if (handle->indirectBufferHandle != -1)
						{
							perFrameCommandOffset *=  currentFrame;

							perFrameCountOffset *= currentFrame;

							if (handle->indexBufferHandle != -1)
							{
								rcb.BindIndexBuffer(bufferHandles[indexAlloc.memIndex], handle->indexBufferOffset, handle->indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

								if (handle->indirectCountBufferHandle != -1)
								{
									rcb.BindingDrawIndexedIndirectCount(bufferHandles[indirectBufferAlloc.memIndex], bufferHandles[indirectCountBufferAlloc.memIndex], handle->indirectBufferOffset + perFrameCommandOffset, handle->indirectCountBufferOffset + perFrameCountOffset, handle->indirectDrawCount);
								}
								else
								{
									rcb.BindingIndexedIndirectDrawCmd(bufferHandles[indirectBufferAlloc.memIndex], handle->indirectDrawCount, handle->indirectBufferOffset + perFrameCommandOffset);
								}
							}
							else
							{
								if (handle->indirectCountBufferHandle != -1)
								{
									rcb.BindingDrawIndirectCount(bufferHandles[indirectBufferAlloc.memIndex], bufferHandles[indirectCountBufferAlloc.memIndex], handle->indirectBufferOffset + perFrameCommandOffset, handle->indirectCountBufferOffset + perFrameCountOffset, handle->indirectDrawCount);
								}
								else
								{
									rcb.BindingIndirectDrawCmd(bufferHandles[indirectBufferAlloc.memIndex], handle->indirectDrawCount, handle->indirectBufferOffset + perFrameCommandOffset);
								}
							}
						}
						else
						{
							if (handle->indexBufferHandle != -1)
							{
								rcb.BindIndexBuffer(bufferHandles[indexAlloc.memIndex], handle->indexBufferOffset, handle->indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
								rcb.BindingDrawIndexedCmd(static_cast<uint32_t>(handle->indexCount), handle->instanceCount, 0, 0, 0);
							}
							else
							{
								rcb.BindingDrawCmd(0, drawSize, 0, handle->instanceCount);
							}
						}
					}
				}

				rcb.EndRenderPassCommand();

				rcb.WriteTimestamp(queryPool, queryCountIndex+1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
			
				queryCountIndex += 2;
			}
		}

		commandCountIter++;
	}

	queryCounts[currentFrame] = (queryCountIndex - queryCountBaseIndex);

	rcb.EndRecordingCommand();
}

void RenderInstance::IncreaseMSAA(int frameGraph, int renderPassIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraph);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	int next = passInstance->currentSampleCount + 1;

	if (next >= maxMSAALevels)
		return;

	if (next >= passInstance->maxSampleCount)
		return;

	passInstance->currentSampleCount = next;
}

void RenderInstance::DecreaseMSAA(int frameGraph, int renderPassIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraph);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	int next = passInstance->currentSampleCount - 1;

	if (next < 0)
		return;

	passInstance->currentSampleCount = next;
}

void RenderInstance::ResetCommandList()
{
	gpuCommandCount = 0;
}

void RenderInstance::CreateGraphicsQueueForAttachments(int frameGraphIndex, int renderPassIndex, uint32_t pipelineCount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraphIndex);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	passInstance->graphicsOTQIndex = renderTargetQueues.Allocate();;
}

int RenderInstance::CreateComputeQueue(uint32_t maxNumPipelines)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	return computeQueues.Allocate();
}


void RenderInstance::AddCommandQueue(int index, GPUCommandStreamType type)
{
	GPUCommand* command = &gpuCommands[gpuCommandCount++];
	command->indexForStreamType = index;
	command->streamType = type;
}

void RenderInstance::EndFrame()
{
	char StringBuffer[512];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int commandCountIter = 0;

	int queryOffset = 0;

	while (commandCountIter < gpuCommandCount)
	{
		GPUCommand* command = &gpuCommands[commandCountIter];

		const char* passDesc = "Undefined pass : ";

		int queryCount = 0;

		if (command->streamType == GPUCommandStreamType::COMPUTE_QUEUE_COMMANDS)
		{
			ComputeQueue* computeQueue = computeQueues.Get(command->indexForStreamType);

			computeQueue->queueCount = 0;

			passDesc = "Compute Pass : ";

			queryCount = 2;
		}
		else if (command->streamType == GPUCommandStreamType::ATTACHMENT_COMMANDS)
		{
			AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(command->indexForStreamType);

			for (int i = 0; i < currentGraphInstance->graphLayout->passesCount; i++)
			{
			
				if (currentGraphInstance->passes[i].graphicsOTQIndex >= 0)
				{
					RenderQueue* queue = renderTargetQueues.Get(currentGraphInstance->passes[i].graphicsOTQIndex);

					queue->queueCount = 0;
				}

				queryCount += 2;
			}

			passDesc = "Render Pass : ";
		}

		if (previousFrame < MAX_FRAMES_IN_FLIGHT && (queryCounts[previousFrame] >= queryOffset + queryCount))
		{
			dev->ReadbackResultsFromQueries(
				queryPool,
				(5 * 2 * previousFrame) + queryOffset,
				queryCount,
				queryResults, 
				sizeof(uint32_t) * maxQueryResults,
				sizeof(uint32_t), 
				VK_QUERY_RESULT_WAIT_BIT
			);

			for (uint32_t i = 0; i < queryCount; i += 2)
			{
				double timeNs = (queryResults[i + 1] - queryResults[i]) * deviceTimeStampPeriodNS;
				double timeMs = timeNs / 1e6;

				int actualSize = snprintf(StringBuffer, 512, "%s Time to run pass: %lf", passDesc, timeMs);

				internalRendererLogger->AddLogMessage(LOGINFO, StringBuffer, actualSize);
			}
		}

		queryOffset += queryCount;

		commandCountIter++;
	}

	cacheAllocator->Reset();

	previousFrame = currentFrame;
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RenderInstance::AddPipelineToRPGraphicsQueue(int psoIndex, int frameGraphIndex, int renderPass)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(frameGraphIndex);

	AttachmentRenderPassInstance* rendPassInst = &currentGraphInstance->passes[renderPass];

	if (rendPassInst->graphicsOTQIndex >= 0)
	{
		RenderQueue* queue = renderTargetQueues.Get(rendPassInst->graphicsOTQIndex);

		queue->pipelines[queue->queueCount++] = psoIndex;
	}
}

void RenderInstance::AddPipelineToComputeQueue(int queueIndex, int psoIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	ComputeQueue* queue = computeQueues.Get(queueIndex);

	queue->pipelines[queue->queueCount++] = psoIndex;
}


void RenderInstance::ReadData(int handle, void* dest, int size, int offset)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	auto alloc = allocations[handle];

	if (bufferTypes[alloc.memIndex] != BufferType::HOST_MEMORY_TYPE)
	{
		return;
	}

	size_t intOffset = alloc.offset;
	
	EntryHandle index = bufferHandles[alloc.memIndex];

	dev->ReadHostBuffer(dest, index, size, intOffset+offset);
}



void RenderInstance::UpdateDriverMemory(void* data, int allocationIndex, int size, int allocOffset, TransferType transferType)
{
	void* outData = data;

	int copies = 1;

	if (transferType == TransferType::CACHED)
	{
		outData = updateCommandsCache->Allocate(size, 16);
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

void RenderInstance::UpdateImageMemory(void* data, int textureIndex, size_t totalSize, int width, int height, int mipLevels, int layers, ImageFormat format)
{

	RenderDriverUpdateCommandImage* rduci = (RenderDriverUpdateCommandImage*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandImage));

	rduci->data = data;
	rduci->format = format;
	rduci->height = height;
	rduci->mipLevels = mipLevels;
	rduci->width = width;
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

void RenderInstance::UpdateShaderResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, DeviceHandleArrayUpdate* resourceArrayData)
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
		DeviceHandleArrayUpdate* cachedUpdate = (DeviceHandleArrayUpdate*)(updateCommandsCache->Allocate(sizeof(DeviceHandleArrayUpdate)));
		
		cachedUpdate->resourceDstBegin = resourceArrayData->resourceDstBegin;
		cachedUpdate->resourceCount = resCount;
		cachedUpdate->resourceHandles = (int*)(updateCommandsCache->Allocate(sizeof(int) * resCount));
		
		memcpy(cachedUpdate->resourceHandles, resourceArrayData->resourceHandles, sizeof(int) * resCount);
		
		argData = cachedUpdate;
		argSize = (sizeof(int) * resCount) + sizeof(DeviceHandleArrayUpdate);
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


void RenderInstance::UpdateBufferResourceArray(int descriptorid, int bindingindex, ShaderResourceType type, BufferArrayUpdate* resourceArrayData)
{
	RenderDriverUpdateCommandResource* rducr = (RenderDriverUpdateCommandResource*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandResource));

	int argSize = 0;

	void* argData = nullptr;

	int resCount = resourceArrayData->allocationCount;

	switch (type)
	{
	case ShaderResourceType::STORAGE_BUFFER:
	case ShaderResourceType::UNIFORM_BUFFER:
	case ShaderResourceType::BUFFER_VIEW:
	{
		BufferArrayUpdate* cachedUpdate = (BufferArrayUpdate*)(updateCommandsCache->Allocate(sizeof(BufferArrayUpdate)));

		cachedUpdate->resourceDstBegin = resourceArrayData->resourceDstBegin;
		cachedUpdate->allocationCount = resCount;
		cachedUpdate->allocationIndices = (int*)(updateCommandsCache->Allocate(sizeof(int) * resCount));

		memcpy(cachedUpdate->allocationIndices, resourceArrayData->allocationIndices, sizeof(int) * resCount);

		argData = cachedUpdate;
		argSize = (sizeof(int) * resCount) + sizeof(BufferArrayUpdate);
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
			imageMemoryUpdateManager.Create(rduci->data, rduci->textureIndex, rduci->totalSize, rduci->width, rduci->height, rduci->mipLevels, rduci->layers, rduci->format);
			header = rduci->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandImage);
			break;
		}
		case DriverUpdateType::MEMORYUPDATE:
		{
			RenderDriverUpdateCommandMemory* rducm = (RenderDriverUpdateCommandMemory*)header;
			

			if (bufferTypes[allocations[rducm->allocationIndex].memIndex] == BufferType::HOST_MEMORY_TYPE)
			{
				driverHostMemoryUpdater.Create(rducm->data, rducm->size, rducm->allocationIndex, rducm->allocOffset, rducm->copiesWithin);
			}
			else if (bufferTypes[allocations[rducm->allocationIndex].memIndex] == BufferType::DEVICE_MEMORY_TYPE)
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


int RenderInstance::UploadFrameAttachmentResource(int frameGraph, int resourceIndex, int descriptorSet, int bindingIndex, int textureStart)
{
	AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(frameGraph);

	EntryHandle* imageViews = currentGraphInstance->resources[resourceIndex].attachmentImageView[0];

	int imageCount = currentGraphInstance->resources[resourceIndex].imageCount;

	int* textureIds = (int*)cacheAllocator->Allocate(sizeof(int) * imageCount);

	for (int i = 0; i < imageCount; i++)
	{
		int textureIndex = textureResourceHandles.Allocate();
		textureResourceHandles.pool[textureIndex] = imageViews[i];
		textureIds[i] = textureIndex;
	}

	DeviceHandleArrayUpdate update;

	update.resourceCount = imageCount;
	update.resourceDstBegin = textureStart;
	update.resourceHandles = textureIds;

	UpdateShaderResourceArray(descriptorSet, bindingIndex, ShaderResourceType::IMAGE2D, &update);

	return imageCount;
}

void RenderInstance::PipelineUpdateInstanceCommandsBuffer(int pipelineIndex, int allocationIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	auto alloc = allocations[allocationIndex];

	size_t align = alloc.alignment;
	uint32_t copiesOfstruct = static_cast<uint32_t>(alloc.structureCopies);

	EntryHandle indirectBufferHandle = bufferHandles[alloc.memIndex];
	size_t indirectOffset = alloc.offset;
	size_t indirectBufferPerFrameSize = ((alloc.requestedSize * copiesOfstruct) + align - 1) & ~(align - 1);
	
	if (handle->group == GRAPHICSO)
	{
	
	}
}

void RenderInstance::PipelineUpdateVertexBuffer(int pipelineIndex, int allocationIndex, int vertexCount, int vertexBuffersubAlloc)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	auto alloc = allocations[allocationIndex];

	EntryHandle vertexBufferHandle = bufferHandles[alloc.memIndex];
	size_t vertexOffset = alloc.offset + vertexBuffersubAlloc;
	size_t vertexCountLL = static_cast<size_t>(vertexCount);

	if (handle->group == GRAPHICSO)
	{
	
	}
}

void RenderInstance::PipelineUpdateIndexBuffer(int pipelineIndex, int allocationIndex, int indexCount, int indexStride, int indexSubAlloc)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	auto alloc = allocations[allocationIndex];

	EntryHandle indexBufferHandle = bufferHandles[alloc.memIndex];
	size_t indexOffset = alloc.offset + indexSubAlloc;
	size_t indexCountLL = static_cast<size_t>(indexStride);

	if (handle->group == GRAPHICSO)
	{
	
	}
}


void RenderInstance::PipelineUpdateIndirectCountBuffer(int pipelineIndex, int allocationIndex)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	auto alloc = allocations[allocationIndex];

	size_t align = alloc.alignment;
	uint32_t copiesOfstruct = static_cast<uint32_t>(alloc.structureCopies);
	EntryHandle indirectCountBufferHandle = bufferHandles[alloc.memIndex];
	size_t indirectCountOffset = alloc.offset;
	size_t indirectCountBufferPerFrameSize = ((alloc.requestedSize * copiesOfstruct) + align - 1) & ~(align - 1);

	if (handle->group == GRAPHICSO)
	{
		
	}
}

void RenderInstance::PipelineUpdateDispatchCommands(int pipelineIndex, int x, int y, int z)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	uint32_t xU = static_cast<uint32_t>(x);
	uint32_t yU = static_cast<uint32_t>(y);
	uint32_t zU = static_cast<uint32_t>(z);

	if (handle->group == COMPUTESO)
	{
	
	}
}

int RenderInstance::CreateUniversalBuffer(size_t size, BufferType bufferMemoryType)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	int bufferIndex = bufferHandles.Allocate();

	EntryHandle bufferHandle;

	if (bufferMemoryType == BufferType::HOST_MEMORY_TYPE)
	{
		bufferHandle = dev->CreateHostBuffer
		(
			size, true,
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
	}
	else if (bufferMemoryType == BufferType::DEVICE_MEMORY_TYPE)
	{
		bufferHandle = dev->CreateDeviceBuffer(size,
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	}
	
	bufferHandles.pool[bufferIndex] = bufferHandle;
	bufferTypes[bufferIndex] = bufferMemoryType;
	bufferToResourceStatus[bufferIndex] = resourceStatuses.Allocate();

	return bufferIndex;
}

void RenderInstance::AddVulkanMemoryBarrier(RecordingBufferObject* rcb, int* descriptorid, int descriptorcount)
{
	VKDevice* dev = vkInstance->GetLogicalDevice(deviceIndex);

	for (int i = 0; i < descriptorcount; i++)
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
					ShaderResourceImageBarrier* barrier = (ShaderResourceImageBarrier*)(imageBarrier + 1);
					ShaderResourceImageBarrier* barrier2 = &barrier[1];

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
						/*
						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier->dstResourceLayout), *imageBarrier->textureHandles, range);
						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							BEFORE,
							srcStage,
							dstStage
						);
						*/
					}


					srcAction = API::ConvertResourceActionToVulkanAccessFlags(barrier2->srcAction);
					dstAction = API::ConvertResourceActionToVulkanAccessFlags(barrier2->dstAction);
					srcStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier2->srcStage);
					dstStage = API::ConvertBarrierStageToVulkanPipelineStage(barrier2->dstStage);

					if (barrier2->dstResourceLayout != barrier2->srcResourceLayout)
					{

						/*
						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier2->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier2->dstResourceLayout), *imageBarrier->textureHandles, range);

						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							AFTER,
							srcStage,
							dstStage
						);
						*/
					}

					break;
				}
				case ShaderResourceType::SAMPLER2D:
				{
					ShaderResourceImage* imageBarrier = (ShaderResourceImage*)header;

					ShaderResourceImageBarrier* barrier = (ShaderResourceImageBarrier*)(imageBarrier + 1);

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
						/*
						EntryHandle barrierIndex = dev->CreateImageMemoryBarrier(srcAction, dstAction, 0, 0, API::ConvertImageLayoutToVulkanImageLayout(barrier->srcResourceLayout),
							API::ConvertImageLayoutToVulkanImageLayout(barrier->dstResourceLayout), *imageBarrier->textureHandles, range);
						vkPipelineObject->AddImageMemoryBarrier(barrierIndex,
							BEFORE,
							srcStage,
							dstStage
						);
						*/
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
						VkBufferMemoryBarrier vkBarrier{};

						VkBuffer buffer = dev->GetBufferHandle(bufferHandles[allocations[index].memIndex]);

						vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
						vkBarrier.pNext = nullptr;
						vkBarrier.srcAccessMask = srcAction;
						vkBarrier.dstAccessMask = dstAction;
						vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						vkBarrier.buffer = buffer;
						vkBarrier.offset = allocations[index].offset;
						vkBarrier.size = size;

						RBOPipelineBarrierArgs args{};

						args.srcStageMask = srcStage;
						args.dstStageMask = dstStage;
						args.bufferMemoryBarrierCount = 1;
						args.pBufferMemoryBarriers = &vkBarrier;

						rcb->BindPipelineBarrierCommand(&args);

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

						VkBufferMemoryBarrier vkBarrier{};

						VkBuffer buffer = dev->GetBufferHandle(bufferHandles[allocations[index].memIndex]);

						vkBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
						vkBarrier.pNext = nullptr;
						vkBarrier.srcAccessMask = srcAction;
						vkBarrier.dstAccessMask = dstAction;
						vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
						vkBarrier.buffer = buffer;
						vkBarrier.offset = allocations[index].offset + offset;
						vkBarrier.size = size;

						RBOPipelineBarrierArgs args{};

						args.srcStageMask = srcStage;
						args.dstStageMask = dstStage;
						args.bufferMemoryBarrierCount = 1;
						args.pBufferMemoryBarriers = &vkBarrier;

						rcb->BindPipelineBarrierCommand(&args);

					}
					break;
				}
				}
			}
		}
	}
}

