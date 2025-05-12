#include "VKTexture.h"

VKTexture::VKTexture(SMBTexture& tex) :
	imageIndex(~0U),
	viewIndex(~0U),
	sampler(VK_NULL_HANDLE)
{
	CreateImageResources(tex.data, tex.imageSizes, tex.width, tex.height, tex.miplevels, tex.type);
	CreateImageViews(tex.miplevels, tex.type);
	CreateImageSampler(tex.miplevels);
};

VKTexture::VKTexture(AppTextureImpl& tex) :
	imageIndex(~0U),
	viewIndex(~0U),
	sampler(VK_NULL_HANDLE)
{
	std::vector<std::vector<char>>imageVec(1, tex.data);
	std::vector<uint32_t> imageSize(1, tex.dataSize);
	CreateImageResources(imageVec ,imageSize, tex.width, tex.height, tex.miplevels, tex.type);
	CreateImageViews(tex.miplevels, tex.type);
	CreateImageSampler(tex.miplevels);
}

VKTexture::VKTexture(VKTexture&& other) noexcept
{
	this->imageIndex = other.imageIndex;
	this->viewIndex = other.viewIndex;
	this->sampler = other.sampler;
	other.imageIndex = ~0U;
	other.viewIndex = ~0U;
	other.sampler = VK_NULL_HANDLE;
}

VKTexture::VKTexture(VKTexture& other) noexcept
{
	this->imageIndex = other.imageIndex;
	this->viewIndex = other.viewIndex;
	this->sampler = other.sampler;
	other.imageIndex = ~0U;
	other.viewIndex = ~0U;
	other.sampler = VK_NULL_HANDLE;
}

VKTexture::~VKTexture()
{
	VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();

	if (sampler)
	{
		vkDestroySampler(device, sampler, nullptr);
	}
}

void VKTexture::DeleteResources()
{
	VKRenderer::gRenderInstance->DeleteVulkanImage(imageIndex);
	VKRenderer::gRenderInstance->DeleteVulkanImage(viewIndex);
}

void VKTexture::CreateImageResources(std::vector<std::vector<char>>& imageData, std::vector<uint32_t>& imageSizes,
	uint32_t width, uint32_t height, uint32_t mipLevels, ImageFormat type)
{	
	imageIndex = VKRenderer::gRenderInstance->CreateVulkanImage(imageData, imageSizes, width, height, mipLevels, type);
}

void VKTexture::CreateImageSampler(uint32_t mipLevels)
{
	VkDevice device =  VKRenderer::gRenderInstance->GetVulkanDevice();
	VkPhysicalDevice gpu = VKRenderer::gRenderInstance->GetVulkanPhysicalDevice();

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(gpu, &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void VKTexture::CreateImageViews(uint32_t miplevels, ImageFormat type)
{
	viewIndex = VKRenderer::gRenderInstance->CreateVulkanImageView(imageIndex, miplevels, type, VK_IMAGE_ASPECT_COLOR_BIT);
}
