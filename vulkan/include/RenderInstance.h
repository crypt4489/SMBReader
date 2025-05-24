#pragma once


#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

#include "FileManager.h"
#include "ThreadManager.h"
#include "VKInstance.h"
#include "VKDescriptorSetCache.h"
#include "VKDescriptorLayoutCache.h"
#include "VKSwapChain.h"
#include "VKShaderCache.h"
#include "VKPipelineCache.h"
#include "WindowManager.h"





class RenderInstance
{
public:

	RenderInstance() : transferSemaphore(Semaphore(1))
	{

	};
	/*
	void CreateSwapChain()
	{
		::VK::Utils::SwapChainSupportDetails swapChainSupport = vkInstance.GetSwapChainSupport(physicalIndex);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = vkInstance.renderSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		
		uint32_t queueFamilyIndices[2] = { graphicsIndex, presentIndex };

		if (graphicsIndex != presentIndex)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} 
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

	}

	void CreateVKSWCImageViews()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	void CreateFrameBuffers()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			std::array<VkImageView, 3> attachments = {
				colorImageView,
				depthImageView,
				swapChainImageViews[i],
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer #" + std::to_string(i) + "!");
			}
		}
	} */

	void CreateDepthImage(uint32_t width, uint32_t height)
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);

		depthImage = dev.CreateImage(width, height,
			1, depthFormat, 1,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			msaaSamples,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

		depthView = dev.CreateImageView(depthImage, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		dev.TransitionImageLayout(graphicsPresentIndex, graphicsIndex, 
			depthImage, depthFormat, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
			1, 1
		);
	}

	void DestroySwapChainAttachments()
	{
		/*
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		if (colorImageView) vkDestroyImageView(logicalDevice, colorImageView, nullptr);
		if (colorImage) vkDestroyImage(logicalDevice, colorImage, nullptr);
		if (colorImageMemory) vkFreeMemory(logicalDevice, colorImageMemory, nullptr);


		vkDestroyImageView(logicalDevice, depthImageView, nullptr);
		vkDestroyImage(logicalDevice, depthImage, nullptr);
		vkFreeMemory(logicalDevice, depthMemory, nullptr);
		*/

		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		dev.DestroyImage(depthImage);
		dev.DestroyImageView(depthView);
		dev.DestroyImage(colorImage);
		dev.DestroyImageView(colorView);
	}

	void RecreateSwapChain() {
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
		int width = 0, height = 0;
		glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(windowMan->GetWindow(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(logicalDevice);

		DestroySwapChainAttachments();
		swapChain.DestroySwapChain();

		CreateMSAAColorResources(width, height);
		CreateDepthImage(width, height);

		swapChain.RecreateSwapChain(width, height);
		
		swapChain.CreateSwapChainImageViews();
		std::vector<VkImageView> attachmentViews = { dev.GetImageView(colorView), dev.GetImageView(depthView)};
		swapChain.CreateFrameBuffers(renderPass, attachmentViews);
	
	}

	std::pair<VkShaderModule, VkShaderStageFlagBits> GetShaderFromCache(const std::string& name)
	{	  
		return shaderCache.GetShader(name);
	}


	void CreateRenderPass()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChain.GetSwapChainFormat();
		colorAttachment.samples = msaaSamples;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChain.GetSwapChainFormat();
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void CreateCommandBuffer()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = dev.GetCommandPool(graphicsPresentIndex);
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void BeginCommandBufferRecording(VkCommandBuffer cb, uint32_t imageIndex)
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; 
		beginInfo.pInheritanceInfo = nullptr; 

		if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChain.swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain.swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChain.GetSwapChainWidth());
		viewport.height = static_cast<float>(swapChain.GetSwapChainHeight());
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cb, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChain.swapChainExtent;
		vkCmdSetScissor(cb, 0, 1, &scissor);

	}

	void EndCommandBufferRecording(VkCommandBuffer cb)
	{
		vkCmdEndRenderPass(cb);

		if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void CreateSyncObjects()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create semaphores!");
			}
		}
	}

	uint32_t BeginFrame()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);

		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex = swapChain.AcquireNextSwapChainImage(UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE);

		if (imageIndex == UINT32_MAX)
		{
			RecreateSwapChain();
			return imageIndex;
		}

		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);

		BeginCommandBufferRecording(commandBuffers[currentFrame], imageIndex);

		return imageIndex;
	}

	void SubmitFrame(uint32_t imageIndex)
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);
		
		EndCommandBufferRecording(commandBuffers[currentFrame]);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(dev.GetQueueHandle(graphicsIndex, 0), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain.swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(dev.GetQueueHandle(presentIndex, 0), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resizeWindow) {
			resizeWindow = false;
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	

	void CreateMSAAColorResources(uint32_t width, uint32_t height) {

		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;
		VKSwapChain& swapChain = dev.GetSwapChain(swapChainIndex);

		colorImage = dev.CreateImage(width, height,
			1, swapChain.GetSwapChainFormat(), 1,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			msaaSamples,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachmentsIndex);

		colorView = dev.CreateImageView(colorImage, 1, swapChain.GetSwapChainFormat(), VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void WaitOnQueues()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		vkQueueWaitIdle(dev.GetQueueHandle(graphicsIndex, 0));
		vkQueueWaitIdle(dev.GetQueueHandle(presentIndex, 0));
	}

	static void frameResizeCB(GLFWwindow* window, int width, int height)
	{
		auto renderInst = reinterpret_cast<RenderInstance*>(glfwGetWindowUserPointer(window));
		renderInst->SetResizeBool(true);
	}

	void CreatePipelines()
	{
		// Create Shaders

		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;

		auto shader1 = shaderCache.GetShader("3dtextured.vert.spv");
		auto shader2 = shaderCache.GetShader("3dtextured.frag.spv");

		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders = { shader1, shader2 };

		auto shader3 = shaderCache.GetShader("text.vert.spv");
		auto shader4 = shaderCache.GetShader("text.frag.spv");

		std::vector<std::pair<VkShaderModule, VkShaderStageFlagBits>> shaders2 = { shader3, shader4 };

		DescriptorSetLayoutBuilder textDescriptor{}, globalBufferBuilder{}, genericObjectBuilder{};
		std::vector<VkDescriptorSetLayout> textDescriptorContainers(1);
		std::vector<VkDescriptorSetLayout> regularMeshConatiners(2);

		textDescriptor.AddPixelImageSamplerLayout(0);

		textDescriptorContainers[0] = textDescriptor.CreateDescriptorSetLayout(logicalDevice);

		descriptorLayoutCache.AddLayout("oneimage", textDescriptorContainers[0]);

		globalBufferBuilder.AddBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

		regularMeshConatiners[0] = globalBufferBuilder.CreateDescriptorSetLayout(logicalDevice);
		
		descriptorLayoutCache.AddLayout("mainrenderpass", regularMeshConatiners[0]);

		genericObjectBuilder.AddDynamicBufferLayout(0, VK_SHADER_STAGE_VERTEX_BIT);

		genericObjectBuilder.AddPixelImageSamplerLayout(1);

		regularMeshConatiners[1] = genericObjectBuilder.CreateDescriptorSetLayout(logicalDevice);

		descriptorLayoutCache.AddLayout("genericobject", regularMeshConatiners[1]);

		mainRenderPassCache.CreatePipeline(regularMeshConatiners, std::nullopt, std::nullopt, shaders, VK_COMPARE_OP_LESS, GetMSAASamples(), "genericpipeline");

		mainRenderPassCache.CreatePipeline(textDescriptorContainers, TextVertex::getBindingDescription(), TextVertex::getAttributeDescriptions(), shaders2, VK_COMPARE_OP_ALWAYS, GetMSAASamples(), "text");

		
	}

	PipelineCacheObject GetPipeline(std::string ptype)
	{
		return mainRenderPassCache.GetPipelineFromCache(ptype);
	}

	void CreateGlobalBuffer()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		globalIndex = dev.CreateHostBuffer
		(
			128'000'000, true, true, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		);

	//	dynamicOffset = dev.GetOffsetIntoHostBuffer(globalIndex, sizeof(glm::mat4) * 256 * MAX_FRAMES_IN_FLIGHT, 16);

		
	}

	void UpdateDynamicGlobalBuffer(void* data, size_t dataSize, size_t offset, uint32_t frame)
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		dev.WriteToHostBuffer(globalIndex, data, dataSize, offset + (dataSize * frame));
	}


	uint32_t GetPageFromUniformBuffer(size_t size, size_t alignment)
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.GetOffsetIntoHostBuffer(globalIndex, size, alignment);
	}

	uint32_t GetMainBufferIndex() const
	{
		return globalIndex;
	}

	VkBuffer GetDynamicUniformBuffer() 
	{
		return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).hostBuffers[globalIndex].first;
	}

	uint32_t CreateVulkanImage(
		std::vector<std::vector<char>>& imageData,
		std::vector<uint32_t>& imageSizes,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return majorDevice.CreateSampledImage(
			imageData, imageSizes, 
			width, height, 
			mipLevels, VK::API::ConvertSMBToVkFormat(type), 
			1, 
			graphicsIndex, transferIndex, 
			attachmentsIndex, stagingBufferIndex);
	}

	uint32_t CreateVulkanImageView(uint32_t imageIndex, uint32_t mipLevels,
		ImageFormat type, VkImageAspectFlags aspectMask)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return majorDevice.CreateImageView(imageIndex, mipLevels, VK::API::ConvertSMBToVkFormat(type), aspectMask);
	}

	uint32_t CreateVulkanSampler(uint32_t mipLevels)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return majorDevice.CreateSampler(mipLevels);
	}

	void DeleteVulkanImageView(uint32_t index)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		majorDevice.DestroyImage(index);
	}

	void DeleteVulkanImage(uint32_t index)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		majorDevice.DestroyImage(index);
	}

	void DeleteVulkanSampler(uint32_t index)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		majorDevice.DestorySampler(index);
	}

	void CreateVulkanRenderer(WindowManager *window)
	{
		this->windowMan = window;
		windowMan->SetWindowResizeCallback(frameResizeCB);
		glfwSetWindowUserPointer(windowMan->GetWindow(), this);
		

		vkInstance.CreateRenderInstance();
		vkInstance.CreateDrawingSurface(window->GetWindow());
		
		physicalIndex = vkInstance.CreatePhysicalDevice();
		VKDevice &majorDevice = vkInstance.CreateLogicalDevice(physicalIndex, deviceIndex);

		msaaSamples = vkInstance.GetMaxMSAALevels(physicalIndex);
		
		std::vector<VkQueueFamilyProperties> famProps;
		majorDevice.QueueFamilyDetails(famProps);
		
		uint32_t graphicsIndex, presentIndex, presentMax, graphicsMax;
		majorDevice.GetPresentQueue(presentIndex, presentMax, famProps, vkInstance.renderSurface);
		majorDevice.GetQueueByMask(graphicsIndex, graphicsMax, famProps, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);
		
		std::set queueFamilies = { graphicsIndex, presentIndex };
		std::vector<uint32_t> queueCounts = { graphicsMax, presentMax };
		
		VkPhysicalDeviceFeatures features{};
		features.geometryShader = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.tessellationShader = VK_TRUE;
		features.samplerAnisotropy = VK_TRUE;
		features.multiDrawIndirect = VK_TRUE;

		graphicsPresentIndex = 0;
		transferIndex = 1;

		majorDevice.CreateLogicalDevice(vkInstance.instanceLayers, vkInstance.deviceExtensions, queueFamilies, queueCounts, features);
		
		majorDevice.CreateCommandPool(graphicsIndex, graphicsPresentIndex);
		majorDevice.CreateCommandPool(graphicsIndex, transferIndex);
		
		gptManager = majorDevice.CreateQueueManager(graphicsIndex, transferIndex, graphicsMax);

		depthFormat = VK::Utils::findSupportedFormat(majorDevice.gpu,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		stagingBufferIndex = majorDevice.CreateHostBuffer(64'000'000, true, false, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		swapChainIndex = majorDevice.CreateSwapChain(vkInstance.renderSurface);

		VKSwapChain& swapChain = majorDevice.GetSwapChain(swapChainIndex);

		auto scd = vkInstance.GetSwapChainSupport(physicalIndex);

		swapChain.SetSwapChainProperties(scd);

		VkFormat swcFormat = swapChain.GetSwapChainFormat();

		auto swcPool = majorDevice.FindImageMemoryIndexForPool(1920, 1200,
			1, swcFormat, 1,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		auto depthPool = majorDevice.FindImageMemoryIndexForPool(1920, 1200,
			1, depthFormat, 1,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		attachmentsIndex = majorDevice.CreateImageMemoryPool(512'000'000, depthPool.first);
		
		CreateMSAAColorResources(800, 600);

		CreateDepthImage(800, 600);

		uint32_t queuefamilies[] = { graphicsIndex };
		
		swapChain.CreateSwapChain(800, 600, queuefamilies, 1);
		swapChain.CreateSwapChainImageViews();

		DescriptorPoolBuilder builder{};
		builder.AddUniformPoolSize(MAX_FRAMES_IN_FLIGHT * 100);
		builder.AddImageSampler(MAX_FRAMES_IN_FLIGHT * 100);
		majorDevice.CreateDesciptorPool(descriptorPoolIndex, builder, MAX_FRAMES_IN_FLIGHT * 100);
		
		CreateRenderPass();
	
		std::vector<VkImageView> attachmentViews = { majorDevice.GetImageView(colorView), majorDevice.GetImageView(depthView) };
		swapChain.CreateFrameBuffers(renderPass, attachmentViews);
		
		
		CreateCommandBuffer();
		CreateSyncObjects();

		descriptorLayoutCache.device = majorDevice.device;
		mainRenderPassCache.renderPass = renderPass;
		mainRenderPassCache.device = majorDevice.device;
		shaderCache.SetLogicalDevice(majorDevice.device);

		CreateGlobalBuffer();

		CreatePipelines();
	}

	VkImageView GetImageView(uint32_t viewIndex)
	{
		VKDevice& majorDevice = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return majorDevice.imageViews[viewIndex];
	}

	VkSampler GetSampler(uint32_t index)
	{
		return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).samplers[index];
	}

	
	void DestroyVulkanRenderer()
	{
		DestroyRenderInstance();
	}

	void DestroyRenderInstance()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		VkDevice logicalDevice = dev.device;

		shaderCache.DestroyShaderCache();

		mainRenderPassCache.DestroyPipelineCache();

		descriptorLayoutCache.DestroyLayoutCache();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (inFlightFences[i])
			{
				vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
			}

			if (renderFinishedSemaphores[i])
			{
				vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
			}

			if (imageAvailableSemaphores[i])
			{
				vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
			}
		}


		if (renderPass)
		{
			vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
		}
	}

	void SetResizeBool(bool set)
	{
		resizeWindow = set;
	}

	VkDevice GetVulkanDevice()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.device;
	}

	VkPhysicalDevice GetVulkanPhysicalDevice()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.gpu;
	}

	VkCommandPool GetVulkanGraphincsCommandPool()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.GetCommandPool(graphicsPresentIndex);
	}

	VkQueue GetGraphicsQueue() 
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.GetQueueHandle(graphicsIndex, 0);
	}

	VkCommandPool GetVulkanTransferCommandPool()
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.GetCommandPool(transferIndex);
	}

	auto GetTransferQueue()
	{
		return gptManager->GetQueue();
	}

	void ReturnTranferQueue(int32_t i)
	{
		gptManager->ReturnQueue(i);
	}

	VkRenderPass GetRenderPass() const
	{
		return renderPass;
	}

	VkDescriptorPool GetDescriptorPool() 
	{
		VKDevice& dev = vkInstance.GetLogicalDevice(physicalIndex, deviceIndex);
		return dev.GetDescriptorPool(descriptorPoolIndex);
	}

	VkCommandBuffer GetCurrentCommandBuffer() const
	{
		return commandBuffers[currentFrame];
	}

	uint32_t GetCurrentFrame() const
	{
		return currentFrame;
	}

	VkSampleCountFlagBits GetMSAASamples() const
	{
		return msaaSamples;
	}

	Semaphore& GetTransferSemaphore()
	{
		return transferSemaphore;
	}

	VKPipelineCache* GetMainRenderPassCache()
	{
		return &mainRenderPassCache;
	}

	VKDescriptorLayoutCache* GetDescriptorLayoutCache()
	{
		return &descriptorLayoutCache;
	}

	 
	VKDescriptorSetCache* GetDescriptorSetCache()
	{
		return &descriptorSetCache;;
	}

	uint32_t GetSwapChainHeight()
	{
		return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainHeight();
	}

	uint32_t GetSwapChainWidth() 
	{
		return vkInstance.GetLogicalDevice(physicalIndex, deviceIndex).GetSwapChain(swapChainIndex).GetSwapChainWidth();
	}

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	

