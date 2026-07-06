#include "pch.h"
#include "VKUtilities.h"

#include <array>

namespace VK {
	namespace Utils {

		const char* ConvertVkResultString(VkResult result)
		{
			const char* strOutput = "";
			switch (result)
			{
				// Success codes
			case VK_SUCCESS:
				strOutput = "VK_SUCCESS";
				break;
			case VK_NOT_READY:
				strOutput = "VK_NOT_READY";
				break;
			case VK_TIMEOUT:
				strOutput = "VK_TIMEOUT";
				break;
			case VK_EVENT_SET:
				strOutput = "VK_EVENT_SET";
				break;
			case VK_EVENT_RESET:
				strOutput = "VK_EVENT_RESET";
				break;
			case VK_INCOMPLETE:
				strOutput = "VK_INCOMPLETE";
				break;
			case VK_SUBOPTIMAL_KHR:
				strOutput = "VK_SUBOPTIMAL_KHR";
				break;
			case VK_THREAD_IDLE_KHR:
				strOutput = "VK_THREAD_IDLE_KHR";
				break;
			case VK_THREAD_DONE_KHR:
				strOutput = "VK_THREAD_DONE_KHR";
				break;
			case VK_OPERATION_DEFERRED_KHR:
				strOutput = "VK_OPERATION_DEFERRED_KHR";
				break;
			case VK_OPERATION_NOT_DEFERRED_KHR:
				strOutput = "VK_OPERATION_NOT_DEFERRED_KHR";
				break;
			case VK_PIPELINE_COMPILE_REQUIRED:
				strOutput = "VK_PIPELINE_COMPILE_REQUIRED";
				break;

				// Error codes
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				strOutput = "VK_ERROR_OUT_OF_HOST_MEMORY";
				break;
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:
				strOutput = "VK_ERROR_OUT_OF_DEVICE_MEMORY";
				break;
			case VK_ERROR_INITIALIZATION_FAILED:
				strOutput = "VK_ERROR_INITIALIZATION_FAILED";
				break;
			case VK_ERROR_DEVICE_LOST:
				strOutput = "VK_ERROR_DEVICE_LOST";
				break;
			case VK_ERROR_MEMORY_MAP_FAILED:
				strOutput = "VK_ERROR_MEMORY_MAP_FAILED";
				break;
			case VK_ERROR_LAYER_NOT_PRESENT:
				strOutput = "VK_ERROR_LAYER_NOT_PRESENT";
				break;
			case VK_ERROR_EXTENSION_NOT_PRESENT:
				strOutput = "VK_ERROR_EXTENSION_NOT_PRESENT";
				break;
			case VK_ERROR_FEATURE_NOT_PRESENT:
				strOutput = "VK_ERROR_FEATURE_NOT_PRESENT";
				break;
			case VK_ERROR_INCOMPATIBLE_DRIVER:
				strOutput = "VK_ERROR_INCOMPATIBLE_DRIVER";
				break;
			case VK_ERROR_TOO_MANY_OBJECTS:
				strOutput = "VK_ERROR_TOO_MANY_OBJECTS";
				break;
			case VK_ERROR_FORMAT_NOT_SUPPORTED:
				strOutput = "VK_ERROR_FORMAT_NOT_SUPPORTED";
				break;
			case VK_ERROR_FRAGMENTED_POOL:
				strOutput = "VK_ERROR_FRAGMENTED_POOL";
				break;
			case VK_ERROR_UNKNOWN:
				strOutput = "VK_ERROR_UNKNOWN";
				break;
			case VK_ERROR_OUT_OF_POOL_MEMORY:
				strOutput = "VK_ERROR_OUT_OF_POOL_MEMORY";
				break;
			case VK_ERROR_INVALID_EXTERNAL_HANDLE:
				strOutput = "VK_ERROR_INVALID_EXTERNAL_HANDLE";
				break;
			case VK_ERROR_FRAGMENTATION:
				strOutput = "VK_ERROR_FRAGMENTATION";
				break;
			case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
				strOutput = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
				break;
			case VK_ERROR_SURFACE_LOST_KHR:
				strOutput = "VK_ERROR_SURFACE_LOST_KHR";
				break;
			case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
				strOutput = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
				break;
			case VK_ERROR_OUT_OF_DATE_KHR:
				strOutput = "VK_ERROR_OUT_OF_DATE_KHR";
				break;
			case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
				strOutput = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
				break;
			case VK_ERROR_VALIDATION_FAILED_EXT:
				strOutput = "VK_ERROR_VALIDATION_FAILED_EXT";
				break;
			case VK_ERROR_INVALID_SHADER_NV:
				strOutput = "VK_ERROR_INVALID_SHADER_NV";
				break;
			case VK_ERROR_NOT_PERMITTED_KHR:
				strOutput = "VK_ERROR_NOT_PERMITTED_KHR";
				break;
			case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
				strOutput = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
				break;

			case VK_RESULT_MAX_ENUM:
			default:
				break;
			}

			return strOutput;
		}

