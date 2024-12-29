#include "VKTexture.h"

VKTexture::VKTexture(SMBTexture& tex) :
	image(VK_NULL_HANDLE),
	imageView(VK_NULL_HANDLE),
	sampler(VK_NULL_HANDLE)
{
	CreateImageResources(tex);
	CreateImageViews(tex);
	CreateImageSampler(tex);
};

VKTexture::VKTexture(VKTexture&& other) noexcept
{
	this->image = other.image;
	this->imageMemory = other.imageMemory;
	this->imageView = other.imageView;
	this->sampler = other.sampler;
	this->format = other.format;
	other.image = VK_NULL_HANDLE;
	other.imageMemory = VK_NULL_HANDLE;
	other.imageView = VK_NULL_HANDLE;
	other.sampler = VK_NULL_HANDLE;
}

VKTexture::VKTexture(VKTexture& other) noexcept
{
	this->image = other.image;
	this->imageMemory = other.imageMemory;
	this->imageView = other.imageView;
	this->sampler = other.sampler;
	this->format = other.format;
	other.image = VK_NULL_HANDLE;
	other.imageMemory = VK_NULL_HANDLE;
	other.imageView = VK_NULL_HANDLE;
	other.sampler = VK_NULL_HANDLE;
}

VKTexture::~VKTexture()
{
	VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();

	if (sampler)
	{
		vkDestroySampler(device, sampler, nullptr);
	}

	if (imageView)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}

	if (imageMemory)
	{
		vkFreeMemory(device, imageMemory, nullptr);
	}

	if (image)
	{
		vkDestroyImage(device, image, nullptr);
	}
}

void VKTexture::CreateImageResources(SMBTexture& tex)
{
	VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();
	VkPhysicalDevice gpu = VKRenderer::gRenderInstance->GetVulkanPhysicalDevice();
	auto transferQueueData = VKRenderer::gRenderInstance->GetTransferQueue();

	VkCommandPool pool;
	VkQueue queue;
	int32_t queueNum;

	std::tie(queue, pool, queueNum) = transferQueueData.value();

	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(std::accumulate(tex.imageSizes.begin(), tex.imageSizes.end(), 0));

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	std::tie(stagingBuffer, stagingMemory) = ::VK::Utils::createBuffer(device, gpu, imagesSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	char* data;
	auto& sizes = tex.imageSizes;
	auto& pixels = tex.data;
	vkMapMemory(device, stagingMemory, 0, imagesSize, 0, reinterpret_cast<void**>(&data));
	for (auto i = 0U; i < tex.miplevels; i++) {
		std::memcpy(data, pixels[i].data(), sizes[i]);
		data += sizes[i];
	}
	vkUnmapMemory(device, stagingMemory);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = tex.width;
	imageInfo.extent.height = tex.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = tex.miplevels;
	imageInfo.arrayLayers = 1;

	format = imageInfo.format = ConvertSMBToVkFormat(tex.type);
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	std::tie(image, imageMemory) = ::VK::Utils::CreateImage(device, gpu, imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	auto& sema = VKRenderer::gRenderInstance->GetTransferSemaphore();

	SemaphoreGuard lock(sema);

	VkCommandBuffer cb = ::VK::Utils::BeginOneTimeCommands(device, pool);

	::VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex.miplevels, 1);

	VkDeviceSize offset = 0U;

	for (auto i = 0U; i < tex.miplevels; i++) {

		::VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, image, tex.width >> i, tex.height >> i, i, offset, { 0, 0, 0 });

		offset += static_cast<VkDeviceSize>(sizes[i]);
	}

	::VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tex.miplevels, 1);

	::VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	VKRenderer::gRenderInstance->ReturnTranferQueue(queueNum);

	vkFreeMemory(device, stagingMemory, nullptr);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
}

void VKTexture::CreateImageSampler(SMBTexture& tex)
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
	samplerInfo.maxLod = static_cast<float>(tex.miplevels);

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void VKTexture::CreateImageViews(SMBTexture& tex)
{
	VkDevice device = VKRenderer::gRenderInstance->GetVulkanDevice();

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = tex.miplevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	imageView = ::VK::Utils::CreateImageView(device, viewInfo);
}

VkFormat VKTexture::ConvertSMBToVkFormat(SMBImageFormat format)
{
	VkFormat vkFormat = VK_FORMAT_MAX_ENUM;
	switch (format)
	{
	case SMBImageFormat::DXT1:
		vkFormat = VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		break;
	case SMBImageFormat::DXT3:
		vkFormat = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	case SMBImageFormat::R8G8B8A8:
		vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	}
	return vkFormat;
}