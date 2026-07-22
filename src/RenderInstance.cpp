#include "RenderInstance.h"

#include <memory>

#include <string.h>
#include <assert.h>

#include "FileManager.h"
#include "ShaderResourceSet.h"

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
		case ImageFormat::R32_UINT:
			vkFormat = VK_FORMAT_R32_UINT;
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

	VkAccessFlags ConvertBarrierActionToVulkanAccessFlags(BarrierAction action)
	{
		VkAccessFlags flags = 0;
		flags |= (VK_ACCESS_SHADER_WRITE_BIT) * ((action & WRITE_SHADER_RESOURCE) != 0);
		flags |= (VK_ACCESS_SHADER_READ_BIT) * ((action & READ_SHADER_RESOURCE) != 0);
		flags |= (VK_ACCESS_UNIFORM_READ_BIT) * ((action & READ_UNIFORM_BUFFER) != 0);
		flags |= (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) * ((action & READ_VERTEX_INPUT) != 0);
		flags |= (VK_ACCESS_INDIRECT_COMMAND_READ_BIT) * ((action & READ_INDIRECT_COMMAND) != 0);
		flags |= (VK_ACCESS_TRANSFER_WRITE_BIT) * ((action & TRANSFER_WRITE_DATA_RESOURCE) != 0);
		flags |= (VK_ACCESS_INDEX_READ_BIT) * ((action & READ_INDEX_INPUT) != 0);
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
		flags |= (VK_PIPELINE_STAGE_TRANSFER_BIT) * ((sourceStage & TRANSFER_BARRIER) != 0);
		flags |= (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) * ((sourceStage & INDEX_INPUT_BARRIER) != 0);
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
		case ImageLayout::TRANSFER_DEST_OPTIMAL:
			outLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;
		case ImageLayout::TRANSFER_SRC_OPTIMAL:
			outLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
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

		features2->features.logicOp = request->requireLogicOp ? VK_TRUE : VK_FALSE;
	}

	VkImageAspectFlags ConvertImageViewAspectMaskToVulkanImageAspectFlags(ImageViewAspectMask aspectMask)
	{
		return
			((aspectMask & COLOR_IMAGE_ASPECT) ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
			((aspectMask & DEPTH_IMAGE_ASPECT) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
			((aspectMask & STENCIL_IMAGE_ASPECT) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
	}

	VkImageType ConvertImageTypeToVulkanImageType(ImageType imageType)
	{
		VkImageType result = VK_IMAGE_TYPE_2D;

		switch (imageType)
		{
		case ImageType::IMAGE_1D:
			result = VK_IMAGE_TYPE_1D;
			break;

		case ImageType::IMAGE_2D:
			result = VK_IMAGE_TYPE_2D;
			break;

		case ImageType::IMAGE_3D:
			result = VK_IMAGE_TYPE_3D;
			break;

		case ImageType::IMAGE_CUBE:
			result = VK_IMAGE_TYPE_2D;
			break;

		default:
			result = VK_IMAGE_TYPE_2D;
			break;
		}

		return result;
	}

	VkImageViewType ConvertImageTypeToVulkanImageViewType(ImageType imageType)
	{
		VkImageViewType result = VK_IMAGE_VIEW_TYPE_2D;

		switch (imageType)
		{
		case ImageType::IMAGE_1D:
			result = VK_IMAGE_VIEW_TYPE_1D;
			break;

		case ImageType::IMAGE_2D:
			result = VK_IMAGE_VIEW_TYPE_2D;
			break;

		case ImageType::IMAGE_3D:
			result = VK_IMAGE_VIEW_TYPE_3D;
			break;

		case ImageType::IMAGE_CUBE:
			result = VK_IMAGE_VIEW_TYPE_CUBE;
			break;

		default:
			result = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}

		return result;
	}

	VkImageUsageFlags ConvertImageUsageFlagsToVulkanImageUsageFlags(ImageUsageFlags flags)
	{
		VkImageUsageFlags vkFlags = 0;

		vkFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT * ((flags & TRANSFER_SRC) != 0);
		vkFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT * ((flags & TRANSFER_DEST) != 0);
		vkFlags |= VK_IMAGE_USAGE_SAMPLED_BIT * ((flags & SAMPLED) != 0);
		vkFlags |= VK_IMAGE_USAGE_STORAGE_BIT * ((flags & STORAGE) != 0);
		vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT * ((flags & DEPTH_ATTACHMENT) != 0);
		vkFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT * ((flags & STENCIL_ATTACHMENT) != 0);
		vkFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT * ((flags & COLOR_ATTACHMENT) != 0);
		vkFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT * ((flags & TRANSIENT_ATTACHMENT) != 0);

		return vkFlags;
	}

	VkMemoryPropertyFlags ConvertMemoryTypeToVkMemoryPropertyFlags(MemoryType memType)
	{
		VkMemoryPropertyFlags retFlags = 0;
		retFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT * ((memType & MemoryTypeBits::DEVICE_MEMORY_TYPE) != 0);
		retFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT * (((memType & MemoryTypeBits::HOST_MEMORY_TYPE) != 0) || ((memType & MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE) != 0));
		retFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT * ((memType & MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE) != 0);
		return retFlags;
	}

	VkBlendFactor ConvertBlendFactorToVulkanBlendFactor(BlendFactor factor)
	{
		VkBlendFactor vkFactor = VK_BLEND_FACTOR_ZERO;

		switch (factor)
		{
		case BlendFactor::FACTOR_ZERO:
			vkFactor = VK_BLEND_FACTOR_ZERO;
			break;

		case BlendFactor::FACTOR_ONE:
			vkFactor = VK_BLEND_FACTOR_ONE;
			break;

		case BlendFactor::FACTOR_SRC_COLOR:
			vkFactor = VK_BLEND_FACTOR_SRC_COLOR;
			break;

		case BlendFactor::FACTOR_DST_COLOR:
			vkFactor = VK_BLEND_FACTOR_DST_COLOR;
			break;

		case BlendFactor::FACTOR_SRC_ALPHA:
			vkFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			break;

		case BlendFactor::FACTOR_DST_ALPHA:
			vkFactor = VK_BLEND_FACTOR_DST_ALPHA;
			break;

		case BlendFactor::FACTOR_ONE_MINUS_SRC_ALPHA:
			vkFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;

		case BlendFactor::FACTOR_ONE_MINUS_DST_ALPHA:
			vkFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		}

		return vkFactor;
	}

	VkBlendOp ConvertBlendOpToVulkanBlendOp(BlendOp op)
	{
		VkBlendOp vkOp = VK_BLEND_OP_ADD;

		switch (op)
		{
		case BlendOp::BLEND_ADD:
			vkOp = VK_BLEND_OP_ADD;
			break;

		case BlendOp::BLEND_SUB:
			vkOp = VK_BLEND_OP_SUBTRACT;
			break;

		case BlendOp::BLEND_REVERSE_SUB:
			vkOp = VK_BLEND_OP_REVERSE_SUBTRACT;
			break;

		case BlendOp::BLEND_MIN:
			vkOp = VK_BLEND_OP_MIN;
			break;

		case BlendOp::BLEND_MAX:
			vkOp = VK_BLEND_OP_MAX;
			break;
		}

		return vkOp;
	}

	VkLogicOp ConvertBlendLogicOpToVulkanLogicOp(BlendLogicOp op)
	{
		VkLogicOp vkOp = VK_LOGIC_OP_CLEAR;

		switch (op)
		{
		case BlendLogicOp::LOGIC_CLEAR:
			vkOp = VK_LOGIC_OP_CLEAR;
			break;

		case BlendLogicOp::LOGIC_AND:
			vkOp = VK_LOGIC_OP_AND;
			break;

		case BlendLogicOp::LOGIC_COPY:
			vkOp = VK_LOGIC_OP_COPY;
			break;
		}

		return vkOp;
	}
}

#define RENDER_MIN(a, b) ((a) > (b) ? (b) : (a))
#define RENDER_MAX(a, b) ((a) < (b) ? (b) : (a))

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		return VK_FALSE;
	}

	Logger* logger = (Logger*)pUserData;

	logger->AddLogMessage(LOGERROR, pCallbackData->pMessage, strlen(pCallbackData->pMessage));

	logger->ProcessMessage();

	return VK_FALSE;
}

#ifdef _MSC_VER
#include <intrin.h>
#endif

int findLSB(unsigned int input)
{
	if (!input) return -1;
#ifdef _MSC_VER
	unsigned long index;
	_BitScanForward(&index, input);
	return index;
#else
	return __builtin_ctz(input);
#endif
}

int findMSB(unsigned int input)
{
	if (!input) return -1;

#ifdef _MSC_VER
	unsigned long index;
	_BitScanReverse(&index, input);
	return index;
#else
	return 31 - __builtin_clz(input);
#endif
}

void RenderInstance::CreateRenderInstance(RenderInstanceCreateInfo* info, Allocator* instanceStorageAllocator, RingAllocator* instanceCacheAllocator)
{
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
	internalRendererLogger->fileHandle = info->internalRendererHandle;

	bufferHandles.Create(instanceStorageAllocator, info->maxBufferPoolsCount);

	imagePools.Create(storageAllocator, info->maxImagePoolsCount);

	pipelineInstancesIdentifier.Create(instanceStorageAllocator, info->maxPipelineInstances);

	pipelineHandles.Create(instanceStorageAllocator, info->maxPipelineHandles);

	gpuCommandStreams.Create(instanceStorageAllocator, info->maxGPUCommandsStreams);

	renderTargetQueues.Create(instanceStorageAllocator, info->maxRenderQueues);

	computeQueues.Create(instanceStorageAllocator, info->maxComputeQueues);

	samplerResourceHandles.Create(instanceStorageAllocator, info->maxSamplerHandles);

	textureResourceHandles.Create(instanceStorageAllocator, info->maxTextureHandles);

	textureViewsResourceHandles.Create(instanceStorageAllocator, info->maxTextureHandles);

	resourceStatuses.Create(instanceStorageAllocator, info->maxResourceStatuses);

	pipelineInfos.Create(instanceStorageAllocator, info->maxPipelineTemplates);

	mainRenderTargets.Create(instanceStorageAllocator, info->maxRenderTargets);

	renderPasses.Create(instanceStorageAllocator, info->maxRenderTargets);

	shaderResourceTemplates.Create(instanceStorageAllocator, info->maxShaderResourceTemplates);

	allocations.Create(instanceStorageAllocator, info->maxAllocations + info->maxSubAllocations);

	descriptorManagers.Create(instanceStorageAllocator, info->maxDescriptorManagers);

	shaderGraphs.Create(instanceStorageAllocator, info->maxShaderGraphs, info->maxShaderHandles);

	windowsSurfaces.Create(instanceStorageAllocator, info->maxWindows);

	swapChains.Create(instanceStorageAllocator, info->maxSwapChains);

	physicalDeviceIndices = (RenderPhysicalDeviceContainer*)storageAllocator->Allocate(sizeof(RenderPhysicalDeviceContainer) * info->maxGPUS, alignof(RenderPhysicalDeviceContainer));

	logicalDeviceIndices = (RenderLogicalDeviceContainer*)storageAllocator->Allocate(sizeof(RenderLogicalDeviceContainer) * info->maxLogicalDevices, alignof(RenderLogicalDeviceContainer));

	maxLogicalDevices = info->maxLogicalDevices;
	maxPhysicalDevices = info->maxGPUS;

	barriersQueue = (uint32_t*)storageAllocator->Allocate(sizeof(uint32_t) * info->maxConcurrentRecordings);

	barrierAccumulators = (BarrierAccumulator*)storageAllocator->Allocate(sizeof(BarrierAccumulator) * info->maxConcurrentRecordings);

	maxBarrierAccumulationCount = info->maxConcurrentRecordings;

	currentBarrierAccumulationTop = 0;

	for (uint32_t i = 0; i < info->maxConcurrentRecordings; i++)
	{
		CreateDriverSpecificBarrierArenas(&barrierAccumulators[i], info->maxTextureHandles, info->maxAllocations);
		barriersQueue[i] = i;
	}

	return;
}

#define MAX_MIPS_FOR_BARRIER 16
#define MAX_ARRAYS_FOR_BARRIER 8

void RenderInstance::CreateDriverSpecificBarrierArenas(BarrierAccumulator* barrierAccumulator, int maxTextures, int maxAllocations)
{
	barrierAccumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator = (SlabAllocator*)storageAllocator->Allocate(sizeof(SlabAllocator), alignof(SlabAllocator));
	barrierAccumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].allocator = (SlabAllocator*)storageAllocator->Allocate(sizeof(SlabAllocator), alignof(SlabAllocator));

	int imageSize = (sizeof(VkImageMemoryBarrier) * MAX_ARRAYS_FOR_BARRIER * MAX_MIPS_FOR_BARRIER * maxTextures) + sizeof(BarrierStage) * 2;

	int bufferSize = (sizeof(VkBufferMemoryBarrier) * maxAllocations) + sizeof(BarrierStage) * 2;

	barrierAccumulator->intraPassCount = 0;
	barrierAccumulator->intraPassTop = 0;

	std::construct_at(barrierAccumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator, storageAllocator->Allocate(imageSize, alignof(VkImageMemoryBarrier)), imageSize);
	std::construct_at(barrierAccumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].allocator, storageAllocator->Allocate(bufferSize, alignof(VkBufferMemoryBarrier)), bufferSize);
	std::construct_at(&barrierAccumulator->intraPassBarrierAllocator, storageAllocator->Allocate(12 * KiB, alignof(VkBufferMemoryBarrier)), 12 * KiB);

	for (int i = 0; i < MAX_INTRA_PASS_BARRIERS; i++)
	{
		barrierAccumulator->intraPassBarriers[i].pipelineInst = -1;
		barrierAccumulator->intraPassBarriers[i].barrierType = BarrierType::NULL_BARRIER;
		barrierAccumulator->intraPassBarriers[i].barrierCount = 0;
	}

	barrierAccumulator->intraPassCount = 0;
	barrierAccumulator->intraPassTop = 0;
	barrierAccumulator->intraPassBarrierAllocator.Reset();
}

uint32_t RenderInstance::PopBarrierAccumulator()
{
	if (currentBarrierAccumulationTop == maxBarrierAccumulationCount)
		return ~0;

	uint32_t barrierAccumIndex = barriersQueue[currentBarrierAccumulationTop++];

	BarrierAccumulator* barrierAccumulator = &barrierAccumulators[barrierAccumIndex];

	ResetIntraBarrierAccumulator(barrierAccumulator);

	return barrierAccumIndex;
}

void RenderInstance::ResetIntraBarrierAccumulator(BarrierAccumulator* accumulator)
{
	for (int i = 0; i < accumulator->intraPassCount; i++)
	{
		accumulator->intraPassBarriers[i].pipelineInst = -1;
		accumulator->intraPassBarriers[i].barrierType = BarrierType::NULL_BARRIER;
		accumulator->intraPassBarriers[i].barrierCount = 0;
	}

	accumulator->intraPassCount = 0;
	accumulator->intraPassTop = 0;
	accumulator->intraPassBarrierAllocator.Reset();
}

void RenderInstance::ReturnBarrierAccumulator(uint32_t returnIndex)
{
	if (currentBarrierAccumulationTop == 0)
		return;

	barriersQueue[--currentBarrierAccumulationTop] = returnIndex;
}

RenderInstance::~RenderInstance()
{
	if (vkInstance) vkInstance->~VKInstance();
};

void RenderInstance::DestoryTexture(int deviceSelection, EntryHandle handle)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	//dev->DestroyTexture(handle);
}

void RenderInstance::DestroySwapChainAttachments(int deviceSelection, EntryHandle swapChainIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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
					RenderTextureDescription* texDesc = textureResourceHandles.Get(inst->textureIds[sampIndex][d]);

					RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(texDesc->viewIndex[0]);

					dev->DestroyImage(texDesc->textureIndex);
					dev->DestroyImageView(imageViewDesc->viewIndex);

					resourceStatuses.Free(texDesc->resourceStatusIndex);

					textureResourceHandles.Free(inst->textureIds[sampIndex][d]);

					textureViewsResourceHandles.Free(texDesc->viewIndex[0]);
				}

				sampLo <<= 1;

				sampIndex++;
			}
		}
	}
}

int RenderInstance::RecreateSwapChain(int deviceSelection, int swapChainIndex, uint32_t width, uint32_t height)
{
	int ret = 0;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	RenderSwapchainData* data = swapChains.Get(swapChainIndex);

	if (width && height) 
	{
		VKSwapChain* swc = dev->GetSwapChain(data->swapChainIdx);
		
		swc->Wait();

		DestroySwapChainAttachments(deviceSelection, data->swapChainIdx);

		CreateSwapChainData(deviceSelection, data->swapChainIdx, width, height, true);

		for (uint32_t i = 0; i < swc->imageCount; i++)
		{
			RenderTextureDescription* desc = textureResourceHandles.Get(data->textureIds[i]);

			RenderImageViewDescription* viewDesc = textureViewsResourceHandles.Get(desc->viewIndex[0]);

			viewDesc->viewIndex = swc->imageViews[i];
		}

		data->width = width;

		data->height = height;

		ret = 1;
	}

	return ret;
}

int RenderInstance::CreateAttachmentGraphInstance(int deviceSelection, AttachmentGraph* graph)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

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

			int sampleCountHi = (resDesc->msaa ? (1 << (deviceContainer->relatedPhysDeviceInfo->maxMSAALevels)) : 1);

			sampHi = RENDER_MAX(sampleCountHi, sampHi);

			currResource->textureIds = nullptr;

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

	
		int renderPassSampleCount = RENDER_MAX(findMSB(sampHi), 1);

		rpInst->maxSampleCount = renderPassSampleCount;

		rpInst->baseRenderTargetData = totalRenderTargetsCreated;

		totalRenderTargetsCreated += renderPassSampleCount;
	}

	graphInstance->consecutiveRenderTargetsBase = mainRenderTargets.Allocate(totalRenderTargetsCreated);

	return attachmentInstanceIndex;
}

int RenderInstance::CreateRenderPass(int deviceSelection, AttachmentGraphInstance* graphInstance)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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

			sampLo = RENDER_MAX((int)vkSampleCountLo, sampLo);

			uint32_t vkRenderPassMappedIdx = 0;

			AttachmentResourceInstance* currResource = &graphInstance->resources[desc->resourceIndex];

			currResource->textureIds = nullptr;

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

uint32_t RenderInstance::BeginFrame(int deviceSelection, int swapChainIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	RenderSwapchainData* swcData = swapChains.Get(swapChainIndex);

	int32_t res = dev->CommandBufferWaitOn(UINT64_MAX, deviceContainer->currentCommandBufferIndex[currentFrame]);

	uint32_t imageIndex = dev->BeginFrameForSwapchain(swcData->swapChainIdx, swcData->rendererWaitSemaphores[currentFrame], currentFrame);

	if (imageIndex == ~0UL)
	{
		return imageIndex;
	}

	dev->CommandBufferResetFence(deviceContainer->currentCommandBufferIndex[currentFrame]);

	return imageIndex;
}

