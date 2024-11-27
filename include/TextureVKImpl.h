#pragma once
#include <numeric>

#include "RenderInstance.h"
#include "SMBTexture.h"
class TextureVKImpl
{
public:
	TextureVKImpl(SMBTexture& tex) :
		image(VK_NULL_HANDLE),
		imageView(VK_NULL_HANDLE),
		sampler(VK_NULL_HANDLE)
	{
		CreateImageResources(tex);
		CreateImageViews(tex);
		CreateImageSampler(tex);
	};

	~TextureVKImpl()
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();

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

	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	VkSampler sampler;
	VkFormat format;

	void CreateImageResources(SMBTexture& tex)
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();
		VkPhysicalDevice gpu = ::VK::Renderer::gRenderInstance->GetVulkanPhysicalDevice();
		VkQueue queue = ::VK::Renderer::gRenderInstance->GetGraphicsQueue();
		VkCommandPool pool = ::VK::Renderer::gRenderInstance->GetVulkanCommandPool();

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

		std::tie(image, imageMemory) = ::VK::Utils::CreateImage(device, gpu, imageInfo);

		::VK::Utils::TransitionImageLayout(device, pool, queue, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, tex.miplevels, 1);

		VkDeviceSize offset = 0U;

		for (auto i = 0U; i < tex.miplevels; i++) {

			::VK::Utils::CopyBufferToImage(device, pool, queue, stagingBuffer, image, tex.width >> i, tex.height >> i, i, offset, {0, 0, 0});

			offset += static_cast<VkDeviceSize>(sizes[i]);
		}

		::VK::Utils::TransitionImageLayout(device, pool, queue, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, tex.miplevels, 1);
	
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	void CreateImageViews(SMBTexture& tex)
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();

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

	void CreateImageSampler(SMBTexture &tex)
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();
		VkPhysicalDevice gpu = ::VK::Renderer::gRenderInstance->GetVulkanPhysicalDevice();

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
#if 0
	void TransitionMips(VkDevice& device, VkPhysicalDevice &gpu, VkCommandPool &pool, VkQueue &queue, uint32_t width, uint32_t height, uint32_t mips)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(gpu, format, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkCommandBuffer commandBuffer = ::VK::Utils::BeginOneTimeCommands(device, pool);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = static_cast<int32_t>(width);
		int32_t mipHeight = static_cast<int32_t>(height);

		for (auto i = 1U; i < mips; i++)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);


			if (mipWidth > 1) mipWidth >>= 2;
			if (mipHeight > 1) mipHeight >>= 2;
		}

		barrier.subresourceRange.baseMipLevel = mips - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		::VK::Utils::EndOneTimeCommands(device, queue, pool, commandBuffer);
	}
#endif
private:
	VkFormat ConvertSMBToVkFormat(SMBImageFormat format)
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
};