		VkDeviceSize GetRawImageSizeFromFormat(VkFormat format, uint32_t width, uint32_t height)
		{
			VkDeviceSize size = 0;
			switch (format)
			{
			case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
			{
				int ret = 0;
				unsigned long blockCountX = (width + 3) / 4;
				unsigned long blockCountY = (height + 3) / 4;
				size = blockCountX * 8 * blockCountY;
				break;
			}
			case VK_FORMAT_BC3_SRGB_BLOCK:
			{
				int ret = 0;
				unsigned long blockCountX = (width + 3) / 4;
				unsigned long blockCountY = (height + 3) / 4;
				size = blockCountX * 16 * blockCountY;
				break;
			}
			case VK_FORMAT_R8G8B8A8_SRGB:
				size = width * height * 4;
				break;
			case VK_FORMAT_R8G8B8A8_UNORM:
				size = width * height * 4;
				break;
			case VK_FORMAT_B8G8R8A8_UNORM:
				size = width * height * 4;
				break;
			case VK_FORMAT_B8G8R8A8_SRGB:
				size = width * height * 4;
				break;
			case VK_FORMAT_D24_UNORM_S8_UINT:
				size = width * height * 4;
				break;
			case VK_FORMAT_D32_SFLOAT_S8_UINT:
				size = width * height * 5;
				break;
			case VK_FORMAT_D32_SFLOAT:
				size = width * height * 4;
				break;
			default:
				break;
			}

			return size;
		}

		int querySwapChainSupportCount(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t* formatsDataSpaceCount, uint32_t* presentModesDataSpaceCount, VkResult results[2])
		{
			results[0] = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, formatsDataSpaceCount, nullptr);
			
			results[1] = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, presentModesDataSpaceCount, nullptr);

			if (results[0] != VK_SUCCESS || !*formatsDataSpaceCount)
			{
				return -1;
			}

			if (results[1] != VK_SUCCESS || !*presentModesDataSpaceCount)
			{
				return -2;
			}