int RenderInstance::SubmitFrame(int deviceSelection, int swapChainIndex, uint32_t imageIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	RenderSwapchainData* swcData = swapChains.Get(swapChainIndex);

	VkPipelineStageFlags waitStages[2] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	int res = -1;
	
	if (deviceContainer->deviceTimelineSyncObject.driverTimelineObject == EntryHandle())
	{
		res = dev->SubmitCommandBuffer(&swcData->rendererWaitSemaphores[currentFrame], &waitStages[0], &swcData->rendererFinishedSemaphores[imageIndex], 1, 1, deviceContainer->currentCommandBufferIndex[currentFrame]);
	}
	else
	{
		uint64_t waitCount[2] = {0, deviceContainer->deviceTimelineSyncObject.currentValue};

		uint64_t signalCount[2] = {0, deviceContainer->deviceTimelineSyncObject.currentValue + 1};

		res = dev->SubmitCommandBuffer(
			&swcData->rendererWaitSemaphores[currentFrame], waitStages, 1,
			&deviceContainer->deviceTimelineSyncObject.driverTimelineObject, 1, waitCount,
			&swcData->rendererFinishedSemaphores[imageIndex], 1,
			&deviceContainer->deviceTimelineSyncObject.driverTimelineObject,
			1,
			signalCount,
			deviceContainer->currentCommandBufferIndex[currentFrame]
		);
		
		deviceContainer->deviceTimelineSyncObject.currentValue++;
	}

	if (res) return -1;

	if (deviceContainer->presentQueue != deviceContainer->graphicsComputeTransfer)
	{
		res = dev->PresentSwapChainSeparatePresentQueue(swcData->swapChainIdx, &swcData->rendererFinishedSemaphores[imageIndex], 1, imageIndex, currentFrame, deviceContainer->presentQueue);
	}
	else
	{
		res = dev->PresentSwapChainCommandBufferInline(swcData->swapChainIdx, &swcData->rendererFinishedSemaphores[imageIndex], 1, imageIndex, currentFrame, deviceContainer->currentCommandBufferIndex[currentFrame]);
	}

	if (res) 
	{
		dev->CommandBufferWaitOn(UINT64_MAX, deviceContainer->currentCommandBufferIndex[currentFrame]);
		return res;
	}

	return 0;
}

void RenderInstance::WaitOnRender(int deviceSelection)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	dev->WaitOnDevice();
}

int RenderInstance::CreateSwapChainAttachment(int deviceSelection, int swapChainIndex, int graphIndex, int renderPassIndex, AttachmentClear* clears, DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	RenderSwapchainData* swcData = swapChains.Get(swapChainIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swcData->swapChainIdx);

	EntryHandle* backBuffers = swapChain->imageViews;

	return CreateAttachmentResources(deviceSelection, graphIndex, renderPassIndex, swapChain->imageCount, backBuffers, swcData->textureIds, swcData->width, swcData->height, RenderPassType::SWAPCHAIN_IMAGE_COUNT, clears, rtvAllocator, dsvAllocator, rtvPoolIndex, dsvPoolIndex);
}

int RenderInstance::CreatePerFrameAttachment(int deviceSelection, int graphIndex, int renderPassIndex, int imageCount, uint32_t width, uint32_t height, AttachmentClear* clears, DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	return CreateAttachmentResources(deviceSelection, graphIndex, renderPassIndex, imageCount, nullptr, nullptr, width, height, RenderPassType::PER_FRAME_IMAGE_COUNT, clears, rtvAllocator, dsvAllocator, rtvPoolIndex, dsvPoolIndex);
}

int RenderInstance::CreateResourceStatusActions(ResourceStatus* status, int numberOfCurrentActions, int numberOfCurrentStages, int numberOfCurrentLayouts)
{
	status->currAction = (BarrierAction*)storageAllocator->Allocate(sizeof(BarrierAction) * numberOfCurrentActions);
	status->currStage = (BarrierStage*)storageAllocator->Allocate(sizeof(BarrierStage) * numberOfCurrentStages);
	status->currentLayout = (ImageLayout*)storageAllocator->Allocate(sizeof(ImageLayout) * numberOfCurrentStages);
	return 0;
}

void RenderInstance::InitializeResourceStatus(ResourceStatus* status, int numberOfCurrentActions, int numberOfCurrentStages, int numberOfCurrentLayouts, BarrierAction action, BarrierStage stage, ImageLayout imageLayout)
{
	for (int i = 0; i < numberOfCurrentActions; i++)
		status->currAction[i] = action;

	for (int i = 0; i < numberOfCurrentStages; i++)
		status->currStage[i] = stage;

	for (int i = 0; i < numberOfCurrentLayouts; i++)
		status->currentLayout[i] = imageLayout;
}

int RenderInstance::CreateAttachmentImage(
	uint32_t width, uint32_t height, 
	uint32_t arrayLayers, uint32_t mipCount,
	ImageType imageType, int sampleCount, 
	ImageFormat format, ImageUsageFlags usageFlags, 
	DeviceSlabAllocator* attachmentAllocator, ImageLayout initialLayout,
	VKDevice* dev, int imageMemoryPoolIndex, ResourceStatusType resourceType)
{

	VkFormat vkAttachmentFormat = API::ConvertImageFormatToVulkanFormat(format);

	VkImageType vkImageType = API::ConvertImageTypeToVulkanImageType(imageType);

	VkImageUsageFlags vkUsageFlags = API::ConvertImageUsageFlagsToVulkanImageUsageFlags(usageFlags);

	VkImageLayout vkInitialLayot = API::ConvertImageLayoutToVulkanImageLayout(initialLayout);

	size_t actualImageSize = 0, actualImageAlignment = 0;

	dev->GetImageMemorySizeAndAlignment(width, height,
		mipCount, vkAttachmentFormat, arrayLayers,
		vkUsageFlags,
		sampleCount,
		vkInitialLayot,
		VK_IMAGE_TILING_OPTIMAL, 0,
		vkImageType, &actualImageSize, &actualImageAlignment);

	size_t actualMemAddr = attachmentAllocator->Allocate(actualImageSize, actualImageAlignment);

	EntryHandle imageHandle = dev->CreateImage(
		width, height,
		mipCount, vkAttachmentFormat, arrayLayers,
		vkUsageFlags,
		sampleCount, actualMemAddr,
		vkInitialLayot,
		VK_IMAGE_TILING_OPTIMAL, 0,
		vkImageType, imagePools[imageMemoryPoolIndex].imagePoolHandle
	);

	int textureIndex = textureResourceHandles.Allocate();

	RenderTextureDescription* desc = textureResourceHandles.Get(textureIndex);

	int resourceStatus = desc->resourceStatusIndex = resourceStatuses.Allocate();

	desc->arrayLayers = arrayLayers;
	desc->mipLayers = mipCount;
	desc->imageHeight = height;
	desc->imageWidth = width;
	desc->format = format;
	desc->textureIndex = imageHandle;
	desc->imageType = imageType;
	desc->viewCount = 0;

	for (int i = 0; i < MAX_VIEWS_ATTACHED_TO_TEXTURE; i++)
		desc->viewIndex[i] = -1;

	ResourceStatus* status = resourceStatuses.Get(resourceStatus);

	uint32_t totalResourceCount = mipCount * arrayLayers;

	CreateResourceStatusActions(status, totalResourceCount, totalResourceCount, totalResourceCount);

	status->resourceType = resourceType;

	InitializeResourceStatus(status, totalResourceCount, totalResourceCount, totalResourceCount, 0, BEGINNING_OF_PIPE, initialLayout);

	return textureIndex;
}

int RenderInstance::CreateAttachmentImageView(int textureIndex, uint32_t firstMip, uint32_t mipCount, uint32_t firstArrayLayer, uint32_t arrayLayerCount, ImageViewAspectMask mask, ImageLayout desiredLayout, VKDevice* dev)
{
	RenderTextureDescription* desc = textureResourceHandles.Get(textureIndex);

	VkFormat vkAttachmentFormat = API::ConvertImageFormatToVulkanFormat(desc->format);

	VkImageViewType vkImageViewType = API::ConvertImageTypeToVulkanImageViewType(desc->imageType);

	VkImageAspectFlags aspectFlags = API::ConvertImageViewAspectMaskToVulkanImageAspectFlags(mask);

	if (desc->viewCount == MAX_VIEWS_ATTACHED_TO_TEXTURE)
		return -1;

	int texViewCount = desc->viewCount++;

	EntryHandle imageViewHandle = dev->CreateImageView(desc->textureIndex, firstMip, firstArrayLayer, mipCount, arrayLayerCount, vkAttachmentFormat, aspectFlags, vkImageViewType);

	int viewIndex = desc->viewIndex[texViewCount] = textureViewsResourceHandles.Allocate();

	RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(viewIndex);

	imageViewDesc->firstLayer = firstMip;
	imageViewDesc->firstMipLevel = firstArrayLayer;
	imageViewDesc->layerCount = arrayLayerCount;
	imageViewDesc->mipLevelCount = mipCount;
	imageViewDesc->mask = mask;
	imageViewDesc->viewIndex = imageViewHandle;
	imageViewDesc->desiredLayoutForView = desiredLayout;

	return texViewCount;
}

int RenderInstance::CreateAttachmentImageView(int deviceSelection, int attachmentGraphInstance, int attachmentResourceIndex, uint32_t firstMip, uint32_t mipCount, uint32_t firstArrayLayer, uint32_t arrayLayerCount, ImageViewAspectMask mask, ImageLayout desiredLayout)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(attachmentGraphInstance);

	AttachmentResourceInstance* resource = &graphInstance->resources[attachmentResourceIndex];

	int imageCount = resource->imageCount;

	int sampleCount = RENDER_MAX(findMSB(resource->sampHi), 1);

	int texViewIndex = -1;

	for (int currSampleCount = 0; currSampleCount < sampleCount; currSampleCount++)
	{
		for (int i = 0; i < imageCount; i++)
		{
			int textureHandle = resource->textureIds[currSampleCount][i];

			texViewIndex = CreateAttachmentImageView(textureHandle, firstMip, mipCount, firstArrayLayer, arrayLayerCount, mask, desiredLayout, dev);
		}
	}

	return texViewIndex;
}

int RenderInstance::CreateAttachmentResources(
	int deviceSelection,
	int graphIndex, int renderPassIndex, int imageCount, 
	EntryHandle* backBufferViews, int* backBufferTexturesIds, uint32_t width, uint32_t height, 
	RenderPassType rpType, AttachmentClear* clears,
	DeviceSlabAllocator* rtvAllocator, DeviceSlabAllocator* dsvAllocator, int rtvPoolIndex, int dsvPoolIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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

		int sampleCount = RENDER_MAX(findMSB(sampHi), 1);

		int imageWidth = width;
		int imageHeight = height;

		if (clears)
		{
			attachDesc->clear = clears[resourceIndex];
		}

		if (resourceTempl->viewType == AttachmentViewType::SWAPCHAIN)
		{
			resourceInst->textureIds = (int**)storageAllocator->Allocate(sizeof(int*) * 1);
			resourceInst->textureIds[0] = backBufferTexturesIds;
		}
		else
		{
			if (!resourceInst->textureIds)
			{
				resourceInst->textureIds = (int**)storageAllocator->Allocate(sizeof(int*) * sampleCount);

				for (int c = 0; c<sampleCount; c++)
				{ 
					resourceInst->textureIds[c] = (int*)storageAllocator->Allocate(sizeof(int) * imageCount);
				}
			}

			resourceInst->imageCount = imageCount;

			switch (resourceInst->usage)
			{

			case AttachmentResourceInstanceUsage::COLOR_ATTACHMENT_USAGE:

				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						int textureIndex = resourceInst->textureIds[v][g] = CreateAttachmentImage(imageWidth, imageHeight, 1, 1,
							ImageType::IMAGE_2D, sampLo, resourceTempl->format,
							ImageUsageFlagBits::COLOR_ATTACHMENT | ImageUsageFlagBits::SAMPLED, rtvAllocator, ImageLayout::UNDEFINED, dev, rtvPoolIndex, MANAGED_IMAGE_RESOURCE);

						CreateAttachmentImageView(textureIndex, 0, 1, 0, 1, COLOR_IMAGE_ASPECT, ImageLayout::COLORATTACHMENT, dev);
					}

					sampLo <<= 1;
				}
				break;
			case AttachmentResourceInstanceUsage::DEPTH_STENCIL_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						int textureIndex = resourceInst->textureIds[v][g] = CreateAttachmentImage(imageWidth, imageHeight, 1, 1,
							ImageType::IMAGE_2D, sampLo, resourceTempl->format,
							ImageUsageFlagBits::DEPTH_ATTACHMENT | ImageUsageFlagBits::STENCIL_ATTACHMENT, dsvAllocator, ImageLayout::UNDEFINED, dev, dsvPoolIndex, MANAGED_IMAGE_RESOURCE);

						CreateAttachmentImageView(textureIndex, 0, 1, 0, 1, DEPTH_IMAGE_ASPECT | STENCIL_IMAGE_ASPECT, ImageLayout::DEPTHSTENCILATTACHMENT, dev);
					}

					sampLo <<= 1;
				}
				break;
			case AttachmentResourceInstanceUsage::DEPTH_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						int textureIndex = resourceInst->textureIds[v][g] = CreateAttachmentImage(imageWidth, imageHeight, 1, 1,
							ImageType::IMAGE_2D, sampLo, resourceTempl->format,
							ImageUsageFlagBits::DEPTH_ATTACHMENT | ImageUsageFlagBits::SAMPLED, dsvAllocator, ImageLayout::UNDEFINED, dev, dsvPoolIndex, MANAGED_IMAGE_RESOURCE);

						CreateAttachmentImageView(textureIndex, 0, 1, 0, 1, DEPTH_IMAGE_ASPECT, ImageLayout::DEPTHATTACHMENT, dev);
					}
					sampLo <<= 1;
				}
				break;

			case AttachmentResourceInstanceUsage::STENCIL_ATTACHMENT_USAGE:
				for (int v = 0; v < sampleCount; v++)
				{
					for (int g = 0; g < imageCount; g++)
					{
						int textureIndex = resourceInst->textureIds[v][g] = CreateAttachmentImage(imageWidth, imageHeight, 1, 1,
							ImageType::IMAGE_2D, sampLo, resourceTempl->format,
							ImageUsageFlagBits::STENCIL_ATTACHMENT, dsvAllocator, ImageLayout::UNDEFINED, dev, dsvPoolIndex, MANAGED_IMAGE_RESOURCE);

						CreateAttachmentImageView(textureIndex, 0, 1, 0, 1, STENCIL_IMAGE_ASPECT, ImageLayout::STENCILATTACHMENT, dev);
					}
					sampLo <<= 1;

				}
				break;

			case AttachmentResourceInstanceUsage::RESOLVE_ATTACHMENT_USAGE:
				for (int g = 0; g < imageCount; g++)
				{
					int textureIndex = resourceInst->textureIds[0][g] = CreateAttachmentImage(imageWidth, imageHeight, 1, 1,
						ImageType::IMAGE_2D, sampLo, resourceTempl->format,
						ImageUsageFlagBits::COLOR_ATTACHMENT | ImageUsageFlagBits::SAMPLED, rtvAllocator, ImageLayout::UNDEFINED, dev, rtvPoolIndex, MANAGED_IMAGE_RESOURCE);

					CreateAttachmentImageView(textureIndex, 0, 1, 0, 1, COLOR_IMAGE_ASPECT, ImageLayout::COLORATTACHMENT, dev);
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

				if (resourceInst->sampHi == 1)
				{
					sampleIndex = 0;
				}

				int textureIndex = resourceInst->textureIds[sampleIndex][d];

				RenderTextureDescription* texDesc = textureResourceHandles.Get(textureIndex);

				RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(texDesc->viewIndex[0]);

				attachmentViews[e] = imageViewDesc->viewIndex;
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

void RenderInstance::CreateSwapChainData(int deviceSelection, EntryHandle swapChainIndex, uint32_t width, uint32_t height, bool recreate)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	VKSwapChain* swapChain = dev->GetSwapChain(swapChainIndex);

	if (recreate)
	{
		swapChain->ResetSwapChain();
		swapChain->RecreateSwapChain(width, height);
	}
	else
	{
		swapChain->CreateSwapChain(width, height, deviceContainer->graphicsComputeTransfer, deviceContainer->presentQueue);
	}

	swapChain->CreateSwapchainViews();
}

void RenderInstance::CreateShaderResourceMap(int deviceSelection, ShaderGraph* graph)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int ResourceSetCount = graph->resourceSetCount;

	DescriptorSetLayoutBuilder** descriptorBuilders = (DescriptorSetLayoutBuilder**)cacheAllocator->Allocate(sizeof(DescriptorSetLayoutBuilder*) * ResourceSetCount);

	for (int j = 0; j < ResourceSetCount; j++)
	{
		ShaderResourceSetTemplate* set = &graph->shaderResourceSetTemplates[j];
		descriptorBuilders[j] = dev->CreateDescriptorSetLayoutBuilder(set->bindingCount);
	}

	for (int j = 0; j < graph->resourceCount; j++)
	{
		ShaderResourceTemplate* resource = &graph->shaderResources[j];

		VkShaderStageFlags stageFlags = API::ConvertShaderStageToVulkanShaderStage(resource->stages);

		if (resource->type == ShaderResourceType::CONSTANT_BUFFER) {
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

void RenderInstance::CreateShaderGraphs(int deviceSelection, StringView* shaderGraphLayouts, int shaderGraphLayoutsCount)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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

		CreateShaderResourceMap(deviceSelection, shaderGraphs.shaderGraphPtrs.Get(i));

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

		uint64_t readCount = 0;

		if (FileManager::FileExists(&nameView)) {

			OSOpenFile(nameView.stringData, nameView.charCount, READ, &handle);

			shaderLength = handle.fileLength;

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength);

			OSReadFile(&handle, shaderLength, shaderData, &readCount);
		}
		else
		{
			nameView.charCount -= 4;

			OSOpenFile(nameView.stringData, nameView.charCount, READ, &handle);

			shaderData = (char*)cacheAllocator->CAllocate(shaderLength + 1);

			OSReadFile(&handle, shaderLength, shaderData, &readCount);

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

int RenderInstance::CreateGraphicRenderStateObject(int deviceSelection, int shaderGraphIndex, int pipelineDescriptionIndex, int* frameGraphAttachments, int* perFrameRenderPassSelection, int frameGraphCount)
{
	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);

	ShaderMap* map = &graph->shaderMaps[0];

	if (map->type != COMPUTESHADERSTAGE)
	{
		uint32_t pipelineVariationsCounter = 0;

		uint32_t totalPiplineVariations = 0;

		PipelineInstanceData* instData = &pipelineInstancesIdentifier.Get(shaderGraphIndex)->instanceData;

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

			pipelineVariationsCounter += CreatePipelineFromGraphAndSpec(deviceSelection,
				pipelineInfos.Get(pipelineDescriptionIndex), shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex), 
				pipelineHandles, pipelineVariationsCounter, 
				attachmentGraphsInstances.Get(frameGraphAttachments[i]), perFrameRenderPassSelection[i]
			);
		}
		
		pipelineInstancesIdentifier.pool[shaderGraphIndex].pipelineIndices = pipelineHandles;
	}

	return 0;
}