private:

	VKInstance vkInstance;
	uint32_t graphicsIndex, presentIndex, presentMax, graphicsMax;
	uint32_t deviceIndex;
	uint32_t physicalIndex;
	uint32_t transferIndex, graphicsPresentIndex;
	uint32_t descriptorPoolIndex;
	uint32_t swapChainIndex;
	uint32_t attachmentsIndex;
	uint32_t depthView, depthImage;
	uint32_t colorView, colorImage;
	uint32_t stagingBufferIndex;
	uint32_t globalIndex;
	uint32_t dynamicIndex, dynamicOffset;


	VkRenderPass renderPass = VK_NULL_HANDLE;

	QueueManager* gptManager;

	Semaphore transferSemaphore; // semaphore for transfer command pool

	std::vector<VkCommandBuffer> commandBuffers{};

	std::vector<VkSemaphore> imageAvailableSemaphores{};
	std::vector<VkSemaphore> renderFinishedSemaphores{};
	std::vector<VkFence> inFlightFences{};

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	bool resizeWindow = false;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	VkFormat depthFormat;

	VKShaderCache shaderCache;
	VKPipelineCache mainRenderPassCache;
	VKDescriptorLayoutCache descriptorLayoutCache;
	VKDescriptorSetCache descriptorSetCache;

};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}