			return 0;
		}

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR* formatsDataSpace, VkPresentModeKHR* presentModesDataSpace) 
		{
			SwapChainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount) {
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formatsDataSpace);
			}

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

			if (presentModeCount) {
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModesDataSpace);
			}

			details.formatCount = formatCount;
			details.presentModeCount = presentModeCount;
			details.formats = formatsDataSpace;
			details.presentModes = presentModesDataSpace;

			return details;
		}

		uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}

			return ~0U;
		}

		VkFormat findSupportedFormat(VkPhysicalDevice gpu, VkFormat* candidates, size_t candidateSize, VkImageTiling tiling, VkFormatFeatureFlags features)
		{
			for (size_t i = 0; i < candidateSize; i++)
			{
				auto format = candidates[i];
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(gpu, format, &props);
				if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
					return format;
				}
				else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
					return format;
				}
			}

			return VK_FORMAT_UNDEFINED;
		}

		int CreateShaderModule(VkDevice device, char* code, size_t size, VkShaderModule* outModule, VkResult* outResult)
		{
			VkShaderModuleCreateInfo createInfo{};

			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = size;;
			createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

			VkShaderModule shaderModule;

			VkResult result = VK_SUCCESS;

			if ((result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule)) != VK_SUCCESS) {
				*outResult = result;
				return -1;
			}

			*outModule = shaderModule;

			return 0;
		}

		int CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
			VkDeviceSize bufferSize,
			VkMemoryPropertyFlags memprops, VkSharingMode sharingMode,
			VkBufferUsageFlags usage, VkBuffer* outBufferHandle, VkDeviceMemory* outDeviceMemory, VkResult* outResult)
		{
			VkBuffer buffer;
			VkDeviceMemory bufferMemory;
			VkResult result = VK_SUCCESS;

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = bufferSize;
			bufferInfo.sharingMode = sharingMode;
			bufferInfo.usage = usage;

			if ((result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer)) != VK_SUCCESS) {
				*outResult = result;
				return -1;
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memprops);

			if ((result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory)) != VK_SUCCESS) {
				vkDestroyBuffer(device, buffer, nullptr);
				*outResult = result;
				return -2;
			}

			if ((vkBindBufferMemory(device, buffer, bufferMemory, 0)) != VK_SUCCESS) {
				vkFreeMemory(device, bufferMemory, nullptr);
				vkDestroyBuffer(device, buffer, nullptr);
				*outResult = result;
				return -3;
			}

			*outBufferHandle = buffer;
			*outDeviceMemory = bufferMemory;

			return 0;
		}

		VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool pool)
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = pool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffer, &beginInfo);

			return commandBuffer;
		}

		void EndOneTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer commandBuffer) {
			vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(queue);

			vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
		}

		void EndOneTimeCommandsSemaphores(VkDevice device, VkQueue queue,
			VkCommandPool pool, VkCommandBuffer commandBuffer, VkSemaphore* semas, VkPipelineStageFlags* flags,
			uint32_t semasCount) {
			if (commandBuffer) vkEndCommandBuffer(commandBuffer);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = (commandBuffer ? 1 : 0);;
			submitInfo.pCommandBuffers = (commandBuffer ? &commandBuffer : nullptr);

			submitInfo.pWaitDstStageMask = flags;
			submitInfo.pWaitSemaphores = semas;
			submitInfo.waitSemaphoreCount = semasCount;

			vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(queue);

			vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
		}

		VkVertexInputBindingDescription CreateVertexInputBindingDescription(uint32_t vertexBufferLocation, size_t stride)
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = vertexBufferLocation;
			bindingDescription.stride = stride;
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		namespace MultiCommands
		{

			void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
				VkOffset3D offset, uint32_t baseLayer, uint32_t layerCount, VkImageAspectFlags aspectMask) {

				VkBufferImageCopy region{};
				region.bufferOffset = bufferOffset;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;

				region.imageSubresource.aspectMask = aspectMask;
				region.imageSubresource.mipLevel = mip;
				region.imageSubresource.baseArrayLayer = baseLayer;
				region.imageSubresource.layerCount = layerCount;

				region.imageOffset = offset;
				region.imageExtent = {
					width,
					height,
					1
				};

				vkCmdCopyBufferToImage(
					commandBuffer,
					buffer,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&region
				);
			}

			void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset, VkDeviceSize bufferSize,
				VkOffset3D offset, uint32_t baseLayer, uint32_t layerCount, VkBufferImageCopy* regions) {


				for (int i = 0; i < layerCount; i++)
				{
					VkBufferImageCopy& region = regions[i];
					region.bufferOffset = bufferOffset + (i * bufferSize);
					region.bufferRowLength = 0;
					region.bufferImageHeight = 0;

					region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					region.imageSubresource.mipLevel = mip;
					region.imageSubresource.baseArrayLayer = baseLayer + i;
					region.imageSubresource.layerCount = layerCount;

					region.imageOffset = offset;
					region.imageExtent = {
						width,
						height,
						1
					};
				}


				vkCmdCopyBufferToImage(
					commandBuffer,
					buffer,
					image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					layerCount,
					regions
				);
			}

			void CopyBufferBatch(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkBufferCopy* copyRegions, int numberOfCopies)
			{
				vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, numberOfCopies, copyRegions);
			}

			void CopyBuffer(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset, int copies, size_t stride) 
			{
				std::array<VkBufferCopy, 15> regions = {};

				for (int i = 0; i < copies; i++)
				{
					regions[i].srcOffset = srcOffset;
					regions[i].dstOffset = dstOffset;
					regions[i].size = size;

					dstOffset += stride;
				}

				vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, copies, regions.data());
			}

			int TransitionImageLayout(VkCommandBuffer commandBuffer,
				VkImage image, VkFormat format, VkImageLayout oldLayout,
				VkImageLayout newLayout, uint32_t mips, uint32_t layers)
			{
				int depthStencilType = 0;
				int colorType = 0;

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = oldLayout;
				barrier.newLayout = newLayout;

				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				barrier.image = image;

				if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
					depthStencilType = 1;
				}


				if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
					depthStencilType = 1;
				}


				if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
					newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
					!depthStencilType)
				{
					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					colorType = 1;
				}

				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = mips;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = layers;

				VkPipelineStageFlags sourceStage;
				VkPipelineStageFlags destinationStage;

				if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				{
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
				{
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && depthStencilType)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				}
				else
				{
					return -1;
				}

				vkCmdPipelineBarrier(
					commandBuffer,
					sourceStage, destinationStage,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				return 0;
			}
		}
	}
}