int RenderInstance::CreateComputePipelineStateObject(int deviceSelection, int shaderGraphIndex)
{
	ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);
	
	ShaderMap* map = &graph->shaderMaps[0];
	
	if (map->type == COMPUTESHADERSTAGE)
	{
		EntryHandle* pipelineID = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle));

		*pipelineID = CreateVulkanComputePipelineTemplate(deviceSelection, graph);

		pipelineInstancesIdentifier.pool[shaderGraphIndex].pipelineIndices = pipelineID;
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

int RenderInstance::CreatePipelineFromGraphAndSpec(int deviceSelection, GenericPipelineStateInfo* stateInfo, ShaderGraph* graph, EntryHandle* outHandles, uint32_t outHandlePointer, AttachmentGraphInstance* graphInstance, uint32_t graphRenderPassIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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
		ShaderResourceTemplate* resource = &graph->shaderResources[i];
		if (resource->type == ShaderResourceType::CONSTANT_BUFFER)
		{
			int rangeIndex = resource->rangeIndex;
			pushConstantsSizes[rangeIndex] += resource->size;

			shaderStages[rangeIndex] = API::ConvertShaderStageToVulkanShaderStage(resource->stages);
		}
	}

	VKGraphicsPipelineBuilder* pipelineBuilder = dev->CreateGraphicsPipelineBuilder(EntryHandle(), stateInfo->blendAttachmentCount, graph->resourceSetCount, 2, pushConstantRangeCount);

	uint32_t globalPushOffset = 0;

	for (int i = 0; i < pushConstantRangeCount; i++)
	{
		pipelineBuilder->AddPushConstantRange(globalPushOffset, pushConstantsSizes[i], shaderStages[i], i);
		globalPushOffset += pushConstantsSizes[i];
	}
	
	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	pipelineBuilder->CreateDynamicStateInfo(dynamicStates, 2);

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

	for (int i = 0; i < stateInfo->blendAttachmentCount; i++)
	{
		BlendAttachments* attach = &stateInfo->blendAttachments[i];

		pipelineBuilder->CreateColorBlendAttachment(i, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			attach->blendingEnabled,
			API::ConvertBlendFactorToVulkanBlendFactor(attach->srcColorFactor),
			API::ConvertBlendFactorToVulkanBlendFactor(attach->dstColorFactor),
			API::ConvertBlendFactorToVulkanBlendFactor(attach->srcAlphaFactor),
			API::ConvertBlendFactorToVulkanBlendFactor(attach->dstAlphaFactor),
			API::ConvertBlendOpToVulkanBlendOp(attach->colorOp),
			API::ConvertBlendOpToVulkanBlendOp(attach->alphaOp)
		);
	}

	pipelineBuilder->CreateColorBlending(stateInfo->blendEnable, API::ConvertBlendLogicOpToVulkanLogicOp(stateInfo->blendOp));

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

EntryHandle RenderInstance::CreateVulkanComputePipelineTemplate(int deviceSelection, ShaderGraph* graph)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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
		ShaderResourceTemplate* resource = &graph->shaderResources[g];

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

void RenderInstance::UploadHostTransfers(int deviceSelection)
{
	int memCount = driverHostMemoryUpdater.linkCount;

	if (!memCount) return;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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

		size_t rsize = 0, align = 0, intOffset = 0;

		int bufferHandle = -1;

		RenderAllocation* alloc = allocations.Get(handle);

		rsize = alloc->requestedSize;
		align = alloc->alignment;

		rsize *= alloc->structureCopies;

		rsize = (rsize + align - 1) & ~(align - 1);

		if (alloc->allocType == AllocationType::PERFRAME)
		{
			intOffset = (currentFrame * rsize) + alloc->offset + region.allocoffset;;
		}
		else if (alloc->allocType == AllocationType::STATIC)
		{
			intOffset = alloc->offset + region.allocoffset;
		}

		bufferHandle = alloc->memIndex;
		
		EntryHandle index = bufferHandles[bufferHandle].bufferHandle;

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

		previousMin = RENDER_MIN(intOffset, previousMin);
		previousMax = RENDER_MAX(intOffset + rsize, previousMax);
	}

	dev->WriteToHostBufferBatch(previousBuffer, batchAddresses, batchSizes, batchOffsets, previousMax - previousMin, previousMin, batchCounter);
}

void RenderInstance::UploadDescriptorsUpdates(int deviceSelection)
{
	int memCount = descriptorUpdatePool.linkCount;

	if (!memCount) return;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int link = descriptorUpdatePool.linkHead;
	int* linkprev = &descriptorUpdatePool.linkHead;
	ShaderResourceUpdate region;

	EntryHandle previousBuffer = EntryHandle();

	DescriptorSetBuilder* builder = nullptr;

	ShaderResourceSet* set = nullptr;

	while (link >= 0)
	{
		link = descriptorUpdatePool.PopLink(&region, link, &linkprev);

		ShaderResourceManager* manager = descriptorManagers.Get(region.descriptorManagerIndex);

		EntryHandle index = manager->descriptorSetHandles[region.descriptorSet];

		void* data = region.data;

		if (index != previousBuffer)
		{
			builder = dev->UpdateDescriptorSet(index);
			
			previousBuffer = index;

			set = manager->descriptorSets[region.descriptorSet];
		}

		if (!set)
			continue;

		switch (region.type)
		{
		case ShaderResourceType::SAMPLERSTATE:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			ShaderResourceSampler* samplerHeader = (ShaderResourceSampler*)&set->resourceBindings[region.bindingIndex].resourceArray.samplers;

			assert(update->updateType == DeviceHandleArrayUpdateType::SAMPLER_UPDATE);

			int* samplerHandlesFromUpdate = (int*)update->resourceHandles;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				builder->AddSamplerDescription(samplerResourceHandles[samplerHandlesFromUpdate[iter]], update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);

				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					samplerHeader->samplerHandles[iter + update->resourceDstBegin] = samplerHandlesFromUpdate[iter];
					samplerHeader->samplerCount = RENDER_MAX(samplerHeader->samplerCount, (iter + update->resourceDstBegin) + 1);
				}

			}
			break;
		}
		case ShaderResourceType::IMAGE2D:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			ShaderResourceImage* imageResource = &set->resourceBindings[region.bindingIndex].resourceArray.images;

			assert(update->updateType == DeviceHandleArrayUpdateType::TEXTURE_VIEW_UPDATE);

			DeviceHandleArrayUpdateTextureView* texViews = (DeviceHandleArrayUpdateTextureView*)update->resourceHandles;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				RenderTextureDescription* desc = textureResourceHandles.Get(texViews[iter].imageHandle);

				RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(desc->viewIndex[texViews[iter].viewIndex]);

				builder->AddImageResourceDescription(imageViewDesc->viewIndex, API::ConvertImageLayoutToVulkanImageLayout(imageViewDesc->desiredLayoutForView), update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);

				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					imageResource->textureDetails[iter + update->resourceDstBegin].textureHandle = texViews[iter].imageHandle;
					imageResource->textureDetails[iter + update->resourceDstBegin].viewIndex = texViews[iter].viewIndex;
					imageResource->textureCount = RENDER_MAX(imageResource->textureCount, (iter + update->resourceDstBegin) + 1);
				}
			}
			break;
		}
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLER2D:
		case ShaderResourceType::SAMPLERCUBE:
		{
			DeviceHandleArrayUpdate* update = (DeviceHandleArrayUpdate*)region.data;

			ShaderResourceCombinedImage* imageResource = &set->resourceBindings[region.bindingIndex].resourceArray.combinedImages;

			assert(update->updateType == DeviceHandleArrayUpdateType::TEXTURE_VIEW_SAMPLER_UPDATE);

			DeviceHandleArrayUpdateTextureViewSampler* texViews = (DeviceHandleArrayUpdateTextureViewSampler*)update->resourceHandles;

			for (int iter = 0; iter < update->resourceCount; iter++)
			{
				RenderTextureDescription* desc = textureResourceHandles.Get(texViews[iter].imageHandle);

				RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(texViews[iter].viewIndex);

				EntryHandle samplerHandle = samplerResourceHandles[texViews[iter].samplerHandle];

				builder->AddCombinedTextureArray(imageViewDesc->viewIndex, samplerHandle, API::ConvertImageLayoutToVulkanImageLayout(imageViewDesc->desiredLayoutForView), update->resourceDstBegin + iter, region.bindingIndex, currentFrame, 1);

				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					imageResource->textureDetails[iter + update->resourceDstBegin].textureHandle = texViews[iter].imageHandle;
					imageResource->textureDetails[iter + update->resourceDstBegin].viewIndex = texViews[iter].viewIndex;
					imageResource->textureDetails[iter + update->resourceDstBegin].samplerHandle = texViews[iter].samplerHandle;
					imageResource->textureCount = RENDER_MAX(imageResource->textureCount, (iter + update->resourceDstBegin) + 1);
				}
			}
			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;

			ShaderResourceBuffer* bufferResource = (ShaderResourceBuffer*)&set->resourceBindings[region.bindingIndex].resourceArray.buffers;
	
			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				auto alloc = allocations[update->allocationIndices[j]];

				if (alloc.allocType == AllocationType::PERFRAME)
					builder->AddStorageBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex].bufferHandle), alloc.deviceAllocSize / MAX_FRAMES_IN_FLIGHT, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
				else
					builder->AddStorageBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex].bufferHandle), alloc.deviceAllocSize, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);

				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					bufferResource->allocationIndex[j + update->resourceDstBegin] = update->allocationIndices[j];
					bufferResource->bufferCount = RENDER_MAX(bufferResource->bufferCount, (j + update->resourceDstBegin) + 1);
				}
			}
			break;
		}
		case ShaderResourceType::UNIFORM_BUFFER:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;
			
			ShaderResourceBuffer* bufferResource = (ShaderResourceBuffer*)&set->resourceBindings[region.bindingIndex].resourceArray.buffers;

			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				auto alloc = allocations[update->allocationIndices[j]];

				if (alloc.allocType == AllocationType::PERFRAME)
					builder->AddUniformBuffer(dev->GetBufferHandle(bufferHandles[alloc.memIndex].bufferHandle), alloc.deviceAllocSize / MAX_FRAMES_IN_FLIGHT, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
				else
					builder->AddUniformBufferDirect(dev->GetBufferHandle(bufferHandles[alloc.memIndex].bufferHandle), alloc.deviceAllocSize, region.bindingIndex, 1, alloc.offset, currentFrame, firstBuffer + j);
			
				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					bufferResource->allocationIndex[j + update->resourceDstBegin] = update->allocationIndices[j];
					bufferResource->bufferCount = RENDER_MAX(bufferResource->bufferCount, (j + update->resourceDstBegin) + 1);
				}
			}
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			BufferArrayUpdate* update = (BufferArrayUpdate*)region.data;
			
			ShaderResourceBuffer* bufferResource = (ShaderResourceBuffer*)&set->resourceBindings[region.bindingIndex].resourceArray.views;

			int arrayCount = update->allocationCount;
			int firstBuffer = update->resourceDstBegin;

			for (int j = 0; j < arrayCount; j++)
			{
				RenderAllocation* alloc = allocations.Get(update->allocationIndices[j]);

				int viewGrab = (alloc->allocType == AllocationType::PERFRAME) ? currentFrame : 0;

				VkBufferView handle = dev->GetBufferView(alloc->viewIndex, viewGrab);

				ShaderResourceHeader* resource = (ShaderResourceHeader*)&set->resourceBindings[region.bindingIndex];

				if (resource->action == ShaderResourceAction::SHADERREAD)
				{
					builder->AddUniformBufferView(handle, region.bindingIndex, currentFrame, 1, j + firstBuffer);
				}
				else if (resource->action == ShaderResourceAction::SHADERWRITE)
				{
					builder->AddStorageBufferView(handle, region.bindingIndex, currentFrame, 1, j + firstBuffer);
				}
				
				if (region.copyCount == (MAX_FRAMES_IN_FLIGHT))
				{
					bufferResource->allocationIndex[j + update->resourceDstBegin] = update->allocationIndices[j];
					bufferResource->bufferCount = RENDER_MAX(bufferResource->bufferCount, (j + update->resourceDstBegin) + 1);
				}
			}
			break;
		}
		}
	}
}

void RenderInstance::UploadImageMemoryTransfers(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum)
{
	int memCount = imageMemoryUpdateManager.linkCount;

	if (!memCount) return;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int link = imageMemoryUpdateManager.linkHead;

	DeviceSlabAllocator* stagingAlloc = &deviceContainer->stagingBufferAllocators[currentFrame];

	TextureMemoryRegion* regions = (TextureMemoryRegion*)cacheAllocator->Allocate(sizeof(TextureMemoryRegion) * memCount, alignof(TextureMemoryRegion));

	int regionCount = 0;

	while (link >= 0)
	{
		link = imageMemoryUpdateManager.PopLink(&regions[regionCount++], link);
	}

	for (int i = 0; i < regionCount; i++)
	{

		TextureMemoryRegion* region = &regions[i];

		RenderTextureDescription* desc = textureResourceHandles.Get(region->textureIndex);

		EntryHandle handle = desc->textureIndex;

		ResourceStatus* resourceStatus = resourceStatuses.Get(textureResourceHandles[region->textureIndex].resourceStatusIndex);

		TransitionImageLayout(dev, rbo, handle, region->mipStart, region->mipLevels,
			desc->mipLayers, region->layerStart, region->layerCount,
			region->transferMask, ImageLayout::TRANSFER_DEST_OPTIMAL,
			resourceStatus, TRANSFER_BARRIER, TRANSFER_WRITE_DATA_RESOURCE, accum, -1);

	}

	InsertAccumulatedBarriers(rbo, accum);

	for (int i = 0; i < regionCount; i++)
	{
		TextureMemoryRegion* region = &regions[i];

		RenderTextureDescription* desc = textureResourceHandles.Get(region->textureIndex);

		EntryHandle handle = desc->textureIndex;

		size_t currentImageOffsetInUploadArena = stagingAlloc->Allocate(region->totalSize, deviceContainer->relatedPhysDeviceInfo->optimalImageCopyOffsetAlignment);

		dev->UploadImageData(
			handle,
			(char*)region->data,
			region->totalSize,
			deviceContainer->stagingBuffers[currentFrame],
			region->width,
			region->height,
			region->mipLevels,
			region->layerCount,
			API::ConvertImageFormatToVulkanFormat(desc->format),
			API::ConvertImageViewAspectMaskToVulkanImageAspectFlags(region->transferMask),
			currentImageOffsetInUploadArena,
			rbo
		);
	}

	imageMemoryUpdateManager.ddsRegionAlloc = 0;
	imageMemoryUpdateManager.linkHead = -1;
}


void RenderInstance::UploadDeviceLocalTransfers(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum)
{
	int memCount = driverDeviceMemoryUpdater.linkCount;

	if (!memCount) return;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

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

	DeviceSlabAllocator* stagingAlloc = &deviceContainer->stagingBufferAllocators[currentFrame];

	while (link >= 0)
	{
		link = driverDeviceMemoryUpdater.PopLink(&region, link, &linkprev);

		int handle = region.allocationIndex;

		size_t intSize = region.size;

		size_t rsize = 0, align = 0, intOffset = 0;

		int bufferHandle = -1, resourceStatusIndex = 0;

		RenderAllocation* alloc = allocations.Get(handle);

		rsize = alloc->requestedSize;
		align = alloc->alignment;

		rsize *= alloc->structureCopies;

		rsize = (rsize + align - 1) & ~(align - 1);

		if (alloc->allocType == AllocationType::PERFRAME)
		{
			intOffset = (currentFrame * rsize) + alloc->offset + region.allocoffset;
			resourceStatusIndex = currentFrame;
		}
		else if (alloc->allocType == AllocationType::STATIC)
		{
			intOffset = alloc->offset + region.allocoffset;
		}

		bufferHandle = alloc->memIndex;

		EntryHandle index = bufferHandles[bufferHandle].bufferHandle;

		InsertBufferBarrier(dev, rbo, handle, BarrierStageBits::TRANSFER_BARRIER, BarrierActionBits::TRANSFER_WRITE_DATA_RESOURCE, accum);
	
		if (index != previousBuffer)
		{
			if (previousBuffer != EntryHandle())
			{
				InsertAccumulatedBarriers(rbo, accum);
				cumulativeSize = (uploadArenaOffset[batchCounter - 1] - uploadArenaOffset[0]) + batchSizes[batchCounter - 1];
				dev->WriteToDeviceBufferBatch(previousBuffer, deviceContainer->stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, uploadArenaOffset, batchCounter, rbo);
			}

			previousBuffer = index;
			batchCounter = 0;
			cumulativeSize = 0;
		}

		uploadArenaOffset[batchCounter] = stagingAlloc->Allocate(region.size, 64);
		batchSizes[batchCounter] = region.size;
		batchData[batchCounter] = region.data;
		batchOffsets[batchCounter] = intOffset;

		batchCounter++;
	}

	InsertAccumulatedBarriers(rbo, accum);

	cumulativeSize = (uploadArenaOffset[batchCounter - 1] - uploadArenaOffset[0]) + batchSizes[batchCounter - 1];

	dev->WriteToDeviceBufferBatch(previousBuffer, deviceContainer->stagingBuffers[currentFrame], batchData, batchSizes, batchOffsets, cumulativeSize, uploadArenaOffset, batchCounter, rbo);
}

void RenderInstance::InvokeTransferCommands(int deviceSelection, RecordingBufferObject* rbo, BarrierAccumulator* accum)
{
	int memCount = transferCommandPool.linkCount;

	if (!memCount) return;

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);
	
	TransferCommand region;
	int link = transferCommandPool.linkHead;
	int* linkprev = &transferCommandPool.linkHead;

	while (link >= 0)
	{
		link = transferCommandPool.PopLink(&region, link, &linkprev);

		int handle = region.allocationIndex;

		size_t intSize = region.size;

		size_t rsize = 0, align = 0, intOffset = 0;

		int bufferHandle = -1, resourceStatusIndex = -1;

		RenderAllocation* alloc = allocations.Get(handle);

		rsize = alloc->requestedSize;
		align = alloc->alignment;

		rsize *= alloc->structureCopies;

		rsize = (rsize + align - 1) & ~(align - 1);

		int resourceIndex = 0;

		if (alloc->allocType == AllocationType::PERFRAME)
		{
			intOffset = (currentFrame * rsize) + alloc->offset;
			resourceIndex = currentFrame;
		}
		else if (alloc->allocType == AllocationType::STATIC)
		{
			intOffset = alloc->offset;
		}

		bufferHandle = alloc->memIndex;

		resourceStatusIndex = alloc->resourceStatus;
		
		ResourceStatus* status = resourceStatuses.Get(resourceStatusIndex);

		EntryHandle index = bufferHandles[bufferHandle].bufferHandle;

		rbo->FillBuffer(index, intSize, intOffset, region.fillVal);

		status->currAction[resourceIndex] = TRANSFER_WRITE_DATA_RESOURCE;
		status->currStage[resourceIndex] = TRANSFER_BARRIER;
	}
}

