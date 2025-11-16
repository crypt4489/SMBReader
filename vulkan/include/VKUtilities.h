#pragma once

#include "vulkan/vulkan.h"
#include <iostream>
#include <vector>


namespace VK {

	namespace Utils {

		struct SwapChainSupportDetails {
				VkSurfaceCapabilitiesKHR capabilities{};
				std::vector<VkSurfaceFormatKHR> formats;
				std::vector<VkPresentModeKHR> presentModes;
		};

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceProperties& props);


		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceMemoryProperties& props);

		std::ostream& operator<<(std::ostream& os, const VkQueueFamilyProperties& props);




		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData);


		extern VkDebugUtilsMessengerEXT debugMessenger;

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator);

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator);

		uint32_t findMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		VkFormat findSupportedFormat(VkPhysicalDevice& gpu, VkFormat* candidates, size_t candidateSize, VkImageTiling tiling, VkFormatFeatureFlags features);

		VkShaderModule createShaderModule(VkDevice& device, char* code, size_t size);

		std::pair<VkBuffer, VkDeviceMemory> createBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice,
				VkDeviceSize bufferSize,
				VkMemoryPropertyFlags memprops, VkSharingMode sharingMode,
				VkBufferUsageFlags usage);

		VkCommandBuffer BeginOneTimeCommands(VkDevice& device, VkCommandPool& pool);

		void EndOneTimeCommands(VkDevice& device, VkQueue& queue, VkCommandPool& pool, VkCommandBuffer commandBuffer);

		void EndOneTimeCommandsSemaphores(VkDevice& device, VkQueue& queue,
			VkCommandPool& pool, VkCommandBuffer commandBuffer, VkSemaphore* semas, VkPipelineStageFlags* flags,
			uint32_t semasCount);

		void CopyBuffer(VkDevice& device, VkCommandPool& pool, VkQueue& queue, VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset);

		std::pair<VkImage, VkDeviceMemory> CreateImage(VkDevice& device, VkPhysicalDevice& gpu, VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties);

		VkImageView CreateImageView(VkDevice& device, VkImageViewCreateInfo& viewInfo);

		void CopyBufferToImage(VkDevice& device, VkCommandPool& pool, VkQueue& queue, VkBuffer& buffer, VkImage& image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
				VkOffset3D offset);

		void TransitionImageLayout(VkDevice& device, VkCommandPool& pool, VkQueue& queue,
				VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mips, uint32_t layers);

		namespace MultiCommands
		{

			void CopyBufferToImage(VkCommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image,
					uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
					VkOffset3D offset);



			void TransitionImageLayout(VkCommandBuffer& commandBuffer,
					VkImage& image, VkFormat format, VkImageLayout oldLayout,
					VkImageLayout newLayout, uint32_t mips, uint32_t layers);
		}

		inline bool operator==(const VkExtent2D &extent1, const VkExtent2D &extent2)
		{
			return (extent1.width == extent2.width) && (extent1.height == extent2.height);
		}
						

	}
	
}
