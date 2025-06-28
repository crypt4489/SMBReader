#include "VKTexture.h"

VKTexture::VKTexture(SMBTexture& tex) :
	imageIndex(),
	viewIndex(),
	samplerIndex()
{
	CreateImageResources(tex.data, tex.imageSizes, tex.width, tex.height, tex.miplevels, tex.type);
	CreateImageViews(tex.miplevels, tex.type);
	CreateImageSampler(tex.miplevels);
};

VKTexture::VKTexture(AppTextureImpl& tex) :
	imageIndex(),
	viewIndex(),
	samplerIndex()
{
	std::vector<std::vector<char>>imageVec(1, tex.data);
	std::vector<uint32_t> imageSize(1, tex.dataSize);
	CreateImageResources(imageVec ,imageSize, tex.width, tex.height, tex.miplevels, tex.type);
	CreateImageViews(tex.miplevels, tex.type);
	CreateImageSampler(tex.miplevels);
}

VKTexture::VKTexture(VKTexture&& other) noexcept
{
	this->imageIndex = std::move(other.imageIndex);
	this->viewIndex = std::move(other.viewIndex);
	this->samplerIndex = std::move(other.samplerIndex);
	//other.imageIndex = ~0U;
	//other.viewIndex = ~0U;
	//other.samplerIndex = ~0U;
}

//VKTexture::VKTexture(VKTexture& other) noexcept
//{
	//this->imageIndex = std::move(other.imageIndex);
	//this->viewIndex = other.viewIndex;
//	this->samplerIndex = other.samplerIndex;
	//other.imageIndex = ~0U;
	//other.viewIndex = ~0U;
	//other.samplerIndex = ~0U;
//}

VKTexture::~VKTexture()
{

}

void VKTexture::DeleteResources()
{
	VKRenderer::gRenderInstance->DeleteVulkanImage(imageIndex);
	VKRenderer::gRenderInstance->DeleteVulkanImageView(viewIndex);
	VKRenderer::gRenderInstance->DeleteVulkanSampler(samplerIndex);
}

void VKTexture::CreateImageResources(std::vector<std::vector<char>>& imageData, std::vector<uint32_t>& imageSizes,
	uint32_t width, uint32_t height, uint32_t mipLevels, ImageFormat type)
{	
	imageIndex = VKRenderer::gRenderInstance->CreateVulkanImage(imageData, imageSizes, width, height, mipLevels, type);
}

void VKTexture::CreateImageSampler(uint32_t mipLevels)
{
	samplerIndex = VKRenderer::gRenderInstance->CreateVulkanSampler(mipLevels);
}

void VKTexture::CreateImageViews(uint32_t miplevels, ImageFormat type)
{
	viewIndex = VKRenderer::gRenderInstance->CreateVulkanImageView(imageIndex, miplevels, type, VK_IMAGE_ASPECT_COLOR_BIT);
}