int RenderInstance::GetAllocFromBuffer(int deviceSelection, int bufferHandle, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	switch (bufferAlignmentType)
	{
	case BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT:
		alignment = (alignment + deviceContainer->relatedPhysDeviceInfo->minUniformAlignment - 1) & ~((size_t)deviceContainer->relatedPhysDeviceInfo->minUniformAlignment - 1);
		break;
	case BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT:
		alignment = (alignment + deviceContainer->relatedPhysDeviceInfo->minStorageAlignment - 1) & ~((size_t)deviceContainer->relatedPhysDeviceInfo->minStorageAlignment - 1);
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
	alloc->parentAllocation = -1;

	if (formatType != ComponentFormatType::NO_BUFFER_FORMAT && formatType != ComponentFormatType::RAW_8BIT_BUFFER)
	{
		alloc->viewIndex = dev->CreateBufferView(bufferHandles[bufferHandle].bufferHandle, API::ConvertComponentFormatTypeToVulkanFormat(formatType), allocSize, location, copies);
	}

	int resourceIndex = alloc->resourceStatus = resourceStatuses.Allocate();

	ResourceStatus* resourceStatus = resourceStatuses.Get(resourceIndex);

	resourceStatus->resourceType = BUFFER_RESOURCE;

	CreateResourceStatusActions(resourceStatus, copies, copies, 0);

	InitializeResourceStatus(resourceStatus, copies, copies, 0, 0, BEGINNING_OF_PIPE, ImageLayout::UNDEFINED);

	return index;
}

int RenderInstance::CreateSuballocation(int deviceSelection, int parentAllocation, size_t structureSize, size_t copiesOfStructure, size_t alignment, AllocationType allocType, ComponentFormatType formatType, BufferAlignmentType bufferAlignmentType, DeviceSlabAllocator* allocator)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	switch (bufferAlignmentType)
	{
	case BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT:
		alignment = (alignment + deviceContainer->relatedPhysDeviceInfo->minUniformAlignment - 1) & ~((size_t)deviceContainer->relatedPhysDeviceInfo->minUniformAlignment - 1);
		break;
	case BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT:
		alignment = (alignment + deviceContainer->relatedPhysDeviceInfo->minStorageAlignment - 1) & ~((size_t)deviceContainer->relatedPhysDeviceInfo->minStorageAlignment - 1);
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

	RenderAllocation* alloc = allocations.Get(parentAllocation);

	size_t location = allocator->Allocate(allocSize * copies, alignment);

	int index = allocations.Allocate();

	RenderAllocation* subAlloc = allocations.Get(index);

	subAlloc->parentAllocation = parentAllocation;
	subAlloc->offset = location + alloc->offset;
	subAlloc->deviceAllocSize = allocSize * copies;
	subAlloc->requestedSize = structureSize;
	subAlloc->alignment = alignment;
	subAlloc->allocType = allocType;
	subAlloc->formatType = formatType;
	subAlloc->structureCopies = copiesOfStructure;


	if (formatType != ComponentFormatType::NO_BUFFER_FORMAT && formatType != ComponentFormatType::RAW_8BIT_BUFFER)
	{
		subAlloc->viewIndex = dev->CreateBufferView(bufferHandles[alloc->memIndex].bufferHandle, API::ConvertComponentFormatTypeToVulkanFormat(formatType), allocSize, alloc->offset + location, copies);
	}

	int resourceIndex = subAlloc->resourceStatus = resourceStatuses.Allocate();

	ResourceStatus* resourceStatus = resourceStatuses.Get(resourceIndex);

	resourceStatus->resourceType = BUFFER_RESOURCE;

	CreateResourceStatusActions(resourceStatus, copies, copies, 0);

	InitializeResourceStatus(resourceStatus, copies, copies, 0, 0, BEGINNING_OF_PIPE, ImageLayout::UNDEFINED);

	return index;
}

int RenderInstance::CreateImageHandle(
	int deviceSelection,
	size_t gpuMemAddress,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, uint32_t arrayLayers, ImageFormat format, ImageType imageType, ImageUsageFlags usageFlags, int poolIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	VkFormat actualFormat = API::ConvertImageFormatToVulkanFormat(format);

	EntryHandle textureHandle = dev->CreateImage(
		width, 
		height, 
		mipLevels, 
		actualFormat, 
		arrayLayers,
	    API::ConvertImageUsageFlagsToVulkanImageUsageFlags(usageFlags),
		1, 
		gpuMemAddress, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_TILING_OPTIMAL, 
		(imageType == ImageType::IMAGE_CUBE) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0,
		API::ConvertImageTypeToVulkanImageType(imageType),
		imagePools[poolIndex].imagePoolHandle
	);

	int textureIndex = textureResourceHandles.Allocate();

	RenderTextureDescription* renderTexDesc = textureResourceHandles.Get(textureIndex);

	renderTexDesc->arrayLayers = arrayLayers;
	renderTexDesc->mipLayers = mipLevels;
	renderTexDesc->imageWidth = width;
	renderTexDesc->imageHeight = height;
	renderTexDesc->format = format;
	renderTexDesc->textureIndex = textureHandle;
	renderTexDesc->imageType = imageType;
	renderTexDesc->viewCount = 0;
	
	for (int i = 0; i < MAX_VIEWS_ATTACHED_TO_TEXTURE; i++)
	{
		renderTexDesc->viewIndex[i] = -1;
	}

	int resourceIndex = renderTexDesc->resourceStatusIndex = resourceStatuses.Allocate();

	ResourceStatus* textureStatus = resourceStatuses.Get(resourceIndex);

	int totalTrackingLayers = mipLevels * arrayLayers;

	CreateResourceStatusActions(textureStatus, totalTrackingLayers, totalTrackingLayers, totalTrackingLayers);

	InitializeResourceStatus(textureStatus, totalTrackingLayers, totalTrackingLayers, totalTrackingLayers, 0, BEGINNING_OF_PIPE, ImageLayout::UNDEFINED);

	return textureIndex;
}

int RenderInstance::CreateImageView(int deviceSelection, int imageHandle, int firstMip, int mipCount, int firstLayer, int layerCount, ImageViewAspectMask imageAspect, ImageLayout desiredImageLayoutUsage)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int viewIndex = textureViewsResourceHandles.Allocate();

	RenderTextureDescription* renderTexDesc = textureResourceHandles.Get(imageHandle);

	renderTexDesc->viewIndex[renderTexDesc->viewCount] = viewIndex;
	
	int retIndex = renderTexDesc->viewCount++;

	RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(viewIndex);

	imageViewDesc->firstLayer = firstLayer;
	imageViewDesc->firstMipLevel = firstMip;
	imageViewDesc->mask = imageAspect;
	imageViewDesc->layerCount = ((layerCount == IMAGE_VIEW_ALL_LAYERS) ? renderTexDesc->arrayLayers : layerCount);
	imageViewDesc->mipLevelCount = ((mipCount == IMAGE_VIEW_ALL_MIPS) ? renderTexDesc->mipLayers : mipCount);
	
	imageViewDesc->desiredLayoutForView = desiredImageLayoutUsage;

	EntryHandle viewHandle = dev->CreateImageView(renderTexDesc->textureIndex, 
		imageViewDesc->firstMipLevel, imageViewDesc->firstLayer, 
		imageViewDesc->mipLevelCount, imageViewDesc->layerCount, 
		API::ConvertImageFormatToVulkanFormat(renderTexDesc->format), 
		API::ConvertImageViewAspectMaskToVulkanImageAspectFlags(imageAspect),
		API::ConvertImageTypeToVulkanImageViewType(renderTexDesc->imageType)
	);

	imageViewDesc->viewIndex = viewHandle;

	return retIndex;
}

void RenderInstance::GetGPURequestedImageSizeAndAlignment(int deviceSelection, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers, ImageFormat type, ImageUsageFlags usageFlags, size_t* actualImageSize, size_t* actualAlignment)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	VkFormat actualFormat = API::ConvertImageFormatToVulkanFormat(type);

	dev->GetImageMemorySizeAndAlignment(
		width, height,
		mipLevels, actualFormat, layers,
		API::ConvertImageUsageFlagsToVulkanImageUsageFlags(usageFlags),
		1,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D,
		actualImageSize, actualAlignment
	);
}

int RenderInstance::CreateImagePool(int deviceSelection, size_t size, ImageFormat format, int maxWidth, int maxHeight, ImageUsageFlags usageFlags, MemoryType memType)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	VkFormat vkFormat = API::ConvertImageFormatToVulkanFormat(format);
	VkImageUsageFlags vkUsageFlags = API::ConvertImageUsageFlagsToVulkanImageUsageFlags(usageFlags);
	VkMemoryPropertyFlags vkMemPropertyFlags = API::ConvertMemoryTypeToVkMemoryPropertyFlags(memType);

	std::pair<int, VkDeviceSize> poolInfo = dev->FindImageMemoryIndexForPool(maxWidth, maxHeight,
		1, vkFormat, 1,
		vkUsageFlags,
		1, vkMemPropertyFlags);

	int poolIndex = imagePools.Allocate();

	ImagePoolDescription* poolDesc = imagePools.Get(poolIndex);

	poolDesc->imagePoolHandle = dev->CreateImageMemoryPool(size, poolInfo.first);
	poolDesc->imagePoolSize = size;
	poolDesc->imagePoolType = memType;

	return poolIndex;
}

ShaderResourceSetBuilder RenderInstance::AllocateShaderResourceSet(int descriptorManagerIndex, int shaderGraphIndex, int targetSet, int setCount)
{ 
	ShaderResourceManager* manager = descriptorManagers.Get(descriptorManagerIndex);

    ShaderResourceSet* set = (ShaderResourceSet*)storageAllocator->Allocate(sizeof(ShaderResourceSet));
   
    ShaderGraph* graph = shaderGraphs.shaderGraphPtrs.Get(shaderGraphIndex);

    ShaderResourceSetTemplate* resourceSet = &graph->shaderResourceSetTemplates[targetSet];

    set->setCount = setCount;
	set->templateMetaData = resourceSet;

	int constantIndex = 0, resourceViewsBinding = 0;

	for (int h = 0; h < resourceSet->totalResourceCount; h++)
	{
		ShaderResourceTemplate* resource = &graph->shaderResources[resourceSet->resourceStart+h];

		if (resource->set != targetSet) continue;

		ShaderResourceHeader* desc = (ShaderResourceHeader*)&set->resourceBindings[resourceViewsBinding];

		ShaderResourceArray* descArray = (ShaderResourceArray*)desc;

		int actualRequestedArraySize = (DESCRIPTOR_COUNT_MASK & resource->arrayCount);

		switch (resource->type)
		{
		case ShaderResourceType::SAMPLERSTATE:
		{
			descArray->resourceArray.samplers.samplerHandles = (int*)storageAllocator->Allocate(sizeof(int) * actualRequestedArraySize, alignof(int));
			descArray->resourceArray.samplers.samplerCount = 0;
			resourceViewsBinding++;
			break;
		}
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		{
			descArray->resourceArray.images.textureDetails = (ShaderResourceImageContainer*)storageAllocator->Allocate(sizeof(ShaderResourceImageContainer) * actualRequestedArraySize, alignof(ShaderResourceImageContainer));
			descArray->resourceArray.images.textureCount = 0;
			resourceViewsBinding++;
			break;
		}
		case ShaderResourceType::SAMPLER3D:
		case ShaderResourceType::SAMPLER2D:
		case ShaderResourceType::SAMPLERCUBE:
		{
			descArray->resourceArray.combinedImages.textureDetails = (ShaderResourceCombinedImageContainer*)storageAllocator->Allocate(
				sizeof(ShaderResourceCombinedImageContainer) * actualRequestedArraySize, alignof(ShaderResourceCombinedImageContainer));
			descArray->resourceArray.combinedImages.textureCount = 0;
			resourceViewsBinding++;
			break;
		}
		case ShaderResourceType::CONSTANT_BUFFER:
		{
			ShaderResourceConstantBuffer* constants = &set->constantBuffers[constantIndex++];

			desc = (ShaderResourceHeader*)constants;

			constants->size = resource->size;
			constants->offset = resource->offset;
			constants->stage = resource->stages;
			constants->rangeindex = resource->rangeIndex;

			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:
		{
			descArray->resourceArray.buffers.allocationIndex = (int*)storageAllocator->Allocate(sizeof(int) * actualRequestedArraySize, alignof(int));
			descArray->resourceArray.buffers.bufferCount = 0;
			resourceViewsBinding++;
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			descArray->resourceArray.views.bufferCount = 0;
			descArray->resourceArray.views.allocationIndex = (int*)storageAllocator->Allocate(sizeof(int) * actualRequestedArraySize, alignof(int));
			resourceViewsBinding++;
			break;
		}
		}

		desc->binding = resource->binding;
		desc->type = resource->type;
		desc->action = resource->action;
		desc->arrayCount = resource->arrayCount;
		desc->stage = resource->stages;
    }

	int descriptorSetIndex = manager->AddShaderToSets(set);

	return { descriptorManagerIndex, descriptorSetIndex, set };
}

int RenderInstance::CreateAttachmentGraph(int deviceSelection, StringView* attachmentLayout, int* subAttachCount)
{
	int attachmentGraphTemplateIndex = attachmentGraphs.Allocate();

	AttachmentGraph* graph = attachmentGraphs.Get(attachmentGraphTemplateIndex);

	CreateAttachmentGraphFromFile(*attachmentLayout, graph, cacheAllocator, internalRendererLogger);

	int currentGraphInstance = CreateAttachmentGraphInstance(deviceSelection, graph);
	
	int currentRenderPassCount = CreateRenderPass(deviceSelection, attachmentGraphsInstances.Get(currentGraphInstance));

	if (subAttachCount)
		*subAttachCount = currentRenderPassCount;

	return currentGraphInstance;
}

int RenderInstance::CreatePhysicalDeviceAdapter(GPUFeatureRequest* requestedPhysicalFeatures, LogicalDeviceFeatures* requestedDeviceFeatures)
{
	uint32_t deviceExtNameCount = vkInstance->GetLogicalDeviceExtensionsCount(requestedDeviceFeatures);

	const char** deviceFeatureNames = (const char**)cacheAllocator->Allocate(sizeof(char*) * deviceExtNameCount);

	vkInstance->GetLogicalDeviceExtensions(requestedDeviceFeatures, deviceFeatureNames);

	int driverGpuIndex = -1;

	EntryHandle physicalIndex = vkInstance->CreatePhysicalDevice(requestedPhysicalFeatures, deviceFeatureNames, deviceExtNameCount, &driverGpuIndex);

	int physicalEntryIndex = physicalDeviceCounter++;

	RenderPhysicalDeviceContainer* container = &physicalDeviceIndices[physicalEntryIndex];

	container->physicalDeviceIndex = physicalIndex;
	container->information.minUniformAlignment = vkInstance->GetMinimumUniformBufferAlignment(physicalIndex);
	container->information.minStorageAlignment = vkInstance->GetMinimumStorageBufferAlignment(physicalIndex);
	container->information.maxMSAALevels = findMSB(vkInstance->GetMaxMSAALevels(physicalIndex));
	container->information.deviceTimeStampPeriodNS = vkInstance->GetTimeStampPeriod(physicalIndex);
	container->information.optimalImageCopyOffsetAlignment = vkInstance->GetOptimalImageCopyOffsetAlignment(physicalIndex);
	container->internalDriverDeviceListIdentifier = driverGpuIndex;

	return physicalEntryIndex;
}

int RenderInstance::OpenPhysicalDevicePicker()
{
	int gpuCount = vkInstance->GetNumberOfGPUDevices();

	if (gpuCount <= 0)
	{
		return -1;
	}

	physicalDevicesOnComputerPerDriver = gpuCount;

	return 0;
}

void RenderInstance::ClosePhysicalDevicePicker()
{
	vkInstance->FreePotentialGPUs();
}

int RenderInstance::CreatePhysicalDeviceAdapterWithQuerying(GPUFeatureRequest* requestedPhysicalFeatures, LogicalDeviceFeatures* requestedDeviceFeatures)
{
	uint32_t deviceExtNameCount = vkInstance->GetLogicalDeviceExtensionsCount(requestedDeviceFeatures);

	const char** deviceFeatureNames = (const char**)cacheAllocator->Allocate(sizeof(char*) * deviceExtNameCount);

	vkInstance->GetLogicalDeviceExtensions(requestedDeviceFeatures, deviceFeatureNames);

	int gpuIndex = 0;

	uint64_t expectedDeviceExtMask = (1ULL << deviceExtNameCount) - 1;

	int* physicalDeviceExlusionList = nullptr;

	if (physicalDeviceCounter)
	{
		physicalDeviceExlusionList = (int*)cacheAllocator->Allocate(sizeof(int) * physicalDeviceCounter);

		for (int i = 0; i < physicalDeviceCounter; i++)
		{
			RenderPhysicalDeviceContainer* container = &physicalDeviceIndices[i];

			physicalDeviceExlusionList[i] = container->internalDriverDeviceListIdentifier;
		}
	}

	char topLineMessageBuffer[64];

	for (; gpuIndex < physicalDevicesOnComputerPerDriver; gpuIndex++)
	{
		bool alreadyUsed = false;

		for (int i = 0; i < physicalDeviceCounter; i++)
		{
			if (physicalDeviceExlusionList[i] == gpuIndex)
			{
				alreadyUsed = true;
				break;
			}
		}

		if (alreadyUsed) continue;

		GPUFeatureRequest currentRequest{};
		uint64_t currentDeviceExtMask = 0;
		
		if (vkInstance->QuerySpecificPhysicalDeviceFeatures(requestedPhysicalFeatures, &currentRequest, deviceFeatureNames, deviceExtNameCount, gpuIndex, &currentDeviceExtMask))
			break;

		int size = snprintf(topLineMessageBuffer, sizeof(topLineMessageBuffer), "GPU at index %d has mismatched values\n", gpuIndex);

		internalRendererLogger->AddLogMessage(LOGINFO, topLineMessageBuffer, size);

		if (!(currentRequest.deviceType & requestedPhysicalFeatures->deviceType))
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Desired GPU type is not what was requested\n"));

		if (currentRequest.desiredMaxImageWidth < requestedPhysicalFeatures->desiredMaxImageWidth)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Desired Max Image Width is less than requested\n"));

		if (currentRequest.desiredMaxImageHeight < requestedPhysicalFeatures->desiredMaxImageHeight)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Desired Max Image Height is less than requested\n"));

		if (requestedPhysicalFeatures->requireDescriptorBindingPartiallyBound &&
			!currentRequest.requireDescriptorBindingPartiallyBound)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Descriptor Binding Partially Bound is not supported\n"));

		if (requestedPhysicalFeatures->requireDescriptorBindingSampledImageUpdateAfterBind &&
			!currentRequest.requireDescriptorBindingSampledImageUpdateAfterBind)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Descriptor Binding Sampled Image Update After Bind is not supported\n"));

		if (requestedPhysicalFeatures->requireDescriptorBindingUpdateUnusedWhilePending &&
			!currentRequest.requireDescriptorBindingUpdateUnusedWhilePending)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Descriptor Binding Update Unused While Pending is not supported\n"));

		if (requestedPhysicalFeatures->requireDescriptorBindingVariableDescriptorCount &&
			!currentRequest.requireDescriptorBindingVariableDescriptorCount)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Descriptor Binding Variable Descriptor Count is not supported\n"));

		if (requestedPhysicalFeatures->requireShaderSampledImageArrayNonUniformIndexing &&
			!currentRequest.requireShaderSampledImageArrayNonUniformIndexing)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Shader Sampled Image Array Non Uniform Indexing is not supported\n"));

		if (requestedPhysicalFeatures->requireStorageBuffer8BitAccess &&
			!currentRequest.requireStorageBuffer8BitAccess)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Storage Buffer 8 Bit Access is not supported\n"));

		if (requestedPhysicalFeatures->requireDrawIndirectCount &&
			!currentRequest.requireDrawIndirectCount)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Draw Indirect Count is not supported\n"));

		if (requestedPhysicalFeatures->requireRuntimeDescriptorArray &&
			!currentRequest.requireRuntimeDescriptorArray)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Runtime Descriptor Array is not supported\n"));

		if (requestedPhysicalFeatures->requireGeometryShader &&
			!currentRequest.requireGeometryShader)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Geometry Shader is not supported\n"));

		if (requestedPhysicalFeatures->requireTextureCompressionBC &&
			!currentRequest.requireTextureCompressionBC)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Texture Compression BC is not supported\n"));

		if (requestedPhysicalFeatures->requireTessellationShader &&
			!currentRequest.requireTessellationShader)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Tessellation Shader is not supported\n"));

		if (requestedPhysicalFeatures->requireSamplerAnisotropy &&
			!currentRequest.requireSamplerAnisotropy)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Sampler Anisotropy is not supported\n"));

		if (requestedPhysicalFeatures->requireMultiDrawIndirect &&
			!currentRequest.requireMultiDrawIndirect)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Multi Draw Indirect is not supported\n"));

		if (requestedPhysicalFeatures->requireWideLines &&
			!currentRequest.requireWideLines)
			internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Wide Lines is not supported\n"));

		if (currentDeviceExtMask != expectedDeviceExtMask)
		{
			for (uint32_t i = 0; i < deviceExtNameCount; i++)
			{
				if (!(currentDeviceExtMask & (1ull << i)))
				{
					internalRendererLogger->AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Device extenstion not supported : "));
					internalRendererLogger->AddLogMessage(LOGINFO, deviceFeatureNames[i], strlen(deviceFeatureNames[i]));
					internalRendererLogger->AddLogMessage(LOGINFO, "\n", 1);
				}
			}
		}
	}

	if (gpuIndex == physicalDevicesOnComputerPerDriver)
	{
		internalRendererLogger->ProcessMessage();
		return -1;
	}

	EntryHandle physicalIndex = vkInstance->CreateGPUFromIndex(gpuIndex);

	int physicalEntryIndex = physicalDeviceCounter++;

	RenderPhysicalDeviceContainer* container = &physicalDeviceIndices[physicalEntryIndex];

	container->physicalDeviceIndex = physicalIndex;
	container->information.minUniformAlignment = vkInstance->GetMinimumUniformBufferAlignment(physicalIndex);
	container->information.minStorageAlignment = vkInstance->GetMinimumStorageBufferAlignment(physicalIndex);
	container->information.maxMSAALevels = findMSB(vkInstance->GetMaxMSAALevels(physicalIndex));
	container->information.deviceTimeStampPeriodNS = vkInstance->GetTimeStampPeriod(physicalIndex);
	container->information.optimalImageCopyOffsetAlignment = vkInstance->GetOptimalImageCopyOffsetAlignment(physicalIndex);
	container->internalDriverDeviceListIdentifier = gpuIndex;

	return physicalEntryIndex;
}

int RenderInstance::CreatePerFrameStagingBuffers(int deviceSelection, uint32_t bufferSize)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		deviceContainer->stagingBuffers[i] = dev->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			API::ConvertMemoryTypeToVkMemoryPropertyFlags(MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE)
		);
		deviceContainer->stagingBufferAllocators[i].dataSize = bufferSize;
		deviceContainer->stagingBufferAllocators[i].dataAllocator = 0;
	}

	return 0;
}

