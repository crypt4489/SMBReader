#pragma once

#include "vulkan/vulkan.h"

namespace VK 
{
	namespace Utils 
	{
		struct SwapChainSupportDetails 
		{
			VkSurfaceFormatKHR* formats;
			VkPresentModeKHR* presentModes;
			uint32_t formatCount;
			uint32_t presentModeCount;
			VkSurfaceCapabilitiesKHR capabilities{};
		};

		const char* ConvertVkResultString(VkResult result);

		VkDeviceSize GetRawImageSizeFromFormat(VkFormat format, uint32_t width, uint32_t height);

		int querySwapChainSupportCount(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t* formatsDataSpaceCount, uint32_t* presentModesDataSpaceCount, VkResult results[2]);

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR* formatsDataSpace, VkPresentModeKHR* presentModesDataSpace);

		uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		VkFormat findSupportedFormat(VkPhysicalDevice gpu, VkFormat* candidates, size_t candidateSize, VkImageTiling tiling, VkFormatFeatureFlags features);

		int CreateShaderModule(VkDevice device, char* code, size_t size, VkShaderModule* outModule, VkResult* outResult);

		int CreateBuffer(
			VkDevice device, VkPhysicalDevice physicalDevice,
			VkDeviceSize bufferSize,
			VkMemoryPropertyFlags memprops, VkSharingMode sharingMode,
			VkBufferUsageFlags usage, 
			VkBuffer* outBufferHandle, VkDeviceMemory* outDeviceMemory, 
			VkResult* outResult
		);

		VkVertexInputBindingDescription CreateVertexInputBindingDescription(uint32_t vertexBufferLocation, size_t stride);

		VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool pool);

		void EndOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer commandBuffer);

		void EndOneTimeCommandsSemaphores(VkDevice device, VkQueue queue,
			VkCommandPool pool, VkCommandBuffer commandBuffer, VkSemaphore* semas, VkPipelineStageFlags* flags,
			uint32_t semasCount);

		namespace MultiCommands
		{
			void CopyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset, int copies, size_t stride);

			void CopyBufferBatch(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkBufferCopy* copyRegions, int numberOfCopies);

			void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
				VkOffset3D offset, uint32_t baseLayer, uint32_t layerCount, VkImageAspectFlags aspectMask);

			void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset, VkDeviceSize bufferSize,
				VkOffset3D offset, uint32_t baseLayer, uint32_t layerCount, VkBufferImageCopy* regions);


			int TransitionImageLayout(VkCommandBuffer commandBuffer,
					VkImage image, VkFormat format, VkImageLayout oldLayout,
					VkImageLayout newLayout, uint32_t mips, uint32_t layers);
		}

		inline bool operator==(const VkExtent2D extent1, const VkExtent2D extent2)
		{
			return (extent1.width == extent2.width) && (extent1.height == extent2.height);
		}
	}
}
