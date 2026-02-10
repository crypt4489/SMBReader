#include "pch.h"

#include <array>

#include "VKUtilities.h"


namespace VK {

	namespace Utils {

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
				SwapChainSupportDetails details;

				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

				uint32_t formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

				if (formatCount) {
					details.formats.resize(formatCount);
					vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
				}

				uint32_t presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

				if (presentModeCount) {
					details.presentModes.resize(presentModeCount);
					vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
				}

				return details;
		}

		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceProperties& props)
		{
				// Extract and display API version
				uint32_t apiVersionMajor = VK_VERSION_MAJOR(props.apiVersion);
				uint32_t apiVersionMinor = VK_VERSION_MINOR(props.apiVersion);
				uint32_t apiVersionPatch = VK_VERSION_PATCH(props.apiVersion);
				os << "API Version: " << apiVersionMajor << "." << apiVersionMinor << "." << apiVersionPatch << "\n";

				// Device properties
				os << "Device ID: " << props.deviceID << "\n";
				os << "Vendor ID: " << props.vendorID << "\n";
				os << "Device Name: " << props.deviceName << "\n";

				// Device type (as string)
				os << "Device Type: ";
				switch (props.deviceType)
				{
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: os << "Integrated GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   os << "Discrete GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    os << "Virtual GPU"; break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:            os << "CPU"; break;
				default:                                     os << "Unknown"; break;
				}
				os << "\n";

				// Other properties
				os << "Driver Version: " << props.driverVersion << "\n";
				os << "Limits: Max Image Dimension 2D: " << props.limits.maxImageDimension2D << "\n";

				return os;
		}


		std::ostream& operator<<(std::ostream& os, const VkPhysicalDeviceMemoryProperties& props)
		{
				uint32_t i;

				for (i = 0; i < props.memoryHeapCount; i++)
				{
					os << "Heap Size : ";
					os << ((double)props.memoryHeaps[i].size / 1'000'000'000) << " GBs\n";
					os << "Heap Flags : ";
					os << props.memoryHeaps[i].flags << "\n";
				}

				for (i = 0; i < props.memoryTypeCount; i++)
				{
					os << "Memory Type Heap Index : ";
					os << props.memoryTypes[i].heapIndex << "\n";
					os << "Memory Type Property Flags : ";
					os << props.memoryTypes[i].propertyFlags << "\n";
				}

				return os;
		}

		std::ostream& operator<<(std::ostream& os, const VkQueueFamilyProperties& props)
		{

				os << "Queue Flags : \n";
				if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) os << "VK_QUEUE_TRANSFER_BIT\n";
				if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) os << "VK_QUEUE_COMPUTE_BIT\n";
				if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) os << "VK_QUEUE_GRAPHICS_BIT\n";
				if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) os << "VK_QUEUE_SPARSE_BINDING_BIT\n";

				os << "Queue Count : " << props.queueCount << "\n";
				os << "Time Stamp Valid Bits : " << props.timestampValidBits << "\n";
				os << "Min Image Transfer Granularity (depth, height, width) : \n";
				os << props.minImageTransferGranularity.depth << " " << props.minImageTransferGranularity.height << " " << props.minImageTransferGranularity.width << "\n";

				return os;
		}

		VkVertexInputBindingDescription CreateVertexInputBindingDescription(uint32_t vertexBufferLocation, size_t stride)
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = vertexBufferLocation;
			bindingDescription.stride = stride;
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}


		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData) {

				if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
					return VK_FALSE;
				}

				std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

				return VK_FALSE;
		}


		VkDebugUtilsMessengerEXT debugMessenger;

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator) {
				auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
				if (func) {
					return func(instance, pCreateInfo, pAllocator, &debugMessenger);
				}
				else {
					return VK_ERROR_EXTENSION_NOT_PRESENT;
				}
		}

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, const VkAllocationCallbacks* pAllocator) {

				if (!debugMessenger) return;

				auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
				if (func) {
					func(instance, debugMessenger, pAllocator);
				}
		}

		uint32_t findMemoryType(VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
				VkPhysicalDeviceMemoryProperties memProperties;
				vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

				for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
					if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
						return i;
					}
				}

				throw std::runtime_error("failed to find suitable memory type!");
		}

		VkFormat findSupportedFormat(VkPhysicalDevice& gpu, VkFormat* candidates, size_t candidateSize, VkImageTiling tiling, VkFormatFeatureFlags features)
		{
				for (size_t i = 0; i<candidateSize; i++)
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

				throw std::runtime_error("failed to find supported format!");
		}

		VkShaderModule createShaderModule(VkDevice& device, char* code, size_t size) {
				VkShaderModuleCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				createInfo.codeSize = size;;
				createInfo.pCode = reinterpret_cast<const uint32_t*>(code);
				VkShaderModule shaderModule;
				if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
					throw std::runtime_error("failed to create shader module!");
				}
				return shaderModule;
			}

			std::pair<VkBuffer, VkDeviceMemory> createBuffer(VkDevice& device, VkPhysicalDevice& physicalDevice,
				VkDeviceSize bufferSize,
				VkMemoryPropertyFlags memprops, VkSharingMode sharingMode,
				VkBufferUsageFlags usage)
			{
				VkBuffer buffer;
				VkDeviceMemory bufferMemory;
				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = bufferSize;
				bufferInfo.sharingMode = sharingMode;
				bufferInfo.usage = usage;
				if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
					throw std::runtime_error("failed to create buffer!");
				}

				VkMemoryRequirements memRequirements;
				vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memprops);

				if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
					throw std::runtime_error("failed to allocate buffer memory!");
				}

				vkBindBufferMemory(device, buffer, bufferMemory, 0);

				return std::make_pair<>(buffer, bufferMemory);
			}

			VkCommandBuffer BeginOneTimeCommands(VkDevice& device, VkCommandPool& pool)
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

			void EndOneTimeCommands(VkDevice& device, VkQueue& queue, VkCommandPool& pool, VkCommandBuffer commandBuffer) {
				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(queue);

				vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
			}

			void EndOneTimeCommandsSemaphores(VkDevice& device, VkQueue& queue, 
				VkCommandPool& pool, VkCommandBuffer commandBuffer, VkSemaphore* semas, VkPipelineStageFlags* flags,
				uint32_t semasCount) {
				if (commandBuffer) vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = (commandBuffer ? 1 : 0);;
				submitInfo.pCommandBuffers = (commandBuffer ?  &commandBuffer :  nullptr);

				submitInfo.pWaitDstStageMask = flags;
				submitInfo.pWaitSemaphores = semas;
				submitInfo.waitSemaphoreCount = semasCount;

				vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(queue);

				vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
			}

			void CopyBuffer( VkCommandBuffer& cmdBuffer, VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset, int copies, size_t stride) {
				
				std::array<VkBufferCopy, 15> regions={};
				
			

				for (int i = 0; i < copies; i++)
				{
					regions[i].srcOffset = srcOffset;
					regions[i].dstOffset = dstOffset;
					regions[i].size = size;

					dstOffset += stride;
				}

				
				vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, copies, regions.data());


			}

			void CopyBufferBatch(VkCommandBuffer& commandBuffer, VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkBufferCopy* copyRegions, int numberOfCopies)
			{
				vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, numberOfCopies, copyRegions);
			}


			std::pair<VkImage, VkDeviceMemory> CreateImage(VkDevice& device, VkPhysicalDevice& gpu, VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties)
			{
				VkImage image;
				VkDeviceMemory imageMemory;

				if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
					throw std::runtime_error("failed to create image!");
				}

				VkMemoryRequirements memRequirements;
				vkGetImageMemoryRequirements(device, image, &memRequirements);

				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = VK::Utils::findMemoryType(gpu, memRequirements.memoryTypeBits, properties);

				if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
					throw std::runtime_error("failed to allocate image memory!");
				}

				vkBindImageMemory(device, image, imageMemory, 0);

				return std::make_pair<>(image, imageMemory);
			}

			VkImageView CreateImageView(VkDevice& device, VkImageViewCreateInfo& viewInfo)
			{
				VkImageView imageView = VK_NULL_HANDLE;
				if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
					throw std::runtime_error("failed to create texture image view!");
				}
				return imageView;
			}

			void CopyBufferToImage(VkDevice& device, VkCommandPool& pool, VkQueue& queue, VkBuffer& buffer, VkImage& image,
				uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
				VkOffset3D offset) {
				VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, pool);

				VkBufferImageCopy region{};
				region.bufferOffset = bufferOffset;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;

				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = mip;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;

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

				EndOneTimeCommands(device, queue, pool, commandBuffer);
			}

			void TransitionImageLayout(VkDevice& device, VkCommandPool& pool, VkQueue& queue,
				VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mips, uint32_t layers) {
				VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, pool);

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = oldLayout;
				barrier.newLayout = newLayout;

				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				barrier.image = image;

				if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

					if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
						barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
					}
				}
				else {
					barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				}
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = mips;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = layers;

				VkPipelineStageFlags sourceStage;
				VkPipelineStageFlags destinationStage;

				if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
				}
				else {
					throw std::invalid_argument("unsupported layout transition!");
				}

				vkCmdPipelineBarrier(
					commandBuffer,
					sourceStage, destinationStage,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				EndOneTimeCommands(device, queue, pool, commandBuffer);
			}

			namespace MultiCommands
			{

				void CopyBufferToImage(VkCommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image,
					uint32_t width, uint32_t height, uint32_t mip, VkDeviceSize bufferOffset,
					VkOffset3D offset, uint32_t baseLayer, uint32_t layerCount) {

					VkBufferImageCopy region{};
					region.bufferOffset = bufferOffset;
					region.bufferRowLength = 0;
					region.bufferImageHeight = 0;

					region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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

				void CopyBufferToImage(VkCommandBuffer& commandBuffer, VkBuffer& buffer, VkImage& image,
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
						region.imageSubresource.baseArrayLayer = baseLayer+i;
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

				void TransitionImageLayout(VkCommandBuffer& commandBuffer,
					VkImage& image, VkFormat format, VkImageLayout oldLayout,
					VkImageLayout newLayout, uint32_t mips, uint32_t layers) {


					VkImageMemoryBarrier barrier{};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.oldLayout = oldLayout;
					barrier.newLayout = newLayout;

					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

					barrier.image = image;
					if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
						barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

						if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
							barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
						}
					}
					else {
						barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					}
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = mips;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = layers;

					VkPipelineStageFlags sourceStage;
					VkPipelineStageFlags destinationStage;

					if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
						barrier.srcAccessMask = 0;
						barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

						sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
						destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					}
					else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

						sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
						destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					}
					else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
						barrier.srcAccessMask = 0;
						barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

						sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
						destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					}
					else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
						barrier.srcAccessMask = 0;
						barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

						sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
						destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					}

					else {
						throw std::invalid_argument("unsupported layout transition!");
					}

					vkCmdPipelineBarrier(
						commandBuffer,
						sourceStage, destinationStage,
						0,
						0, nullptr,
						0, nullptr,
						1, &barrier
					);
				}
			}

		}
	
}