int RenderInstance::CreateLogicalDevice(LogicalDeviceCreateInfo* createInfo)
{	
	int currentLogicalDeviceIndex = logicalDeviceCounter++;

	RenderLogicalDeviceContainer* logicalDevice = &logicalDeviceIndices[currentLogicalDeviceIndex];

	RenderPhysicalDeviceContainer* physicalDevice = &physicalDeviceIndices[createInfo->physicalDeviceIndex];

	EntryHandle physicalIndex = physicalDevice->physicalDeviceIndex;

	logicalDevice->relatedPhysDeviceInfo = &physicalDevice->information;

	uint32_t deviceExtNameCount = vkInstance->GetLogicalDeviceExtensionsCount(createInfo->requestedDeviceFeatures);

	const char** deviceFeatureNames = (const char**)cacheAllocator->Allocate(sizeof(char*) * deviceExtNameCount);

	vkInstance->GetLogicalDeviceExtensions(createInfo->requestedDeviceFeatures, deviceFeatureNames);

	VkPhysicalDeviceVulkan12Features features12{};
	VkPhysicalDeviceFeatures2 features2{};

	API::ConvertGPUFeatureRequestToVkPhysicalDeviceProperties(createInfo->requestedPhysicalFeatures, &features2, &features12);

	EntryHandle deviceIndex = vkInstance->CreateLogicalDevice(physicalIndex);

	logicalDevice->logicalDeviceIndex = deviceIndex;

	VKDevice* majorDevice = vkInstance->GetLogicalDevice(deviceIndex);

	uint32_t totalQueueFamilyCount = majorDevice->QueueFamilyDetailsCount();

	VkQueueFamilyProperties* famPropsContainer = (VkQueueFamilyProperties*)cacheAllocator->CAllocate(totalQueueFamilyCount * sizeof(VkQueueFamilyProperties));

	QueueIndex queueIndices[2]{};
	uint32_t queueCounts[2]{};

	uint32_t queueCount = 0, totalQueuePrios = 0;

	int queueSuccessful = majorDevice->GetQueueByMask(&queueIndices[0], &queueCounts[0], VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, famPropsContainer);

	if (queueSuccessful)
	{
		//TO-DO implement fallback
	}

	queueSuccessful = majorDevice->GetPresentQueue(&queueIndices[1], &queueCounts[1], vkInstance->GetRenderSurface(windowsSurfaces.pool[createInfo->surfaceIndexForPresent]()), famPropsContainer);

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

	void* driverDeviceDataHead = storageAllocator->Allocate(createInfo->driverPermanentSize + createInfo->driverCacheSize, 64);
	void* deviceDataHead = storageAllocator->Allocate(createInfo->deviceInstPermanentSize + createInfo->deviceInstHandleSize + createInfo->deviceInstCacheSize + 192, 64);

	majorDevice->CreateLogicalDevice(
		deviceFeatureNames, 
		deviceExtNameCount,
		&features2,
		queueIndices,
		queueCounts,
		queuePriorites,
		queueCount, 
		createInfo->deviceInstPermanentSize,
		createInfo->deviceInstHandleSize,
		createInfo->deviceInstCacheSize,
		createInfo->driverPermanentSize,
		createInfo->driverCacheSize,
		driverDeviceDataHead,
		deviceDataHead
	);

	logicalDevice->graphicsComputeTransfer = majorDevice->CreateQueueManager(queueIndices[0], queueCounts[0], VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT, (queueCount == 1) ? true : false);

	if (queueCount > 1)
	{
		logicalDevice->presentQueue = majorDevice->CreateQueueManager(queueIndices[1], queueCounts[1], 0, true);
	}
	else
	{
		logicalDevice->presentQueue = logicalDevice->graphicsComputeTransfer;
	}

	logicalDevice->currentCommandBufferIndex = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * MAX_FRAMES_IN_FLIGHT, alignof(EntryHandle));
	logicalDevice->stagingBuffers = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * MAX_FRAMES_IN_FLIGHT, alignof(EntryHandle));

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		EntryHandle* lprimaryCommandBuffers = majorDevice->CreateReusableCommandBuffers(logicalDevice->graphicsComputeTransfer, 1, true, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		logicalDevice->currentCommandBufferIndex[i] = *lprimaryCommandBuffers;
	}

	logicalDevice->stagingBufferAllocators = (DeviceSlabAllocator*)storageAllocator->Allocate(sizeof(DeviceSlabAllocator) * MAX_FRAMES_IN_FLIGHT, alignof(DeviceSlabAllocator));

	logicalDevice->queryResults = (uint32_t*)storageAllocator->Allocate(sizeof(uint32_t) * createInfo->maxQueries, alignof(uint32_t));

	logicalDevice->queryCounts = (uint32_t*)storageAllocator->Allocate(sizeof(uint32_t) * MAX_FRAMES_IN_FLIGHT, alignof(uint32_t));

	logicalDevice->maxQueryResults = createInfo->maxQueries;

	logicalDevice->queryPoolIndex = majorDevice->CreateQueryPool(VK_QUERY_TYPE_TIMESTAMP, MAX_FRAMES_IN_FLIGHT * createInfo->maxQueries);

	logicalDevice->deviceTimelineSyncObject.currentValue = 0;
	logicalDevice->deviceTimelineSyncObject.driverTimelineObject = *majorDevice->CreateTimelineSemaphores(1, logicalDevice->deviceTimelineSyncObject.currentValue);

	return currentLogicalDeviceIndex;
}

int RenderInstance::CreateSwapChainHandle(int deviceSelection, int surfaceIndex, ImageFormat mainBackBufferColorFormat, uint32_t _width, uint32_t _height)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	RenderWindowSpecificData* winData = windowsSurfaces.Get(surfaceIndex);

	int swapChainInternalIndex = swapChains.Allocate();

	EntryHandle swapChainIndex = dev->CreateSwapChain(MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT, API::ConvertImageFormatToVulkanFormat(mainBackBufferColorFormat), winData->vkRenderSurface);

	CreateSwapChainData(deviceSelection, swapChainIndex, _width, _height, false);

	VKSwapChain* vkSwcData = dev->GetSwapChain(swapChainIndex);

	RenderSwapchainData* swcData = swapChains.Get(swapChainInternalIndex);

	swcData->swapChainIdx = swapChainIndex;
	swcData->height = _height;
	swcData->width = _width;

	uint32_t imageCount = swcData->imageCount = vkSwcData->imageCount;

	swcData->rendererWaitSemaphores = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * MAX_FRAMES_IN_FLIGHT);
	swcData->rendererFinishedSemaphores = (EntryHandle*)storageAllocator->Allocate(sizeof(EntryHandle) * imageCount);

	swcData->textureIds = (int*)storageAllocator->Allocate(sizeof(int) * imageCount);

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		swcData->rendererWaitSemaphores[i] = *dev->CreateSemaphores(1);

		
	}

	for (uint32_t i = 0; i < imageCount; i++)
	{
		swcData->rendererFinishedSemaphores[i] = *dev->CreateSemaphores(1);

		int textureID = swcData->textureIds[i] = textureResourceHandles.Allocate();

		RenderTextureDescription* desc = textureResourceHandles.Get(textureID);

		desc->arrayLayers = 1;

		desc->imageHeight = _height;

		desc->imageWidth = _width;

		desc->mipLayers = 1;

		desc->format = mainBackBufferColorFormat;

		desc->viewCount = 0;

		int resourceIndex = desc->resourceStatusIndex = resourceStatuses.Allocate();

		ResourceStatus* status = resourceStatuses.Get(resourceIndex);

		status->resourceType = ResourceStatusType::MANAGED_IMAGE_RESOURCE;

		int viewIndex = desc->viewIndex[desc->viewCount++] = textureViewsResourceHandles.Allocate();

		RenderImageViewDescription* viewDesc = textureViewsResourceHandles.Get(viewIndex);

		viewDesc->viewIndex = vkSwcData->imageViews[i];

		viewDesc->mask = COLOR_IMAGE_ASPECT;

		viewDesc->firstLayer = viewDesc->firstMipLevel = 0;
		viewDesc->layerCount = viewDesc->mipLevelCount = 1;

		viewDesc->desiredLayoutForView = ImageLayout::SHADERREADABLE;
	}

	return swapChainInternalIndex;
}

ImageFormat RenderInstance::FindSupportedBackBufferColorFormat(int physicalDeviceIndex, int surfaceIndex, ImageFormat* requestedFormats, uint32_t requestSize)
{
	EntryHandle physicalIndex = physicalDeviceIndices[physicalDeviceIndex].physicalDeviceIndex;

	for (uint32_t i = 0; i < requestSize; i++)
	{
		bool ret = vkInstance->ValidateSwapChainFormatSupport(physicalIndex, API::ConvertImageFormatToVulkanFormat(requestedFormats[i]), windowsSurfaces[surfaceIndex]());

		if (ret)
		{
			return requestedFormats[i];
		}
	}

	return ImageFormat::IMAGE_UNKNOWN;
}

