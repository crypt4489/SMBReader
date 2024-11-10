#pragma once
#include <vulkan/vulkan.h>

#include "ProgramArgs.h"
#include "RenderInstance.h"
#include "Texture.h"
class TextureVKImpl
{
public:
	TextureVKImpl(Texture &tex) :  
		image(VK_NULL_HANDLE), 
		imageView(VK_NULL_HANDLE), 
		sampler(VK_NULL_HANDLE) 
	{
		CreateImageResources(tex);
	};
	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;
	VkSampler sampler;

	void CreateImageResources(Texture &tex)
	{
		VkDevice device = ::VK::Renderer::gRenderInstance->GetVulkanDevice();
		VkPhysicalDevice gpu = ::VK::Renderer::gRenderInstance->GetVulkanPhysicalDevice();

		VkDeviceSize imageSize = 0;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;

		std::tie(stagingBuffer, stagingMemory) = ::VK::Utils::createBuffer(device, gpu, imageSize,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		void* data;
		vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data);
		std::memcpy(data, tex.data.data(), imageSize);
		vkUnmapMemory(device, stagingMemory);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = tex.width;
		imageInfo.extent.height = tex.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;

		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
	}
};