ImageFormat RenderInstance::FindSupportedDepthFormat(int deviceSelection, ImageFormat* requestedFormats, uint32_t requestSize)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	VkFormat format;

	for (uint32_t i = 0; i < requestSize; i++)
	{
		format = API::ConvertImageFormatToVulkanFormat(requestedFormats[i]);

		format = VK::Utils::findSupportedFormat(dev->gpu,
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

int RenderInstance::CreateSampler(int deviceSelection, uint32_t maxMipsLevel)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	EntryHandle samplerHandle = dev->CreateSampler(maxMipsLevel);

	int samplerIndex = samplerResourceHandles.Allocate();

	samplerResourceHandles.pool[samplerIndex] = samplerHandle;

	return samplerIndex;
}

uint32_t RenderInstance::GetSwapChainHeight(int swapChainIndex)
{
	RenderSwapchainData* data = swapChains.Get(swapChainIndex);

	return data->height;
}

uint32_t RenderInstance::GetSwapChainWidth(int swapChainIndex)
{
	RenderSwapchainData* data = swapChains.Get(swapChainIndex);

	return data->width;
}

EntryHandle RenderInstance::CreateShaderResourceSet(ShaderResourceManager* descriptorManager, int deviceSelection, int descriptorSet)
{
	if (descriptorManager->descriptorSetHandles[descriptorSet] != EntryHandle())
	{
		return descriptorManager->descriptorSetHandles[descriptorSet];
	}

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	ShaderResourceSet* set = descriptorManager->descriptorSets[descriptorSet];

	int frames = set->setCount;	

	uint32_t varCountRequested = 0;

	int bindingCount = set->templateMetaData->bindingCount;

	int layoutHandle = set->templateMetaData->vulkanDescLayout;

	int lastBinding = bindingCount - 1;;

	ShaderResourceHeader* lastheader = (ShaderResourceHeader*)&set->resourceBindings[lastBinding];

	if (lastheader->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY)
	{
		varCountRequested = (lastheader->arrayCount & DESCRIPTOR_COUNT_MASK);
	}

	DescriptorSetBuilder* builder = dev->CreateDescriptorSetBuilder(descriptorManager->deviceResourceHeap, shaderResourceTemplates[layoutHandle], frames, varCountRequested);

	for (int i = 0; i < bindingCount; i++)
	{
		ShaderResourceArray* header = (ShaderResourceArray*)&set->resourceBindings[i];

		switch (header->type)
		{
			case ShaderResourceType::SAMPLERSTATE:
			{
				ShaderResourceSampler* samplers = &header->resourceArray.samplers;
	
				for (int sampler = 0; sampler < samplers->samplerCount; sampler++)
				{
					builder->AddSamplerDescription(samplerResourceHandles[samplers->samplerHandles[sampler]], sampler, i, 0, frames);
				}

				break;
			}
			case ShaderResourceType::IMAGE2D:
			{
				ShaderResourceImage* image = &header->resourceArray.images;
				
				for (int imageIndex = 0; imageIndex < image->textureCount; imageIndex++)
				{
					RenderTextureDescription* desc = textureResourceHandles.Get(image->textureDetails[imageIndex].textureHandle);

					RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(desc->viewIndex[image->textureDetails[imageIndex].viewIndex]);

					builder->AddImageResourceDescription(imageViewDesc->viewIndex, API::ConvertImageLayoutToVulkanImageLayout(imageViewDesc->desiredLayoutForView), imageIndex, i, 0, frames);
				}

				break;
			}
			case ShaderResourceType::IMAGESTORE2D:
			{
				ShaderResourceImage* image = &header->resourceArray.images;

				for (int imageIndex = 0; imageIndex < image->textureCount; imageIndex++)
				{
					RenderTextureDescription* desc = textureResourceHandles.Get(image->textureDetails[imageIndex].textureHandle);

					RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(desc->viewIndex[image->textureDetails[imageIndex].viewIndex]);

					builder->AddStorageImageDescription(imageViewDesc->viewIndex, API::ConvertImageLayoutToVulkanImageLayout(imageViewDesc->desiredLayoutForView), imageIndex, i, 0, frames);
				}

				break;
			}
			case ShaderResourceType::SAMPLER3D:
			case ShaderResourceType::SAMPLER2D:
			case ShaderResourceType::SAMPLERCUBE:
			{
				ShaderResourceCombinedImage* image = &header->resourceArray.combinedImages;
			
				for (int imageIndex = 0; imageIndex < image->textureCount; imageIndex++)
				{
					RenderTextureDescription* desc = textureResourceHandles.Get(image->textureDetails[imageIndex].textureHandle);

					RenderImageViewDescription* imageViewDesc = textureViewsResourceHandles.Get(desc->viewIndex[image->textureDetails[imageIndex].viewIndex]);

					EntryHandle samplerHandle = samplerResourceHandles[image->textureDetails[imageIndex].samplerHandle];

					builder->AddCombinedTextureArray(imageViewDesc->viewIndex, samplerHandle, API::ConvertImageLayoutToVulkanImageLayout(imageViewDesc->desiredLayoutForView), imageIndex, i, 0, frames);
				}
				
				break;
			}
			case ShaderResourceType::STORAGE_BUFFER:
			{
				ShaderResourceBuffer* buffer = &header->resourceArray.buffers;
				
				int arrayCount = buffer->bufferCount;

				for (int j = 0; j < arrayCount; j++)
				{
					RenderAllocation* alloc = allocations.Get(buffer->allocationIndex[j]);

					if (alloc->allocType == AllocationType::PERFRAME)
						builder->AddStorageBuffer(dev->GetBufferHandle(bufferHandles[alloc->memIndex].bufferHandle), alloc->deviceAllocSize / frames, i, frames, alloc->offset, 0, j);
					else
						builder->AddStorageBufferDirect(dev->GetBufferHandle(bufferHandles[alloc->memIndex].bufferHandle), alloc->deviceAllocSize, i, frames, alloc->offset, 0, j);
					
				}
				break;
			}
			case ShaderResourceType::UNIFORM_BUFFER:
			{
				ShaderResourceBuffer* buffer = &header->resourceArray.buffers;

				int arrayCount = buffer->bufferCount;

				for (int j = 0; j < arrayCount; j++)
				{
					RenderAllocation* alloc = allocations.Get(buffer->allocationIndex[j]);

					if (alloc->allocType == AllocationType::PERFRAME)
						builder->AddUniformBuffer(dev->GetBufferHandle(bufferHandles[alloc->memIndex].bufferHandle), alloc->deviceAllocSize / frames, i, frames, alloc->offset, 0, j);
					else
						builder->AddUniformBufferDirect(dev->GetBufferHandle(bufferHandles[alloc->memIndex].bufferHandle), alloc->deviceAllocSize, i, frames, alloc->offset, 0, j);
				}
				break;
			}
			case ShaderResourceType::BUFFER_VIEW:
			{
				ShaderResourceBuffer* buffer = &header->resourceArray.views;

				int arrayCount = buffer->bufferCount;

				for (int j = 0; j < arrayCount; j++)
				{
					RenderAllocation* alloc = allocations.Get(buffer->allocationIndex[j]);

					int frameCount = (alloc->allocType == AllocationType::PERFRAME) ? MAX_FRAMES_IN_FLIGHT : 1;

					for (int g = 0; g < frameCount; g++)
					{
						VkBufferView handle = dev->GetBufferView(alloc->viewIndex, g);

						if (header->action == ShaderResourceAction::SHADERREAD)
						{
							builder->AddUniformBufferView(handle, i, g, 1, j);
						}
						else if (header->action == ShaderResourceAction::SHADERWRITE)
						{
							builder->AddStorageBufferView(handle, i, g, 1, j);
						}
					}
				}
				break;
			}
		}
	}

	EntryHandle handle = builder->AddDescriptorsToCache();

	descriptorManager->descriptorSetHandles.pool[descriptorSet] = handle;

	return handle;
}

int RenderInstance::CreateGraphicsPipelineObject(int deviceSelection, GraphicsIntermediaryPipelineInfo* info)
{
	PipelineInstanceData* pid = &pipelineInstancesIdentifier.Get(info->pipelinename)->instanceData;

	int ret = pipelineHandles.Allocate();

	PipelineHandle* posStruct = pipelineHandles.Get(ret);
	
	posStruct->group = GRAPHICSO;
	posStruct->pipelineIdentifierGroup = info->pipelinename;
	posStruct->resourceSetCount = info->descCount;

	uint32_t pushRangeCount = 0;

	for (uint32_t i = 0; i < info->descCount; i++)
	{
		ShaderResourceManager* descriptorManager = descriptorManagers.Get(info->descriptorsetid[i].descriptorManagerIndex);
		posStruct->resourceSets[i] = info->descriptorsetid[i];
		CreateShaderResourceSet(descriptorManager, deviceSelection, info->descriptorsetid[i].descriptorSetIndex);
		pushRangeCount += descriptorManager->GetConstantBufferCount(info->descriptorsetid[i].descriptorSetIndex);
	}

	posStruct->pushRangeCount = pushRangeCount;
	posStruct->indexBufferHandle = info->indexBufferHandle;
	posStruct->indexCount = info->indexCount;
	posStruct->vertexBufferHandle = info->vertexBufferHandle;
	posStruct->vertexCount = info->vertexCount;
	posStruct->indirectBufferHandle = info->indirectAllocation;
	posStruct->indirectCountBufferHandle = info->indirectCountAllocation;
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

int RenderInstance::CreateComputePipelineObject(int deviceSelection, ComputeIntermediaryPipelineInfo* info)
{
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
		ShaderResourceManager* descriptorManager = descriptorManagers.Get(info->descriptorsetid[i].descriptorManagerIndex);
		posStruct->resourceSets[i] = info->descriptorsetid[i];
		CreateShaderResourceSet(descriptorManager, deviceSelection, info->descriptorsetid[i].descriptorSetIndex);
		pushRangeCount += descriptorManager->GetConstantBufferCount(info->descriptorsetid[i].descriptorSetIndex);
	}

	posStruct->pushRangeCount = pushRangeCount;

	return ret;
}

void RenderInstance::DrawScene(int deviceSelection, int commandStreamIndex, uint32_t imageIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	deviceContainer->stagingBufferAllocators[currentFrame].dataAllocator = 0;

	EntryHandle cbindex = deviceContainer->currentCommandBufferIndex[currentFrame];
	RecordingBufferObject rcb = dev->GetRecordingBufferObject(cbindex);
	rcb.ResetCommandPoolForBuffer();

	SwapUpdateCommands();

	UploadHostTransfers(deviceSelection);

	UploadDescriptorsUpdates(deviceSelection);

	uint32_t accumulatorIndex = PopBarrierAccumulator();

	BarrierAccumulator* accumulator = &barrierAccumulators[accumulatorIndex];

	rcb.BeginRecordingCommand(nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	rcb.ResetQueries(deviceContainer->queryPoolIndex, deviceContainer->maxQueryResults * currentFrame, deviceContainer->maxQueryResults);

	UploadDeviceLocalTransfers(deviceSelection, &rcb, accumulator);

	InvokeTransferCommands(deviceSelection, &rcb, accumulator);

	UploadImageMemoryTransfers(deviceSelection, &rcb, accumulator);

	int commandCountIter = 0;

	int queryCountBaseIndex = deviceContainer->maxQueryResults * currentFrame;

	int queryCountIndex = queryCountBaseIndex;

	GPUCommandStreamAllocator* stream = gpuCommandStreams.Get(commandStreamIndex);

	while (commandCountIter < stream->commandCount)
	{
		GPUCommand* command = &stream->commands[commandCountIter];

		if (command->streamType == GPUCommandStreamType::COMPUTE_QUEUE_COMMANDS)
		{
			rcb.WriteTimestamp(deviceContainer->queryPoolIndex, queryCountIndex, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
			
			ComputeQueue* queue = computeQueues.Get(command->indexForStreamType);

			for (uint32_t pipeInst = 0; pipeInst < queue->queueCount; pipeInst++)
			{
				int pipelineIndex = queue->pipelines[pipeInst];

				PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

				GeneratePipelineDescriptorBarriers(deviceSelection, &rcb, handle->resourceSets, handle->resourceSetCount, accumulator, pipelineIndex);
			}

			InsertAccumulatedBarriers(&rcb, accumulator);

			for (uint32_t pipeInst = 0; pipeInst < queue->queueCount; pipeInst++)
			{
				int pipelineIndex = queue->pipelines[pipeInst];

				PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

				EntryHandle pipelineTemp = pipelineInstancesIdentifier[handle->pipelineIdentifierGroup].pipelineIndices[0];
			
				rcb.BindComputePipeline(pipelineTemp);

				for (uint32_t ii = 0; ii < handle->resourceSetCount; ii++)
				{
					ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle->resourceSets[ii].descriptorManagerIndex);

					rcb.BindComputeDescriptorSets(descriptorManager->descriptorSetHandles[handle->resourceSets[ii].descriptorSetIndex], currentFrame, 1, ii, 0, nullptr);
				}

				for (uint32_t ii = 0, jj = 0, constantBufferPerSet = 0; ii < handle->pushRangeCount && jj < handle->resourceSetCount;)
				{
					ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle->resourceSets[jj].descriptorManagerIndex);

					ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager->GetConstantBuffer(handle->resourceSets[jj].descriptorSetIndex, constantBufferPerSet++);
					if (!pushArgs)
					{
						jj++;
						constantBufferPerSet = 0;
						continue;
					}

					rcb.PushConstantsCommand(pushArgs->offset, pushArgs->size, API::ConvertShaderStageToVulkanShaderStage(pushArgs->stage), pushArgs->data);

					ii++;
				}

				InsertIntraPassBarrier(&rcb, accumulator, pipelineIndex);

				rcb.DispatchCommand(handle->x, handle->y, handle->z);

			}

			ResetIntraBarrierAccumulator(accumulator);
			
			rcb.WriteTimestamp(deviceContainer->queryPoolIndex, queryCountIndex+1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			queryCountIndex += 2;
		}
		else if (command->streamType == GPUCommandStreamType::ATTACHMENT_COMMANDS)
		{
			AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(command->indexForStreamType);

			for (int i = 0; i < currentGraphInstance->graphLayout->passesCount; i++)
			{
				rcb.WriteTimestamp(deviceContainer->queryPoolIndex, queryCountIndex, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

				AttachmentRenderPassInstance* rpInst = &currentGraphInstance->passes[i];

				int SubRenderTargetSelection = rpInst->rpType == RenderPassType::SWAPCHAIN_IMAGE_COUNT ? imageIndex : currentFrame;

				int sampleLevelForRenderPass = rpInst->currentSampleCount;

				int possibleQueueIndex = rpInst->graphicsOTQIndex;

				int renderTargetPerPassBase = rpInst->baseRenderTargetData;

				int absoluteRenderTargetIndex = currentGraphInstance->consecutiveRenderTargetsBase + renderTargetPerPassBase + sampleLevelForRenderPass;

				RenderTarget* renderTarget = dev->GetRenderTarget(mainRenderTargets[absoluteRenderTargetIndex]);

				VkClearValue* clears = (VkClearValue*)cacheAllocator->Allocate(sizeof(VkClearValue) * rpInst->attachInstCount);

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

				if (possibleQueueIndex >= 0)
				{
					RenderQueue* queue = renderTargetQueues.Get(possibleQueueIndex);

					for (uint32_t pipeInst = 0; pipeInst < queue->queueCount; pipeInst++)
					{
						int pipelineIndex = queue->pipelines[pipeInst];

						PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

						GeneratePipelineDescriptorBarriers(deviceSelection, &rcb, handle->resourceSets, handle->resourceSetCount, accumulator, pipelineIndex);

						GenerateDrawBindingsBarriers(deviceSelection, &rcb, handle, accumulator);

						//InsertIntraPassBarrier(&rcb, accumulator, pipelineIndex);
					}

					InsertAccumulatedBarriers(&rcb, accumulator);
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
						int pipelineIndex = queue->pipelines[pipeInst];

						PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

						PipelineInstanceData* pid = &pipelineInstancesIdentifier.Get(handle->pipelineIdentifierGroup)->instanceData;

						uint32_t pipelineOffset = 0;

						for (int i = 0; i < pid->frameGraphCount; i++)
						{
							if (command->indexForStreamType == pid->frameGraphIndices[i])
							{
								pipelineOffset = pid->frameGraphPipelineIndices[i];
								break;
							}
						}

						EntryHandle pipelineTemp = pipelineInstancesIdentifier[handle->pipelineIdentifierGroup].pipelineIndices[pipelineOffset + sampleLevelForRenderPass];

						rcb.BindGraphicsPipeline(pipelineTemp);

						for (uint32_t ii = 0; ii < handle->resourceSetCount; ii++)
						{
							ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle->resourceSets[ii].descriptorManagerIndex);

							rcb.BindGraphicsDescriptorSets(descriptorManager->descriptorSetHandles[handle->resourceSets[ii].descriptorSetIndex], currentFrame, 1, ii, 0, nullptr);
						}

						uint32_t vertexCount = handle->vertexCount;

						uint32_t indexCount = handle->indexCount;

						size_t perFrameIndirectBufferOffset = -1, perFrameIndirectCountBufferOffset = -1;

						size_t vertexOffset = -1, indexOffset = -1, indirectBufferBaseOffset = -1, indirectCountBufferBaseOffset = -1;

						int vertexMemIndex = -1, indexMemIndex = -1, indirectBufferIndex = -1, indirectCountBufferIndex = -1;

						if (handle->vertexBufferHandle != -1)
						{		
							RenderAllocation* vertexAlloc = allocations.Get(handle->vertexBufferHandle);

							vertexMemIndex = vertexAlloc->memIndex;

							vertexOffset = vertexAlloc->offset;

							rcb.BindVertexBuffer(bufferHandles[vertexMemIndex].bufferHandle, 0, 1, &vertexOffset);
						}

						if (handle->indexBufferHandle != -1)
						{			
							RenderAllocation* indexAlloc = allocations.Get(handle->indexBufferHandle);

							indexMemIndex = indexAlloc->memIndex;

							indexOffset = indexAlloc->offset;

							rcb.BindIndexBuffer(bufferHandles[indexMemIndex].bufferHandle, indexOffset, handle->indexSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
						}

						if (handle->indirectBufferHandle != -1)
						{
							RenderAllocation* indirectBufferAlloc = allocations.Get(handle->indirectBufferHandle);

							size_t align = indirectBufferAlloc->alignment;

							size_t copiesOfstruct = static_cast<size_t>(indirectBufferAlloc->structureCopies);

							indirectBufferBaseOffset = indirectBufferAlloc->offset;

							perFrameIndirectBufferOffset = (((indirectBufferAlloc->requestedSize * copiesOfstruct) + (align - 1)) & ~(align - 1));

							indirectBufferIndex = indirectBufferAlloc->memIndex;
						}

						if (handle->indirectCountBufferHandle != -1)
						{
							RenderAllocation* indirectCountBufferAlloc = allocations.Get(handle->indirectCountBufferHandle);

							size_t align = indirectCountBufferAlloc->alignment;

							size_t copiesOfstruct = static_cast<size_t>(indirectCountBufferAlloc->structureCopies);

							indirectCountBufferBaseOffset = indirectCountBufferAlloc->offset;

							perFrameIndirectCountBufferOffset = (((indirectCountBufferAlloc->requestedSize * copiesOfstruct) + (align - 1)) & ~(align - 1));

							indirectCountBufferIndex = indirectCountBufferAlloc->memIndex;
						}

						for (uint32_t ii = 0, jj = 0, constantBufferPerSet = 0; ii < handle->pushRangeCount && jj < handle->resourceSetCount;)
						{
							ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle->resourceSets[ii].descriptorManagerIndex);

							ShaderResourceConstantBuffer* pushArgs = (ShaderResourceConstantBuffer*)descriptorManager->GetConstantBuffer(handle->resourceSets[jj].descriptorSetIndex, constantBufferPerSet++);
							
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
							perFrameIndirectBufferOffset *=  currentFrame;

							perFrameIndirectCountBufferOffset *= currentFrame;

							if (indexMemIndex != -1)
							{
								if (handle->indirectCountBufferHandle != -1)
								{
									rcb.BindingDrawIndexedIndirectCount(
										bufferHandles[indirectBufferIndex].bufferHandle, 
										bufferHandles[indirectCountBufferIndex].bufferHandle, 
										indirectBufferBaseOffset + perFrameIndirectBufferOffset, 
										indirectCountBufferBaseOffset + perFrameIndirectCountBufferOffset, 
										handle->indirectDrawCount);
								}
								else
								{
									rcb.BindingIndexedIndirectDrawCmd(bufferHandles[indirectBufferIndex].bufferHandle, handle->indirectDrawCount, indirectBufferBaseOffset + perFrameIndirectBufferOffset);
								}
							}
							else
							{
								if (handle->indirectCountBufferHandle != -1)
								{
									rcb.BindingDrawIndirectCount(
										bufferHandles[indirectBufferIndex].bufferHandle,
										bufferHandles[indirectCountBufferIndex].bufferHandle,
										indirectBufferBaseOffset + perFrameIndirectBufferOffset,
										indirectCountBufferBaseOffset + perFrameIndirectCountBufferOffset,
										handle->indirectDrawCount);
								}
								else
								{
									rcb.BindingIndirectDrawCmd(bufferHandles[indirectBufferIndex].bufferHandle, handle->indirectDrawCount, indirectBufferBaseOffset + perFrameIndirectBufferOffset);
								}
							}
						}
						else
						{
							if (indexMemIndex != -1)
							{
								rcb.BindingDrawIndexedCmd(indexCount, handle->instanceCount, 0, 0, 0);
							}
							else
							{
								rcb.BindingDrawCmd(0, vertexCount, 0, handle->instanceCount);
							}
						}
					}
				}

				rcb.EndRenderPassCommand();

				ResetIntraBarrierAccumulator(accumulator);

				rcb.WriteTimestamp(deviceContainer->queryPoolIndex, queryCountIndex+1, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
			
				queryCountIndex += 2;
			}
		}

		commandCountIter++;
	}

	deviceContainer->queryCounts[currentFrame] = (queryCountIndex - queryCountBaseIndex);

	ReturnBarrierAccumulator(accumulatorIndex);

	rcb.EndRecordingCommand();
}

void RenderInstance::IncreaseMSAA(int frameGraph, int renderPassIndex)
{
	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraph);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	int next = passInstance->currentSampleCount + 1;

	if (next >= passInstance->maxSampleCount)
		return;

	passInstance->currentSampleCount = next;
}

void RenderInstance::DecreaseMSAA(int frameGraph, int renderPassIndex)
{
	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraph);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	int next = passInstance->currentSampleCount - 1;

	if (next < 0)
		return;

	passInstance->currentSampleCount = next;
}

void RenderInstance::ResetCommandList(int commandStreamIndex)
{
	GPUCommandStreamAllocator* stream = gpuCommandStreams.Get(commandStreamIndex);
	stream->commandCount = 0;
}

void RenderInstance::CreateGraphicsQueueForAttachments(int frameGraphIndex, int renderPassIndex, uint32_t pipelineCount)
{
	AttachmentGraphInstance* graphInstance = attachmentGraphsInstances.Get(frameGraphIndex);

	AttachmentRenderPassInstance* passInstance = &graphInstance->passes[renderPassIndex];

	passInstance->graphicsOTQIndex = renderTargetQueues.Allocate();;
}

int RenderInstance::CreateComputeQueue()
{
	return computeQueues.Allocate();
}

void RenderInstance::AddCommandQueue(int commandStreamIndex, int handleIndex, GPUCommandStreamType type)
{
	GPUCommandStreamAllocator* stream = gpuCommandStreams.Get(commandStreamIndex);

	if (stream->commandCount == stream->maxCommandCount)
		return;

	GPUCommand* command = &stream->commands[stream->commandCount++];

	command->indexForStreamType = handleIndex;
	command->streamType = type;
}

int RenderInstance::CreateGPUCommandStream(int maxGPUCommandCount)
{
	int gpuCommandsIndex = gpuCommandStreams.Allocate();

	GPUCommandStreamAllocator* stream = gpuCommandStreams.Get(gpuCommandsIndex);

	stream->commandCount = 0;
	stream->maxCommandCount = maxGPUCommandCount;
	stream->commands = (GPUCommand*)storageAllocator->Allocate(sizeof(GPUCommand) * maxGPUCommandCount);

	return gpuCommandsIndex;
}

void RenderInstance::EndFrame(int commandStreamIndex, int deviceSelection)
{
	char StringBuffer[512];

	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int commandCountIter = 0;

	int queryOffset = 0;

	GPUCommandStreamAllocator* stream = gpuCommandStreams.Get(commandStreamIndex);

	while (commandCountIter < stream->commandCount)
	{
		GPUCommand* command = &stream->commands[commandCountIter];

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

		if (previousFrame < MAX_FRAMES_IN_FLIGHT && (deviceContainer->queryCounts[previousFrame] >= queryOffset + queryCount))
		{
			dev->ReadbackResultsFromQueries(
				deviceContainer->queryPoolIndex,
				(deviceContainer->maxQueryResults * previousFrame) + queryOffset,
				queryCount,
				deviceContainer->queryResults,
				sizeof(uint32_t) * deviceContainer->maxQueryResults,
				sizeof(uint32_t), 
				VK_QUERY_RESULT_WAIT_BIT
			);

			for (uint32_t i = 0; i < queryCount; i += 2)
			{
				double timeNs = (deviceContainer->queryResults[i + 1] - deviceContainer->queryResults[i]) * deviceContainer->relatedPhysDeviceInfo->deviceTimeStampPeriodNS;
				
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
	ComputeQueue* queue = computeQueues.Get(queueIndex);

	queue->pipelines[queue->queueCount++] = psoIndex;
}

void RenderInstance::ReadData(int deviceSelection, int handle, void* dest, int size, int offset)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	size_t allocOffset = 0;

	MemoryType type = HOST_MEMORY_TYPE;

	int memIndex = -1;

	RenderAllocation* allocation = allocations.Get(handle);

	memIndex = allocation->memIndex;

	allocOffset = allocation->offset;

	type = bufferHandles[memIndex].type;

	if (!((type & MemoryTypeBits::HOST_MEMORY_TYPE) || (type & MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE)))
		return;

	EntryHandle index = bufferHandles[memIndex].bufferHandle;

	dev->ReadHostBuffer(dest, index, size, allocOffset+offset);
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

	RenderAllocation* alloc = allocations.Get(allocationIndex);

	if (alloc->allocType == AllocationType::PERFRAME)
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

void RenderInstance::UpdateImageMemory(void* data, int textureIndex, size_t totalSize, int width, int height, int mipLevels, int mipStart, int layerCount, int layerStart, ImageViewAspectMask mask)
{
	RenderDriverUpdateCommandImage* rduci = (RenderDriverUpdateCommandImage*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandImage));

	rduci->data = data;
	rduci->height = height;
	rduci->mipLevels = mipLevels;
	rduci->width = width;
	rduci->layersCount = layerCount;
	rduci->textureIndex = textureIndex;
	rduci->updateType = DriverUpdateType::IMAGEMEMORYUPDATE;
	rduci->totalSize = totalSize;
	rduci->mask = mask;
	rduci->mipStart = mipStart;
	rduci->layerStart = layerStart;
}

void RenderInstance::InsertTransferCommand(int allocationIndex, int size, int allocOffset, uint32_t fillValue)
{
	RenderDriverUpdateCommandFill* rducf = (RenderDriverUpdateCommandFill*)updateCommandBuffers[currentUpdateCommandBuffer]->Allocate(sizeof(RenderDriverUpdateCommandFill));

	int copies = 1;

	RenderAllocation* alloc = allocations.Get(allocationIndex);

	if (alloc->allocType == AllocationType::PERFRAME)
	{
		copies = MAX_FRAMES_IN_FLIGHT;
	}

	rducf->allocationIndex = allocationIndex;
	rducf->allocOffset = allocOffset;
	rducf->fillValue = fillValue;
	rducf->size = size;
	rducf->updateType = DriverUpdateType::TRANSFERCOMMAND;
	rducf->copiesWithin = copies;
}

void RenderInstance::UpdateShaderResourceArray(ShaderResourceSetHandle handle, int bindingindex, ShaderResourceType type, DeviceHandleArrayUpdate* resourceArrayData)
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

		int sizeOfUnderLyingStruct = sizeof(int);

		switch (resourceArrayData->updateType)
		{
		case DeviceHandleArrayUpdateType::TEXTURE_VIEW_UPDATE:
			sizeOfUnderLyingStruct = sizeof(DeviceHandleArrayUpdateTextureView);
			break;
		case DeviceHandleArrayUpdateType::TEXTURE_VIEW_SAMPLER_UPDATE:
			sizeOfUnderLyingStruct = sizeof(DeviceHandleArrayUpdateTextureViewSampler);
			break;
		case DeviceHandleArrayUpdateType::SAMPLER_UPDATE:
		default:
			break;
		}
		
		cachedUpdate->updateType = resourceArrayData->updateType;
		cachedUpdate->resourceDstBegin = resourceArrayData->resourceDstBegin;
		cachedUpdate->resourceCount = resCount;
		cachedUpdate->resourceHandles = updateCommandsCache->Allocate(sizeOfUnderLyingStruct * resCount);
		
		memcpy(cachedUpdate->resourceHandles, resourceArrayData->resourceHandles, sizeOfUnderLyingStruct * resCount);
		
		argData = cachedUpdate;
		argSize = (sizeOfUnderLyingStruct * resCount) + sizeof(DeviceHandleArrayUpdate);
		break;
	}
	}

	ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle.descriptorManagerIndex);

	ShaderResourceSet* set = descriptorManager->descriptorSets[handle.descriptorSetIndex];

	rducr->bindingindex = bindingindex;
	rducr->updateType = DriverUpdateType::RESOURCEUPDATE;
	rducr->descriptorIdManagerIndex = PACK_DESCRIPTOR_MANAGER_INDEX(handle.descriptorManagerIndex) | PACK_DESCRIPTOR_SET_INDEX(handle.descriptorSetIndex);
	rducr->type = type;
	rducr->cachedDataSize = argSize;
	rducr->data = argData;
	rducr->copies = set->setCount;
}


void RenderInstance::UpdateBufferResourceArray(ShaderResourceSetHandle handle, int bindingindex, ShaderResourceType type, BufferArrayUpdate* resourceArrayData)
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

	ShaderResourceManager* descriptorManager = descriptorManagers.Get(handle.descriptorManagerIndex);

	ShaderResourceSet* set = descriptorManager->descriptorSets[handle.descriptorSetIndex];

	rducr->bindingindex = bindingindex;
	rducr->updateType = DriverUpdateType::RESOURCEUPDATE;
	rducr->descriptorIdManagerIndex = PACK_DESCRIPTOR_MANAGER_INDEX(handle.descriptorManagerIndex) | PACK_DESCRIPTOR_SET_INDEX(handle.descriptorSetIndex);
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

			int descriptorManager = UNPACK_DESCRIPTOR_MANAGER_INDEX(rducr->descriptorIdManagerIndex);

			int descriptorId = UNPACK_DESCRIPTOR_SET_INDEX(rducr->descriptorIdManagerIndex);

			descriptorUpdatePool.Create(descriptorManager, descriptorId, rducr->bindingindex, rducr->type, rducr->data, rducr->cachedDataSize, rducr->copies);
			header = rducr->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandResource);
			break;
		}
		case DriverUpdateType::TRANSFERCOMMAND:
		{
			RenderDriverUpdateCommandFill* rducf = (RenderDriverUpdateCommandFill*)header;
			transferCommandPool.Create(rducf->allocationIndex,  rducf->size, rducf->allocOffset, rducf->fillValue, rducf->copiesWithin);
			header = rducf->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandFill);
			break;
		}
		case DriverUpdateType::IMAGEMEMORYUPDATE:
		{
			RenderDriverUpdateCommandImage* rduci = (RenderDriverUpdateCommandImage*)header;
			imageMemoryUpdateManager.Create(rduci->data, rduci->textureIndex, rduci->totalSize, rduci->width, rduci->height, rduci->mipLevels, rduci->layersCount, rduci->mipStart, rduci->layerStart, rduci->mask);
			header = rduci->GetNext();
			currentSize -= sizeof(RenderDriverUpdateCommandImage);
			break;
		}
		case DriverUpdateType::MEMORYUPDATE:
		{
			RenderDriverUpdateCommandMemory* rducm = (RenderDriverUpdateCommandMemory*)header;

			int truthIndex = rducm->allocationIndex;

			RenderAllocation* alloc = allocations.Get(truthIndex);

			MemoryType bufType = bufferHandles[alloc->memIndex].type;

			if ((bufType & MemoryTypeBits::HOST_MEMORY_TYPE) || (bufType & MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE))
			{
				driverHostMemoryUpdater.Create(rducm->data, rducm->size, rducm->allocationIndex, rducm->allocOffset, rducm->copiesWithin);
			}
			else if (bufType & MemoryTypeBits::DEVICE_MEMORY_TYPE)
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


int RenderInstance::UploadFrameAttachmentResource(int frameGraph, int resourceIndex, int perTextureViewIndex, ShaderResourceSetHandle handle, int bindingIndex, int textureStart)
{
	AttachmentGraphInstance* currentGraphInstance = attachmentGraphsInstances.Get(frameGraph);

	int imageCount = currentGraphInstance->resources[resourceIndex].imageCount;

	DeviceHandleArrayUpdateTextureView* textureIds = (DeviceHandleArrayUpdateTextureView*)cacheAllocator->Allocate(sizeof(DeviceHandleArrayUpdateTextureView) * imageCount);

	for (int i = 0; i < imageCount; i++)
	{
		int textureIndex = currentGraphInstance->resources[resourceIndex].textureIds[0][i];
		textureIds[i].imageHandle = textureIndex;
		textureIds[i].viewIndex = perTextureViewIndex;
	}

	DeviceHandleArrayUpdate update;

	update.updateType = DeviceHandleArrayUpdateType::TEXTURE_VIEW_UPDATE;
	update.resourceCount = imageCount;
	update.resourceDstBegin = textureStart;
	update.resourceHandles = textureIds;

	UpdateShaderResourceArray(handle, bindingIndex, ShaderResourceType::IMAGE2D, &update);

	return imageCount;
}

void RenderInstance::PipelineUpdateIndirectCommandBuffer(int pipelineIndex, int allocationIndex)
{
	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	RenderAllocation alloc = allocations[allocationIndex];
	
	if (handle->group == GRAPHICSO)
		handle->indirectBufferHandle = alloc.memIndex;
}

void RenderInstance::PipelineUpdateVertexBuffer(int pipelineIndex, int allocationIndex, uint32_t vertexCount)
{
	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	if (handle->group == GRAPHICSO)
	{
		handle->vertexBufferHandle = allocationIndex;
		handle->vertexCount = vertexCount;
	}
}

void RenderInstance::PipelineUpdateIndexBuffer(int pipelineIndex, int allocationIndex, uint32_t indexCount, uint32_t indexStride)
{
	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	if (handle->group == GRAPHICSO)
	{
		handle->indexBufferHandle = allocationIndex;
		handle->indexSize = indexStride;
		handle->indexCount = indexCount;
	}
}

void RenderInstance::PipelineUpdateIndirectCountBuffer(int pipelineIndex, int allocationIndex)
{
	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	if (handle->group == GRAPHICSO)
		handle->indirectCountBufferHandle = allocationIndex;
}

void RenderInstance::PipelineUpdateDispatchCommands(int pipelineIndex, uint32_t x, uint32_t y, uint32_t z)
{
	PipelineHandle* handle = pipelineHandles.Get(pipelineIndex);

	if (handle->group == COMPUTESO)
	{
		handle->x = x;
		handle->y = y;
		handle->z = z;
	}
}

int RenderInstance::CreateUniversalBuffer(int deviceSelection, size_t size, MemoryType bufferMemoryType)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int bufferIndex = bufferHandles.Allocate();

	EntryHandle bufferHandle = dev->CreateBuffer(
		size,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		API::ConvertMemoryTypeToVkMemoryPropertyFlags(bufferMemoryType));
	
	bufferHandles.pool[bufferIndex].bufferHandle = bufferHandle;
	bufferHandles.pool[bufferIndex].type = bufferMemoryType;
	bufferHandles.pool[bufferIndex].resourceStatus = resourceStatuses.Allocate();

	return bufferIndex;
}

int RenderInstance::CreateHighLevelInstance(uint32_t vkDriverSpecificMemory, uint32_t vkDriverCacheSize, uint32_t instancePermanentSpecificMemory, uint32_t instanceCacheMemory)
{
	void* driverInstanceDataHead = storageAllocator->Allocate(vkDriverSpecificMemory + vkDriverCacheSize);
	void* instanceDataHead = storageAllocator->Allocate(instancePermanentSpecificMemory + instanceCacheMemory);

	vkInstance->SetInstanceDataAndSize(driverInstanceDataHead, vkDriverSpecificMemory, vkDriverCacheSize);

	VKInstanceDebugData vkDebugData{};

	vkDebugData.userCallback = vulkanDebugCallback;
	vkDebugData.userData = internalRendererLogger;
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

	vkInstance->CreateRenderInstance(instanceDataHead, instancePermanentSpecificMemory, instanceCacheMemory, vkDebugDataTemp, &instanceFeaturesRequest);

	return 0;
}

int RenderInstance::CreateWindowedSurface(OSWindowInternalData* windowData)
{
	int windowAllocIndex = windowsSurfaces.Allocate();

#if defined(_WIN32)
	EntryHandle renderSurfaceIndex = vkInstance->CreateWindowedSurface(windowData->inst, windowData->wnd);
#else
	EntryHandle renderSurfaceIndex = EntryHandle();
#endif

	if (renderSurfaceIndex == EntryHandle())
		return -1;

	RenderWindowSpecificData* winData = windowsSurfaces.Get(windowAllocIndex);

	winData->vkRenderSurface = renderSurfaceIndex;

	return windowAllocIndex;
}

int RenderInstance::CreateDescriptorHeap(int deviceSelection, DescriptorTypes* types, uint32_t* descriptorCountPerFrame, uint32_t numDescriptorTypesCount, uint32_t maxDescriptorSets, uint32_t maxShaderResourceSets)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	int descriptorManagerIndex = descriptorManagers.Allocate();

	ShaderResourceManager* manager = descriptorManagers.Get(descriptorManagerIndex);

	manager->Create(storageAllocator, maxShaderResourceSets);

	DescriptorPoolBuilder builder = dev->CreateDescriptorPoolBuilder(numDescriptorTypesCount, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	for (uint32_t i = 0; i < numDescriptorTypesCount; i++)
	{
		uint32_t individualCount = descriptorCountPerFrame[i];

		switch (types[i])
		{
		case DescriptorTypes::UNIFORM_DESCRIPTOR:
		{
			builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		case DescriptorTypes::UNORDERED_ACCESS_DESCRIPTOR:
		{
			builder.AddStoragePoolSize(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		case DescriptorTypes::SAMPLED_IMAGE_DESCRIPTOR:
		{
			builder.AddSampledImage(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		case DescriptorTypes::STORAGE_IMAGE_DESCRIPTOR:
		{
			builder.AddStorageImage(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		case DescriptorTypes::SAMPLER_DESCRIPTOR:
		{
			builder.AddSampler(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		case DescriptorTypes::COMBINED_IMAGE_SAMPLER_DESCRIPTOR:
		{
			builder.AddImageSamplerCombined(MAX_FRAMES_IN_FLIGHT * individualCount);
			break;
		}
		default:
		{
			break;
		}
		}
	}
	
	manager->deviceResourceHeap = dev->CreateDesciptorPool(&builder, MAX_FRAMES_IN_FLIGHT * maxDescriptorSets);

	return descriptorManagerIndex;
}

void RenderInstance::GeneratePipelineDescriptorBarriers(int deviceSelection, RecordingBufferObject* rcb, ShaderResourceSetHandle* descriptorid, int descriptorcount, BarrierAccumulator* accumulator, int pipelineIndex)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	for (int i = 0; i < descriptorcount; i++)
	{
		ShaderResourceManager* manager = descriptorManagers.Get(descriptorid[i].descriptorManagerIndex);

		ShaderResourceSet* set = manager->descriptorSets[descriptorid[i].descriptorSetIndex];

		int counter = 0;

		int totalBindingCount = set->templateMetaData->bindingCount;

		while (counter < totalBindingCount)
		{
			ShaderResourceArray* header = (ShaderResourceArray*)&set->resourceBindings[counter++];
			switch (header->type)
			{
			case ShaderResourceType::SAMPLERCUBE:
			case ShaderResourceType::SAMPLER2D:
			case ShaderResourceType::SAMPLER3D:
			{
				ShaderResourceCombinedImage* imageBarrier = &header->resourceArray.combinedImages;

				int arrayCount = imageBarrier->textureCount;

				for (int imageIndex = 0; imageIndex < arrayCount; imageIndex++)
				{
					int currImageIndex = imageBarrier->textureDetails[imageIndex].textureHandle;

					int viewIndex = imageBarrier->textureDetails[imageIndex].viewIndex;

					TransitionImageLayout(dev, rcb, currImageIndex, viewIndex, ConvertShaderStageToBarrierStage(header->stage), READ_SHADER_RESOURCE, accumulator, pipelineIndex);
				}
				break;
			}
			case ShaderResourceType::IMAGE2D:
			{
				ShaderResourceImage* imageBarrier = &header->resourceArray.images;

				int arrayCount = imageBarrier->textureCount;

				for (int imageIndex = 0; imageIndex < arrayCount; imageIndex++)
				{
					int currImageIndex = imageBarrier->textureDetails[imageIndex].textureHandle;

					int viewIndex = imageBarrier->textureDetails[imageIndex].viewIndex;

					TransitionImageLayout(dev, rcb, currImageIndex, viewIndex, ConvertShaderStageToBarrierStage(header->stage), READ_SHADER_RESOURCE, accumulator, pipelineIndex);
				}
				break;
			}
			case ShaderResourceType::IMAGESTORE2D:
			{
				ShaderResourceImage* imageBarrier = &header->resourceArray.images;

				int arrayCount = imageBarrier->textureCount;

				for (int imageIndex = 0; imageIndex < arrayCount; imageIndex++)
				{
					int currImageIndex = imageBarrier->textureDetails[imageIndex].textureHandle;

					int viewIndex = imageBarrier->textureDetails[imageIndex].viewIndex;

					TransitionImageLayout(dev, rcb, currImageIndex, viewIndex, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE, accumulator, pipelineIndex);
				}
				break;
			}
			case ShaderResourceType::BUFFER_VIEW:
			case ShaderResourceType::STORAGE_BUFFER:
			case ShaderResourceType::UNIFORM_BUFFER:
			{
				ShaderResourceBuffer* bufferBarrier = (header->type == ShaderResourceType::BUFFER_VIEW) ? (ShaderResourceBuffer*)&header->resourceArray.views : (ShaderResourceBuffer*)&header->resourceArray.buffers;

				int arrayCount = bufferBarrier->bufferCount;

				for (int g = 0; g < arrayCount; g++)
				{
					int allocationIndex = bufferBarrier->allocationIndex[g];

					InsertBufferBarrier(dev, rcb, allocationIndex, ConvertShaderStageToBarrierStage(header->stage), header, pipelineIndex, accumulator);
				}
				break;
			}
			}
		}
	}
}

void RenderInstance::InsertAccumulatedBarriers(RecordingBufferObject* rcb, BarrierAccumulator* accumulator)
{
	RBOPipelineBarrierArgs args{};
	bool insert = false;

	if (accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].barrierCount)
	{
		args.srcStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].srcStage);
		args.dstStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].dstStage);

		args.imageMemoryBarrierCount = accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].barrierCount;
		args.pImageMemoryBarriers = (VkImageMemoryBarrier*)accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].allocator->dataHead;

		insert = true;
	}

	if (accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].barrierCount)
	{
		args.srcStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].srcStage);
		args.dstStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].dstStage);

		args.bufferMemoryBarrierCount = accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].barrierCount;
		args.pBufferMemoryBarriers = (VkBufferMemoryBarrier*)accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator->dataHead;

		insert = true;
	}

	if (insert)
	{
		rcb->BindPipelineBarrierCommand(&args);

		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].barrierCount = accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].dstStage = accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].srcStage = 0;

		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator->Reset();

		accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].barrierCount = accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].srcStage = accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].dstStage = 0;

		accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].allocator->Reset();
	}
}

void RenderInstance::GenerateDrawBindingsBarriers(int deviceSelection, RecordingBufferObject* rcb, PipelineHandle* handle, BarrierAccumulator* accumulator)
{
	RenderLogicalDeviceContainer* deviceContainer = &logicalDeviceIndices[deviceSelection];

	VKDevice* dev = vkInstance->GetLogicalDevice(deviceContainer->logicalDeviceIndex);

	if (handle->vertexBufferHandle != -1)
		InsertBufferBarrier(dev, rcb, handle->vertexBufferHandle, BarrierStageBits::VERTEX_INPUT_BARRIER, BarrierActionBits::READ_VERTEX_INPUT, accumulator);

	if (handle->indirectBufferHandle != -1)
		InsertBufferBarrier(dev, rcb, handle->indirectBufferHandle, BarrierStageBits::INDIRECT_DRAW_BARRIER, BarrierActionBits::READ_INDIRECT_COMMAND, accumulator);

	if (handle->indirectCountBufferHandle != -1)
		InsertBufferBarrier(dev, rcb, handle->indirectCountBufferHandle, BarrierStageBits::INDIRECT_DRAW_BARRIER, BarrierActionBits::READ_INDIRECT_COMMAND, accumulator);
}

void RenderInstance::TransitionImageLayout(VKDevice* dev, RecordingBufferObject* rcb, int imageIndex, int perImageViewIndex, BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator, int pipelineIndex)
{
	RenderTextureDescription* desc = textureResourceHandles.Get(imageIndex);

	ResourceStatus* status = resourceStatuses.Get(desc->resourceStatusIndex);

	RenderImageViewDescription* viewDesc = textureViewsResourceHandles.Get(desc->viewIndex[perImageViewIndex]);

	if (status->resourceType == ResourceStatusType::MANAGED_IMAGE_RESOURCE)
		return;

	int viewMipStart = viewDesc->firstMipLevel;
	int viewMipCount = viewDesc->mipLevelCount;
	int totalMipCount = desc->mipLayers;

	int viewLayerStart = viewDesc->firstLayer;
	int viewLayerCount = viewDesc->layerCount;
	int totalLayerCount = desc->arrayLayers;

	TransitionImageLayout(dev, rcb, desc->textureIndex, viewMipStart, viewMipCount, totalMipCount, viewLayerStart, viewLayerCount, viewDesc->mask, viewDesc->desiredLayoutForView, status, destBarrierStage, destBarrierAction, accumulator, pipelineIndex);
}

void RenderInstance::TransitionImageLayout(VKDevice* dev, RecordingBufferObject* rcb, EntryHandle imageIndex, int mipStart, int mipCount, int totalMipCount, int layerStart, int layerCount,
	ImageViewAspectMask mask, ImageLayout requestedLayout, ResourceStatus* status,
	BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator, int pipelineIndex)
{
	VkImageMemoryBarrier barrier{};

	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = API::ConvertImageViewAspectMaskToVulkanImageAspectFlags(mask);

	barrier.dstAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(destBarrierAction);
	barrier.newLayout = API::ConvertImageLayoutToVulkanImageLayout(requestedLayout);
	barrier.image = dev->GetImageByHandle(imageIndex);

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	struct MipArrayLevel
	{
		int trackedMipStart;
		int trackedMipCount;
		BarrierAction actions;
		BarrierStage stages;
		ImageLayout layout;
		int coalesced;
		struct MipArrayLevel* nextInLevel;
	};

	MipArrayLevel* arrayLevels = (MipArrayLevel*)cacheAllocator->CAllocate(sizeof(MipArrayLevel) * (mipCount * layerCount));

	MipArrayLevel** linkedListPtr = (MipArrayLevel**)cacheAllocator->CAllocate(sizeof(MipArrayLevel*) * layerCount);

	int nodeCount = 0;

	for (int j = layerStart; j < layerStart + layerCount; j++)
	{
		MipArrayLevel* curr = nullptr;

		MipArrayLevel** next = &linkedListPtr[j - layerStart];

		for (int i = mipStart; i < mipStart + mipCount; i++)
		{
			int currentMipArrayIndex = (j * totalMipCount) + i;

			BarrierAction currAction = status->currAction[currentMipArrayIndex];
			BarrierStage currStage = status->currStage[currentMipArrayIndex];
			ImageLayout currLayout = status->currentLayout[currentMipArrayIndex];;

			if ((currLayout != requestedLayout)
				|| (currStage != destBarrierStage)
				|| (currAction != destBarrierAction)
				)
			{
				if (!curr || currLayout != curr->layout)
				{
					if (curr && curr->trackedMipCount)
					{
						*next = curr;
						next = &curr->nextInLevel;
					}

					MipArrayLevel* newLevel = &arrayLevels[nodeCount++];

					newLevel->trackedMipStart = i;
					newLevel->layout = currLayout;
					newLevel->trackedMipCount = 1;
					newLevel->nextInLevel = nullptr;
					newLevel->actions = currAction;
					newLevel->stages = currStage;
					newLevel->coalesced = 0;

					curr = newLevel;
				}
				else
				{
					curr->actions |= currAction;
					curr->stages |= currStage;
					curr->trackedMipCount++;
				}

				status->currentLayout[currentMipArrayIndex] = requestedLayout;
				status->currAction[currentMipArrayIndex] = destBarrierAction;
				status->currStage[currentMipArrayIndex] = destBarrierStage;
			}
			else
			{
				if (curr && curr->trackedMipCount)
				{
					*next = curr;
					next = &curr->nextInLevel;
				}

				curr = nullptr;
			}
		}

		if (curr && curr->trackedMipCount)
		{
			*next = curr;
		}
	}

	//attempt to merge square rectangles with same layout and mip start/count, no splitting

	for (int i = 0; i < layerCount; i++)
	{
		MipArrayLevel* curr = linkedListPtr[i];

		while (curr)
		{
			if (curr->coalesced)
			{
				curr = curr->nextInLevel;
				continue;
			}

			int trackedMipStart = curr->trackedMipStart;
			int trackedMipCount = curr->trackedMipCount;
			int trackedLayerCount = 1;
			int trackedLayerStart = i + layerStart;

			for (int j = i + 1; j < layerCount; j++)
			{
				MipArrayLevel* candidate = linkedListPtr[j];
				int extended = 0;
				while (candidate)
				{
					if (!candidate->coalesced)
					{
						if (candidate->trackedMipStart == trackedMipStart &&
							candidate->trackedMipCount == trackedMipCount &&
							candidate->layout == curr->layout)
						{
							trackedLayerCount++;
							extended = 1;
							candidate->coalesced = 1;

							curr->stages |= candidate->stages;
							curr->actions |= candidate->actions;

							break;
						}
					}
					candidate = candidate->nextInLevel;
				}
				if (!extended)
					break;
			}

			VkImageMemoryBarrier* currentBarrier = nullptr;
			
			if (destBarrierStage & curr->stages)
			{
				currentBarrier = (VkImageMemoryBarrier*)accumulator->intraPassBarrierAllocator.Allocate(sizeof(VkImageMemoryBarrier));
				
				IntraPassBarrier* intraBarrier = GetIntraPassBarrier(accumulator, BarrierType::IMAGE_BARRIER, pipelineIndex, currentBarrier);

				intraBarrier->destStage |= destBarrierStage;
				intraBarrier->srcStage |= curr->stages;
				intraBarrier->barrierCount++;
			}
			else
			{
				currentBarrier = (VkImageMemoryBarrier*)accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].allocator->Allocate(sizeof(VkImageMemoryBarrier));

				accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].srcStage |= curr->stages;
				accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].dstStage |= destBarrierStage;
				accumulator->accumulators[IMAGE_BARRIER_ACCUMULATOR].barrierCount++;
			}

			*currentBarrier = barrier;

			currentBarrier->oldLayout = API::ConvertImageLayoutToVulkanImageLayout(curr->layout);
			currentBarrier->srcAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(curr->actions);
			currentBarrier->subresourceRange.baseMipLevel = trackedMipStart;
			currentBarrier->subresourceRange.levelCount = trackedMipCount;
			currentBarrier->subresourceRange.baseArrayLayer = trackedLayerStart;
			currentBarrier->subresourceRange.layerCount = trackedLayerCount;

			curr = curr->nextInLevel;
		}
	}
}

IntraPassBarrier* RenderInstance::GetIntraPassBarrier(BarrierAccumulator* accum, BarrierType type, int pipelineIndex, void* driverBarrierData)
{
	int topIndex = RENDER_MAX(accum->intraPassCount - 1, 0);

	IntraPassBarrier* intraPassBarrier = &accum->intraPassBarriers[topIndex];

	if (intraPassBarrier->pipelineInst != pipelineIndex || intraPassBarrier->barrierType != type)
	{
		intraPassBarrier = &accum->intraPassBarriers[accum->intraPassCount++];
		intraPassBarrier->pipelineInst = pipelineIndex;
		intraPassBarrier->barrierType = type;
		intraPassBarrier->barrierCount = 0;
		intraPassBarrier->driverSpecificBarriers = driverBarrierData;
		intraPassBarrier->destStage = 0;
		intraPassBarrier->srcStage = 0;
	}

	return intraPassBarrier;
}

void RenderInstance::InsertBufferBarrier(VKDevice* dev, RecordingBufferObject* rcb, int allocationIndex, BarrierStage destBarrierStage, ShaderResourceHeader* header, int pipelineIndex, BarrierAccumulator* accumulator)
{
	size_t size = 0, offset = 0, align = 0;

	int memIndex = -1, resourceStatusIndex = -1, bufferLastAccessFrame = 0;

	AllocationType allocType;

	VkBufferMemoryBarrier* vkBarrier = nullptr;

	RenderAllocation* alloc = allocations.Get(allocationIndex);

	resourceStatusIndex = alloc->resourceStatus;

	allocType = alloc->allocType;

	ResourceStatus* status = resourceStatuses.Get(resourceStatusIndex);

	if (allocType == AllocationType::PERFRAME)
		bufferLastAccessFrame = currentFrame;

	if 
	(
		(status->currAction[bufferLastAccessFrame] & (BarrierActionBits::READ_SHADER_RESOURCE | BarrierActionBits::READ_UNIFORM_BUFFER))
		&& !(status->currAction[bufferLastAccessFrame] & BarrierActionBits::WRITE_SHADER_RESOURCE)
		&& (header->action == ShaderResourceAction::SHADERREAD)
		&& (status->currStage[bufferLastAccessFrame] & destBarrierStage)
	)
	{
		return;
	}

	memIndex = alloc->memIndex;

	align = alloc->alignment;

	size = ((alloc->requestedSize * alloc->structureCopies) + align - 1) & ~(align - 1);

	offset = alloc->offset;

	if (allocType == AllocationType::PERFRAME)
	{
		size_t strideSize = size;

		offset += (currentFrame * strideSize);
	}

	if (destBarrierStage & status->currStage[bufferLastAccessFrame])
	{
		vkBarrier = (VkBufferMemoryBarrier*)accumulator->intraPassBarrierAllocator.Allocate(sizeof(VkBufferMemoryBarrier));

		IntraPassBarrier* intraBarrier = GetIntraPassBarrier(accumulator, BarrierType::BUFFER_BARRIER, pipelineIndex, vkBarrier);

		intraBarrier->destStage |= destBarrierStage;
		intraBarrier->srcStage |= status->currStage[bufferLastAccessFrame];
		intraBarrier->barrierCount++;
	}
	else
	{
		vkBarrier = (VkBufferMemoryBarrier*)accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator->Allocate(sizeof(VkBufferMemoryBarrier));

		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].srcStage |= status->currStage[bufferLastAccessFrame];
		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].dstStage |= destBarrierStage;
		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].barrierCount++;
	}

	VkBuffer buffer = dev->GetBufferHandle(bufferHandles[memIndex].bufferHandle);

	BarrierAction newAction = 0;

	if (header->type == ShaderResourceType::UNIFORM_BUFFER)
		newAction = BarrierActionBits::READ_UNIFORM_BUFFER;
	else
	{
		if (header->action == ShaderResourceAction::SHADERWRITE)
			newAction = BarrierActionBits::WRITE_SHADER_RESOURCE;
		else if (header->action == ShaderResourceAction::SHADERREADWRITE)
			newAction = BarrierActionBits::READ_SHADER_RESOURCE | BarrierActionBits::WRITE_SHADER_RESOURCE;
		else
			newAction = BarrierActionBits::READ_SHADER_RESOURCE;
	}

	vkBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	vkBarrier->pNext = nullptr;
	vkBarrier->srcAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(status->currAction[bufferLastAccessFrame]);
	vkBarrier->dstAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(newAction);
	vkBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkBarrier->buffer = buffer;
	vkBarrier->offset = offset;
	vkBarrier->size = size;

	status->currStage[bufferLastAccessFrame] = destBarrierStage;
	status->currAction[bufferLastAccessFrame] = newAction;
}

void RenderInstance::InsertBufferBarrier(VKDevice* dev, RecordingBufferObject* rcb, int allocationIndex, BarrierStage destBarrierStage, BarrierAction destBarrierAction, BarrierAccumulator* accumulator)
{
	RenderAllocation* bufferAlloc = allocations.Get(allocationIndex);

	ResourceStatus* status = resourceStatuses.Get(bufferAlloc->resourceStatus);

	int resourceIndexToUpdate = 0;

	if (bufferAlloc->allocType == AllocationType::PERFRAME)
		resourceIndexToUpdate = currentFrame;

	if (status->currStage[resourceIndexToUpdate] != destBarrierStage ||
		status->currAction[resourceIndexToUpdate] != destBarrierAction)
	{
		size_t align = bufferAlloc->alignment;

		size_t copiesOfstruct = static_cast<size_t>(bufferAlloc->structureCopies);

		size_t bufferSize = ((bufferAlloc->requestedSize * copiesOfstruct) + (align - 1)) & ~(align - 1);

		size_t bufferBaseOffset = bufferAlloc->offset;

		int bufferIndex = bufferAlloc->memIndex;

		if (bufferAlloc->allocType == AllocationType::PERFRAME)
		{
			size_t perFrameBufferOffset = (((bufferAlloc->requestedSize * copiesOfstruct) + (align - 1)) & ~(align - 1));

			perFrameBufferOffset *= currentFrame;

			bufferBaseOffset += perFrameBufferOffset;
		}

		VkBufferMemoryBarrier* vkBarrier = (VkBufferMemoryBarrier*)accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].allocator->Allocate(sizeof(VkBufferMemoryBarrier));

		VkBuffer buffer = dev->GetBufferHandle(bufferHandles[bufferIndex].bufferHandle);

		vkBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		vkBarrier->pNext = nullptr;
		vkBarrier->srcAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(status->currAction[resourceIndexToUpdate]);
		vkBarrier->dstAccessMask = API::ConvertBarrierActionToVulkanAccessFlags(destBarrierAction);
		vkBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkBarrier->buffer = buffer;
		vkBarrier->offset = bufferBaseOffset;
		vkBarrier->size = bufferSize;

		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].srcStage |= status->currStage[resourceIndexToUpdate];
		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].dstStage |= destBarrierStage;
		accumulator->accumulators[BUFFER_BARRIER_ACCUMULATOR].barrierCount++;

		status->currAction[resourceIndexToUpdate] = destBarrierAction;
		status->currStage[resourceIndexToUpdate] = destBarrierStage;
	}
}

void RenderInstance::InsertIntraPassBarrier(RecordingBufferObject* rbo, BarrierAccumulator* accum, int pipelineIndex)
{
	if (accum->intraPassTop == accum->intraPassCount)
		return;

	IntraPassBarrier* ipb = &accum->intraPassBarriers[accum->intraPassTop];

	while(accum->intraPassTop < accum->intraPassCount && ipb->pipelineInst == pipelineIndex)
	{
		RBOPipelineBarrierArgs args{};
		args.srcStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(ipb->srcStage);
		args.dstStageMask |= API::ConvertBarrierStageToVulkanPipelineStage(ipb->destStage);

		if (ipb->barrierType == BarrierType::IMAGE_BARRIER)
		{
			args.imageMemoryBarrierCount = ipb->barrierCount;
			args.pImageMemoryBarriers = (VkImageMemoryBarrier*)ipb->driverSpecificBarriers;

		} 
		else if (ipb->barrierType == BarrierType::BUFFER_BARRIER)
		{
			args.bufferMemoryBarrierCount = ipb->barrierCount;
			args.pBufferMemoryBarriers = (VkBufferMemoryBarrier*)ipb->driverSpecificBarriers;
		}
	
		rbo->BindPipelineBarrierCommand(&args);

		accum->intraPassTop++;

		ipb = &accum->intraPassBarriers[accum->intraPassTop];
	}
}
