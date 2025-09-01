#include "VKDevice.h"
#include "VKRenderGraph.h"
#include <mutex>
#include <shared_mutex>

RecordingBufferObject::RecordingBufferObject(VKDevice& device, VKCommandBuffer& buffer) :
	cbBufferHandler(buffer), vkDeviceHandle(device), currLayout(VK_NULL_HANDLE), currPipeline(VK_NULL_HANDLE)
{

}

void RecordingBufferObject::BindPipeline(uint32_t renderTarget, std::string pipelinename)
{
	auto pbo = vkDeviceHandle.GetPipelineCache(renderTarget);
	auto pco = pbo[pipelinename];
	currLayout = pco->pipelineLayout;
	currPipeline = pco->pipeline;
	vkCmdBindPipeline(cbBufferHandler.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currPipeline);
}

void RecordingBufferObject::BindDescriptorSets(std::string descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet, 
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	auto descsets = vkDeviceHandle.GetDescriptorSets();
	auto descset = descsets.GetDescriptorSetPerFrame(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, currLayout, 
		firstDescriptorSet, descriptorCount, 
		&descset, dynamicOffsetCount, 
		offsets);
}

void RecordingBufferObject::BindVertexBuffer(uint32_t bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets)
{
	VkBuffer buffer = vkDeviceHandle.hostBuffers[bufferIndex].first;
	vkCmdBindVertexBuffers(cbBufferHandler.buffer, firstBindingCount, bindingCount, &buffer, offsets);
}

void RecordingBufferObject::BindingDrawCmd(uint32_t first, uint32_t drawSize)
{
	vkCmdDraw(cbBufferHandler.buffer, drawSize, 1, first, 0);
}

void RecordingBufferObject::BindingIndirectDrawCmd(uint32_t indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset)
{
	VkBuffer buffer = vkDeviceHandle.hostBuffers[indirectBufferIndex].first;
	vkCmdDrawIndirect(cbBufferHandler.buffer, buffer, indirectBufferOffset, drawCount, sizeof(VkDrawIndirectCommand));
}

void RecordingBufferObject::EndRenderPassCommand()
{
	vkCmdEndRenderPass(cbBufferHandler.buffer);
}

void RecordingBufferObject::BeginRenderPassCommand(uint32_t renderTargetIndex, uint32_t imageIndex,
	VkRect2D rect,
	VkClearColorValue color,
	VkClearDepthStencilValue depthStencil)
{

	VkRenderPassBeginInfo renderPassInfo{};
	auto& ref = vkDeviceHandle.GetRenderTarget(renderTargetIndex);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vkDeviceHandle.renderPasses[ref.renderPassIndex];
	renderPassInfo.framebuffer = vkDeviceHandle.frameBuffers[ref.framebufferIndices[imageIndex]];
	renderPassInfo.renderArea = rect;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = color;
	clearValues[1].depthStencil = depthStencil;

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cbBufferHandler.buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}


void RecordingBufferObject::SetViewportCommand(float xs, float ys, float width, float height, float minDepth, float maxDepth)
{
	VkViewport viewport{};
	viewport.x = xs;
	viewport.y = ys;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	vkCmdSetViewport(cbBufferHandler.buffer, 0, 1, &viewport);
}

void RecordingBufferObject::SetScissorCommand(int xo, int yo, uint32_t extentx, uint32_t extenty)
{
	VkRect2D scissor{};
	scissor.offset = { xo, yo };

	scissor.extent = { extentx, extenty };

	vkCmdSetScissor(cbBufferHandler.buffer, 0, 1, &scissor);
}


void RecordingBufferObject::BeginRecordingCommand()
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(cbBufferHandler.buffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}

void RecordingBufferObject::EndRecordingCommand()
{
	if (vkEndCommandBuffer(cbBufferHandler.buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

VKDevice::VKDevice(VkPhysicalDevice _gpu) : gpu(_gpu), device(VK_NULL_HANDLE)
{
}

VKDevice::~VKDevice()
{
	if (device)
	{
		vkDeviceWaitIdle(device);

		shaders.DestroyShaderCache();

		descriptorLayoutCache.DestroyLayoutCache();

		for (auto& [index, poc] : renderPassPipelineCache)
		{
			poc.DestroyPipelineCache();
		}

		for (auto& fb : frameBuffers)
		{
			vkDestroyFramebuffer(device, fb, nullptr);
		}

		for (auto& f : fences)
		{
			vkDestroyFence(device, f, nullptr);
		}

		for (auto& sema : semaphores)
		{
			vkDestroySemaphore(device, sema, nullptr);
		}

		for (auto& rp : renderPasses)
		{
			vkDestroyRenderPass(device, rp, nullptr);
		}

		for (auto& hb : hostBuffers)
		{
			vkDestroyBuffer(device, hb.first, nullptr);
			vkFreeMemory(device, hb.second, nullptr);
		}

		for (auto& sc : swapChains)
		{
			sc.DestroySwapChain();
		}

		for (auto& smp : samplers)
		{
			vkDestroySampler(device, smp, nullptr);
		}

		for (auto& iv : imageViews)
		{
			vkDestroyImageView(device, iv, nullptr);
		}

		for (auto& i : images)
		{
			vkDestroyImage(device, std::get<VkImage>(i), nullptr);
		}

		for (auto& m : deviceMemories)
		{
			vkFreeMemory(device, m, nullptr);
		}

		for (auto& d : descriptorPools)
		{
			vkDestroyDescriptorPool(device, d, nullptr);
		}

		for (auto& c : commandPools)
		{
			vkDestroyCommandPool(device, c, nullptr);
		}

		for (auto& qm : queueManagers)
		{
			delete qm;
		}

		vkDestroyDevice(device, nullptr);
	}
}

VKDevice& VKDevice::operator=(VKDevice&& _dev) noexcept {
	this->device = _dev.device;
	_dev.device = VK_NULL_HANDLE;
	this->gpu = _dev.gpu;
	_dev.gpu = VK_NULL_HANDLE;
	this->commandPools = std::move(_dev.commandPools);
	this->queueManagers = std::move(_dev.queueManagers);
	this->descriptorPools = std::move(_dev.descriptorPools);
	this->swapChains = std::move(_dev.swapChains);
	this->deviceMemories = std::move(_dev.deviceMemories);
	this->allocators = std::move(_dev.allocators);
	this->deviceBuffers = std::move(_dev.deviceBuffers);
	this->images = std::move(_dev.images);
	this->imageViews = std::move(_dev.imageViews);
	this->renderPasses = std::move(_dev.renderPasses);
	this->shaders = std::move(_dev.shaders);
	return *this;
};

VKDevice::VKDevice(VKDevice&& _dev)  noexcept
{
	//this->shaders;
	this->device = _dev.device;
	_dev.device = VK_NULL_HANDLE;
	this->gpu = _dev.gpu;
	_dev.gpu = VK_NULL_HANDLE;
	this->commandPools = std::move(_dev.commandPools);
	this->queueManagers = std::move(_dev.queueManagers);
	this->descriptorPools = std::move(_dev.descriptorPools);
	this->swapChains = std::move(_dev.swapChains);
	this->deviceMemories = std::move(_dev.deviceMemories);
	this->allocators = std::move(_dev.allocators);
	this->deviceBuffers = std::move(_dev.deviceBuffers);
	this->images = std::move(_dev.images);
	this->imageViews = std::move(_dev.imageViews);
	this->renderPasses = std::move(_dev.renderPasses);
	this->shaders = std::move(_dev.shaders);
};

VkPhysicalDevice VKDevice::GetGPU() const
{
	return gpu;
}

void VKDevice::QueueFamilyDetails(std::vector<VkQueueFamilyProperties>& famProps)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

	famProps.resize(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, famProps.data());
}

int32_t VKDevice::GetPresentQueue(QueueIndex& queueIdx,
	QueueIndex& maxQueueCount,
	std::vector<VkQueueFamilyProperties>& famProps,
	VkSurfaceKHR& renderSurface)
{
	uint32_t i = 0;
	for (const auto& props : famProps)
	{
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, renderSurface, &presentSupport);

		if (presentSupport)
		{
			maxQueueCount = QueueIndex(props.queueCount);
			queueIdx = QueueIndex(i);
			return 0;
		}
		i++;
	}
	return -1;
}

int32_t VKDevice::GetQueueByMask(QueueIndex& queueIdx,
	QueueIndex& maxQueueCount,
	std::vector<VkQueueFamilyProperties>& famProps,
	uint32_t queueMask)
{
	uint32_t i = 0;
	for (const auto& props : famProps)
	{

		if ((props.queueFlags & queueMask) == queueMask) {
			queueIdx = QueueIndex(i);
			maxQueueCount = QueueIndex(props.queueCount);
			return 0;
		}
		i++;
	}
	return -1;
}

void VKDevice::CreateLogicalDevice(
	std::vector<const char*>& instanceLayers,
	std::vector<const char*>& deviceExtensions,
	uint32_t queueFlags,
	VkPhysicalDeviceFeatures& features,
	VkSurfaceKHR renderSurface
	)
{
	std::unordered_map<uint32_t, std::tuple<uint32_t, bool>> queueIndices;
	std::vector<VkQueueFamilyProperties> famProps;
	QueueIndex index, maxCount;

	QueueFamilyDetails(famProps);

	GetQueueByMask(index, maxCount, famProps, queueFlags);

	queueIndices[index] = { maxCount, false };

	if (renderSurface)
	{
		GetPresentQueue(index, maxCount, famProps, renderSurface);
		queueIndices[index] = { maxCount, true };
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	auto result = std::max_element(queueIndices.begin(), queueIndices.end(), [](auto& ref, auto& ref2) {
		return std::get<uint32_t>(ref.second) < std::get<uint32_t>(ref2.second);
	});
	
	std::vector<float> queuePriorties(std::get<uint32_t>((*result).second), 1.0f);

	for (const auto& queueFamily : queueIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily.first;
		queueCreateInfo.queueCount = std::get<uint32_t>(queueFamily.second);
		queueCreateInfo.pQueuePriorities = queuePriorties.data();
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo logDeviceInfo{};
	logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logDeviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	logDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	logDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	logDeviceInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
	logDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
	logDeviceInfo.ppEnabledLayerNames = instanceLayers.data();
	logDeviceInfo.pEnabledFeatures = &features;

	VkResult res = vkCreateDevice(gpu, &logDeviceInfo, nullptr, &device);

	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical GPU, mate!");
	}

	descriptorLayoutCache.device = device;
	shaders.device = device;

	for (const auto queueFamily : queueIndices) {

		uint32_t queueIndex = queueFamily.first;
		uint32_t maxCount = std::get<uint32_t>(queueFamily.second);
		bool present = std::get<bool>(queueFamily.second);
		CreateQueueManager(queueIndex, maxCount, queueFlags, present);
	}
}

void VKDevice::WaitOnDevice()
{
	vkDeviceWaitIdle(device);
}

void VKDevice::GetExclusiveLock()
{
	deviceLock.lock();
}

void VKDevice::UnlockExclusiveLock()
{
	deviceLock.unlock();
}

void VKDevice::GetSharedLock()
{
	deviceLock.lock_shared();
}

static uint32_t FindQueueManagerByCapapbilites(std::vector<QueueManager*>& managers, uint32_t capabilities)
{
	uint32_t i = 0;
	for (;i<managers.size(); i++)
	{
		if ((managers[i]->queueCapabilities & capabilities) == capabilities) {
			return i;
		}
	}

	return ~0ui32;
}

std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> VKDevice::GetQueueHandle(uint32_t capabilites)
{
	auto ret = FindQueueManagerByCapapbilites(this->queueManagers, capabilites);
	uint32_t queueIdx = this->queueManagers[ret]->GetQueue();
	return { ret, queueIdx, this->queueManagers[ret]->queueFamilyIndex, this->queueManagers[ret]->poolIndices[queueIdx] };
}

void VKDevice::CreateQueueManager(uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport)
{
	std::shared_lock lock(deviceLock);
	queueManagers.emplace_back(new QueueManager(std::vector<uint32_t>{}, maxCount, queueIndex, queueFlags, presentsupport, *this));
}

void VKDevice::AllocateCommandPools(uint32_t size)
{
	commandPools.resize(size);
}

uint32_t VKDevice::CreateCommandPool(QueueIndex& queueIndex)
{
	std::shared_lock lock(deviceLock);
	uint32_t poolIndex = static_cast<uint32_t>(commandPools.size());
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueIndex;
	commandPools.push_back(VK_NULL_HANDLE);

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPools[poolIndex]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

	return poolIndex;
}

VkDescriptorPool VKDevice::CreateDesciptorPool(uint32_t& poolIndex, DescriptorPoolBuilder& builder, uint32_t maxSets)
{
	std::shared_lock lock(deviceLock);
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	uint32_t poolSizeCount = static_cast<uint32_t>(builder.poolSizes.size());

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = builder.poolSizes.data();
	poolInfo.maxSets = maxSets;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	descriptorPools.push_back(descriptorPool);
	poolIndex = static_cast<uint32_t>(descriptorPools.size() - 1);
	return descriptorPool;
}

std::pair<uint32_t, VkDeviceSize> VKDevice::FindImageMemoryIndexForPool(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps)
{
	std::shared_lock lock(deviceLock);
	VkImage image;
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = layers;

	imageInfo.format = type;//VK::API::ConvertSMBToVkFormat(type);
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = flags;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);
	uint32_t memoryTypeIndex = VK::Utils::findMemoryType(gpu, memRequirements.memoryTypeBits, memProps);

	vkDestroyImage(device, image, nullptr);

	return std::make_pair(memoryTypeIndex, memRequirements.alignment);

}

uint32_t VKDevice::CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex)
{
	std::shared_lock lock(deviceLock);
	VkDeviceMemory deviceMemory;

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = poolSize;
	allocInfo.memoryTypeIndex = memoryTypeIndex;

	if (vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	deviceMemories.push_back(deviceMemory);
	allocators.emplace_back(poolSize);
	return static_cast<uint32_t>(deviceMemories.size() - 1);
}

uint32_t VKDevice::CreateSwapChain(VkSurfaceKHR surface)
{
	std::shared_lock lock(deviceLock);
	uint32_t index = static_cast<uint32_t>(swapChains.size());
	swapChains.emplace_back(this, surface);
	return index;
}

uint32_t VKDevice::CreateSwapChain(VkSurfaceKHR surface, uint32_t attachmentCount)
{
	std::shared_lock lock(deviceLock);
	uint32_t index = static_cast<uint32_t>(swapChains.size());
	swapChains.emplace_back(this, surface, attachmentCount);
	return index;
}


ImageIndex VKDevice::CreateImage(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps, uint32_t memIndex)
{
	std::shared_lock lock(deviceLock);
	VkImage image;
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = layers;

	imageInfo.format = type;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = flags;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	auto iter = deviceMemories[memIndex];

	auto& alloc = allocators[memIndex];

	VkDeviceSize addr = alloc.GetMemory(memRequirements.size, memRequirements.alignment);

	vkBindImageMemory(device, image, iter, addr);

	images.push_back(std::tuple(image, addr, memIndex));

	return std::move(ImageIndex(images.size() - 1));
}

ImageIndex VKDevice::CreateImageView(
	ImageIndex& imageIndex, uint32_t mipLevels,
	VkFormat type, VkImageAspectFlags aspectMask)
{
	std::shared_lock lock(deviceLock);
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = std::get<VkImage>(images[imageIndex]);
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = type;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	VkImageView imageView = VK_NULL_HANDLE;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	imageViews.push_back(imageView);

	return std::move(ImageIndex(imageViews.size() - 1));
}

ImageIndex VKDevice::CreateImageView(
	VkImage image, uint32_t mipLevels,
	VkFormat type, VkImageAspectFlags aspectMask)
{
	std::shared_lock lock(deviceLock);
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = type;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	VkImageView imageView = VK_NULL_HANDLE;
	if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	imageViews.push_back(imageView);

	return std::move(ImageIndex(imageViews.size() - 1));
}

BufferIndex VKDevice::CreateHostBuffer(VkDeviceSize allocSize, bool coherent, bool createAllocator, VkBufferUsageFlags usage)
{
	std::shared_lock lock(deviceLock);
	hostBuffers.emplace_back(VK_NULL_HANDLE, VK_NULL_HANDLE);
	auto& ref = hostBuffers.back();
	std::tie(ref.first, ref.second) = VK::Utils::createBuffer(device, gpu, allocSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), VK_SHARING_MODE_EXCLUSIVE, usage);

	uint32_t ret = static_cast<uint32_t>(hostBuffers.size() - 1);

	if (createAllocator)
	{
		hostAllocators.emplace_back(ret, allocSize);
	}

	return BufferIndex(ret);
}

ImageIndex VKDevice::CreateSampledImage(
	std::vector<std::vector<char>>& imageData,
	std::vector<uint32_t>& imageSizes,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, VkFormat type,
	uint32_t memIndex,
	uint32_t hostIndex)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(GRAPHICS | TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	uint32_t poolIndex = std::get<3>(queueDetails);

	VkQueue queue; 
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VkDeviceSize imagesSize = std::accumulate(imageSizes.begin(), imageSizes.end(), 0UL);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	std::tie(stagingBuffer, stagingMemory) = hostBuffers[hostIndex];

	char* data;
	auto& sizes = imageSizes;
	auto& pixels = imageData;
	vkMapMemory(device, stagingMemory, 0, imagesSize, 0, reinterpret_cast<void**>(&data));
	for (auto i = 0U; i < mipLevels; i++) {
		std::memcpy(data, pixels[i].data(), sizes[i]);
		data += sizes[i];
	}
	vkUnmapMemory(device, stagingMemory);

	VkFormat format = type;

	ImageIndex imageIndex = CreateImage(
		width, height, mipLevels, type, 1,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, commandPools[poolIndex]);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

	VkDeviceSize offset = 0U;

	for (auto i = 0U; i < mipLevels; i++) {

		VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, std::get<VkImage>(images[imageIndex]), width >> i, height >> i, i, offset, { 0, 0, 0 });

		offset += static_cast<VkDeviceSize>(sizes[i]);
	}

	VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, commandPools[poolIndex], cb);

	queueManagers[managerIndex]->ReturnQueue(queueIndex);

	return imageIndex;
}

void VKDevice::TransitionImageLayout(ImageIndex& imageIndex,
	VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
	uint32_t mips, uint32_t layers)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	uint32_t poolIndex = std::get<3>(queueDetails);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
	VK::Utils::TransitionImageLayout(
		device,
		commandPools[poolIndex], queue,
		std::get<VkImage>(images[imageIndex]), format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);
	queueManagers[managerIndex]->ReturnQueue(queueIndex);
}

ImageIndex VKDevice::CreateSampler(uint32_t mipLevels)
{
	std::shared_lock lock(deviceLock);
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

	auto ret = ImageIndex(samplers.size());

	samplers.emplace_back(VK_NULL_HANDLE);

	if (vkCreateSampler(device, &samplerInfo, nullptr, &samplers.back()) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}

	return ret;
}

void VKDevice::DestorySampler(ImageIndex& samplerIndex)
{
	std::shared_lock lock(deviceLock);
	vkDestroySampler(device, samplers[samplerIndex], nullptr);
	samplers[samplerIndex] = VK_NULL_HANDLE;
}

void VKDevice::DestroyImageView(ImageIndex& imageViewIndex)
{
	std::shared_lock lock(deviceLock);
	vkDestroyImageView(device, imageViews[imageViewIndex], nullptr);
	imageViews[imageViewIndex] = VK_NULL_HANDLE;
}

void VKDevice::DestroyImage(ImageIndex& imageIndex)
{
	std::shared_lock lock(deviceLock);
	vkDestroyImage(device, std::get<VkImage>(images[imageIndex]), nullptr);
	auto& alloc = allocators[std::get<uint32_t>(images[imageIndex])];
	alloc.FreeMemory(std::get<VkDeviceSize>(images[imageIndex])); //image, address, and memory type index
	images[imageIndex] = std::tuple(VK_NULL_HANDLE, 0, 0);
}

void VKDevice::WriteToHostBuffer(BufferIndex& hostIndex, void* data, size_t size, size_t offset)
{
	std::shared_lock lock(deviceLock);
	auto deviceMem = hostBuffers[hostIndex].second;
	void* datalocal;
	vkMapMemory(device, deviceMem, offset, size, 0, reinterpret_cast<void**>(&datalocal));
	std::memcpy(datalocal, data, size);
	vkUnmapMemory(device, deviceMem);
}

OffsetIndex VKDevice::GetOffsetIntoHostBuffer(BufferIndex& hostIndex, size_t size, uint32_t alignment)
{
	std::shared_lock lock(deviceLock);
	auto alloc = std::find_if(hostAllocators.begin(), hostAllocators.end(), [&hostIndex](auto& pair)
		{
			return hostIndex == pair.first;
		}
	);

	return OffsetIndex(alloc->second.GetMemory(size, alignment));
}

uint32_t VKDevice::CreateRenderPasses(VKRenderPassBuilder& builder)
{
	std::shared_lock lock(deviceLock);
	auto ref = &renderPasses.emplace_back(VK_NULL_HANDLE);
	uint32_t ret = static_cast<uint32_t>(renderPasses.size());
	if (vkCreateRenderPass(device, &builder.createInfo, nullptr, ref) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	return ret-1;
}

void VKDevice::CreateRenderGraph(uint32_t renderPass, std::vector<uint32_t> &dynamicOffsets, std::string perGraphDescriptor)
{
	std::shared_lock lock(deviceLock);

	uint32_t index = static_cast<uint32_t>(graphs.size());
	graphs.push_back( new VKRenderGraph(renderPass) );
	auto& graph = graphs[index];

	auto ret = graphMapping.insert({ renderPass,  index});
	//VKRenderGraph& iter = ret.first->second;
	if (!perGraphDescriptor.empty())
	{
		graph->descriptorname = perGraphDescriptor;
	}

	for (auto i : dynamicOffsets)
	{
		graph->dynamicOffsets.push_back(i);
	}
}

std::vector<uint32_t> VKDevice::CreateSemaphores(uint32_t count)
{
	std::shared_lock lock(deviceLock);
	uint32_t startingIndex = static_cast<uint32_t>(semaphores.size());
	std::vector<uint32_t> ret{ startingIndex };
	semaphores.emplace_back(VK_NULL_HANDLE);
	for (uint32_t i = 1; i < count; i++) {
		ret.push_back(startingIndex + i);
		semaphores.emplace_back(VK_NULL_HANDLE);
	}

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (uint32_t i = 0; i < count; i++) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphores[startingIndex + i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
	}

	return ret;
}

std::vector<uint32_t> VKDevice::CreateFences(uint32_t first, uint32_t count, VkFenceCreateFlags flags)
{
	std::shared_lock lock(deviceLock);
	uint32_t startingIndex = first;
	std::vector<uint32_t> ret(count);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = flags;

	for (uint32_t i = 0; i < count; i++) {
		if (vkCreateFence(device, &fenceInfo, nullptr, &fences[startingIndex + i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fences!");
		}

		ret[i] = startingIndex + i;
	}

	return ret;
}

uint32_t VKDevice::BeginFrameForSwapchain(uint32_t swapChainIndex, uint32_t requestedImage)
{
	std::shared_lock lock(deviceLock);
	VKSwapChain& swapChain = swapChains[swapChainIndex];

	uint32_t imageIndex = swapChain.AcquireNextSwapChainImage(UINT64_MAX, requestedImage);

	return imageIndex;
}

uint32_t VKDevice::SubmitCommandBuffer(
	std::vector<uint32_t>& wait,
	std::vector<VkPipelineStageFlags>& waitStages,
	std::vector<uint32_t>& signal,
	uint32_t cbIndex)
{
	std::shared_lock lock(deviceLock);
	auto& vkcb = commandBuffers[cbIndex];
	VkQueue queue;
	vkGetDeviceQueue(device, vkcb.queueFamilyIndex, vkcb.queueIndex, &queue);

	uint32_t woCount = static_cast<uint32_t>(wait.size());
	uint32_t signalCount = static_cast<uint32_t>(signal.size());

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::vector<VkSemaphore> waitSemaphores(woCount, VK_NULL_HANDLE);
	std::vector<VkSemaphore> signalSemaphores(signalCount, VK_NULL_HANDLE);

	uint32_t i;
	for (i = 0; i < woCount; i++)
	{
		waitSemaphores[i] = semaphores[wait[i]];
	}

	for (i = 0; i < signalCount; i++)
	{
		signalSemaphores[i] = semaphores[signal[i]];
	}

	submitInfo.waitSemaphoreCount = woCount;
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.data();

	submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkcb.buffer;


	if (vkQueueSubmit(queue, 1, &submitInfo, fences[vkcb.fenceIdx]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	return 1;
}

uint32_t VKDevice::SubmitCommandsForSwapChain(uint32_t swapChainIdx, uint32_t frameIndex, uint32_t cbIndex)
{
	//std::shared_lock lock(deviceLock);
	VKSwapChain& swc = swapChains[swapChainIdx];
	auto& depends = swc.dependencies.chains[frameIndex];
	std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	return SubmitCommandBuffer(depends[0], waitStages, depends[1], cbIndex);
}

uint32_t VKDevice::PresentSwapChain(uint32_t swapChainIdx, uint32_t frameIdx, uint32_t commandBufferIndex)
{
	std::shared_lock lock(deviceLock);
	VkQueue queue;
	uint32_t managerIndex, queueIndex, queueFamilyIndex;
	if (commandBufferIndex == ~0U)
	{
		auto queueDetails = GetQueueHandle(PRESENT);
		managerIndex = std::get<0>(queueDetails);
		queueIndex = std::get<1>(queueDetails);
		queueFamilyIndex = std::get<2>(queueDetails);
	}
	else {
		auto rbo = commandBuffers[commandBufferIndex];
		queueIndex = rbo.queueIndex;
		queueFamilyIndex = rbo.queueFamilyIndex;
	}
	

	

	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VKSwapChain& swc = swapChains[swapChainIdx];
	auto& depends = swc.dependencies.chains[frameIdx];
	auto& waitIndices = depends[1];

	uint32_t waitCount = static_cast<uint32_t>(waitIndices.size());
	std::vector<VkSemaphore> waitSemaphores(waitCount, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < waitCount; i++)
	{
		waitSemaphores[i] = semaphores[depends[1][i]];
	}


	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = waitCount;
	presentInfo.pWaitSemaphores = waitSemaphores.data();

	VkSwapchainKHR swapChains[] = { swc.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &frameIdx;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(queue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return 0;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
	
	if (commandBufferIndex == ~0U)
	{
		auto& ref = queueManagers[managerIndex];
		ref->ReturnQueue(queueIndex);
	}

	return 1;
}

std::vector<uint32_t> VKDevice::CreateReusableCommandBuffers( 
	uint32_t numberOfCommandBuffers, uint32_t firstCommandBuffer, VkCommandBufferLevel level, bool createFences, uint32_t capabilites)
{

	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(capabilites);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	uint32_t poolIndex = std::get<3>(queueDetails);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

	allocInfo.commandPool = commandPools[poolIndex];
	allocInfo.level = level;
	allocInfo.commandBufferCount = numberOfCommandBuffers;

	std::vector<VkCommandBuffer> l(numberOfCommandBuffers);

	if (vkAllocateCommandBuffers(device, &allocInfo, l.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	std::vector<uint32_t> ret(numberOfCommandBuffers);

	uint32_t j = 0;

	for (uint32_t i = firstCommandBuffer; i < firstCommandBuffer + numberOfCommandBuffers; i++, j++) {
		auto& iter = commandBuffers[i];
		iter.buffer = l[j];
		iter.queueFamilyIndex = queueFamilyIndex;
		iter.queueIndex = queueIndex;
		iter.poolIndex = poolIndex;
		ret[j] = i;
	}

	if (createFences)
	{
		auto fencesidx = CreateFences(firstCommandBuffer, numberOfCommandBuffers, VK_FENCE_CREATE_SIGNALED_BIT);
		j = 0;
		for (uint32_t i = firstCommandBuffer; i < firstCommandBuffer + numberOfCommandBuffers; i++, j++) {
			auto& iter = commandBuffers[i];
			iter.fenceIdx = fencesidx[j];
		}
	}

	return ret;
}

uint32_t VKDevice::GetCommandBufferIndex(uint64_t timeout)
{
	std::shared_lock lock(deviceLock);
	uint32_t index = 0;
	for (auto& vkcb : commandBuffers)
	{
		if (vkcb.fenceIdx != ~0U)
		{
			VkResult res = vkWaitForFences(device, 1, &fences[vkcb.fenceIdx], VK_TRUE, timeout);
			if (res == VK_SUCCESS)
			{
				vkResetFences(device, 1, &fences[vkcb.fenceIdx]);
				vkResetCommandBuffer(vkcb.buffer, 0);
				return index;
			}
		}
		else {
			return index;
		}
		index++;
	}
	return ~0u;
}

uint32_t VKDevice::RequestWithPossibleBufferResetAndFenceReset(uint64_t timeout, uint32_t bufferIndex, bool reset, bool fenceReset)
{
	std::shared_lock lock(deviceLock);
	auto& vkcb = commandBuffers[bufferIndex];

	if (vkcb.fenceIdx == ~0U)
		return bufferIndex;

	VkResult res = vkWaitForFences(device, 1, &fences[vkcb.fenceIdx], VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return ~0U;

	if (fenceReset)
		vkResetFences(device, 1, &fences[vkcb.fenceIdx]);
	
	if (reset)
		vkResetCommandBuffer(vkcb.buffer, 0);

	return bufferIndex;
}

int32_t VKDevice::WaitOnCommandBufferAndPossibleResetFence(uint64_t timeout, uint32_t bufferIndex, bool resetfence)
{
	std::shared_lock lock(deviceLock);
	auto& vkcb = commandBuffers[bufferIndex];

	if (vkcb.fenceIdx == ~0U)
		return 0;

	VkResult res = vkWaitForFences(device, 1, &fences[vkcb.fenceIdx], VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return -1;
	if (resetfence)
		vkResetFences(device, 1, &fences[vkcb.fenceIdx]);

	return 0;
}

uint32_t VKDevice::CreateFrameBuffer(std::vector<uint32_t>& attachmentIndices, uint32_t renderPassIndex, VkExtent2D& extent)
{
	std::shared_lock lock(deviceLock);
	uint32_t ret = static_cast<uint32_t>(frameBuffers.size());
	uint32_t attachmentsCount = static_cast<uint32_t>(attachmentIndices.size());
	std::vector<VkImageView> attachments(attachmentsCount);

	for (uint32_t i = 0; i < attachmentsCount; i++) attachments[i] = imageViews[attachmentIndices[i]];

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPasses[renderPassIndex];
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	frameBuffers.push_back(VK_NULL_HANDLE);

	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffers[ret]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	}

	return ret;
}

uint32_t VKDevice::GetFamiliesOfCapableQueues(std::vector<uint32_t>& queueFamilies, uint32_t capabilities)
{
	std::shared_lock lock(deviceLock);
	auto iter = queueManagers.begin();
	while (capabilities && iter != queueManagers.end())
	{
		uint32_t comp = capabilities;
		capabilities &= ~((*iter)->queueCapabilities & capabilities);
		if (comp != capabilities) queueFamilies.push_back((*iter)->queueFamilyIndex);
		iter = std::next(iter);
	}
	return capabilities;
}

VkCommandBuffer VKDevice::GetCommandBufferHandle(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return commandBuffers[index].buffer;
}

VkImageView VKDevice::GetImageView(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return imageViews[index];
}

VkCommandPool VKDevice::GetCommandPool(uint32_t poolIndex)
{
	std::shared_lock lock(deviceLock);
	return commandPools[poolIndex];
}

VkDescriptorPool VKDevice::GetDescriptorPool(uint32_t poolIndex)
{
	std::shared_lock lock(deviceLock);
	return descriptorPools[poolIndex];
}

VKSwapChain& VKDevice::GetSwapChain(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return swapChains[index];
}

VkRenderPass VKDevice::GetRenderPass(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return renderPasses[index];
}

VkFence VKDevice::GetFence(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return fences[index];
}

VkSemaphore VKDevice::GetSemaphore(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return semaphores[index];
}

VkFramebuffer VKDevice::GetFrameBuffer(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return frameBuffers[index];
}

VKShaderCache& VKDevice::GetShaders()
{
	return shaders;
}

VKDescriptorSetCache& VKDevice::GetDescriptorSets()
{
	return descriptorSetCache;
}

VKDescriptorLayoutCache& VKDevice::GetDescriptorLayouts()
{
	return descriptorLayoutCache;
}

void VKDevice::CreatePipelineCache(uint32_t renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	renderPassPipelineCache.insert({ renderPassIndex, VKPipelineCache(device, renderPasses[renderPassIndex], this) });
}

VKPipelineCache& VKDevice::GetPipelineCache(uint32_t renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	return renderPassPipelineCache[renderPassIndex];
}

DescriptorSetBuilder VKDevice::CreateDescriptorSetBuilder(uint32_t poolIndex, std::string layoutname, uint32_t numberofsets)
{
	std::shared_lock lock(deviceLock);
	DescriptorSetBuilder dsb{device, &descriptorSetCache};
	auto ref = descriptorLayoutCache.GetLayout(layoutname);
	dsb.AllocDescriptorSets(descriptorPools[poolIndex], ref, numberofsets);
	return dsb;
}

DescriptorSetLayoutBuilder VKDevice::CreateDescriptorSetLayoutBuilder()
{
	return { device, &descriptorLayoutCache };
}

RecordingBufferObject VKDevice::GetRecordingBufferObject(uint32_t commandBufferIndex)
{
	std::shared_lock lock(deviceLock);
	return { *this, commandBuffers[commandBufferIndex] };
}

uint32_t VKDevice::CreateRenderTarget(uint32_t renderPassIndex, uint32_t framebufferCount)
{
	std::shared_lock lock(deviceLock);
	uint32_t ret = static_cast<uint32_t>(renderTargets.size());
	renderTargets.emplace_back(renderPassIndex, framebufferCount);
	return ret;
}

RenderTarget& VKDevice::GetRenderTarget(uint32_t renderTargetIndex)
{
	std::shared_lock lock(deviceLock);
	return renderTargets[renderTargetIndex];
}

QueueManager::QueueManager(std::vector<uint32_t> _cqs, int32_t _mqc, uint32_t _qfi, uint32_t _queueCapabilities, bool present, VKDevice& _d) :
	bitmap(0U),
	maxQueueCount(_mqc),
	queueFamilyIndex(_qfi),
	queueCapabilities(ConvertQueueProps(_queueCapabilities, present)),
	device(_d),
	sema(Semaphore(_mqc))
{

	assert(maxQueueCount <= 16);

	poolIndices.resize(_mqc);

	QueueIndex index = QueueIndex{ _qfi };

	for (auto& i : poolIndices)
	{
		i = _d.CreateCommandPool(index);
	}

	for (uint32_t i = 0; i < _cqs.size(); i++)
	{
		bitmap.set(_cqs[i]);
	}
}

uint32_t QueueManager::GetQueue()
{
	sema.Wait();
	std::scoped_lock lock(bitwiseMutex);
	for (int32_t i = 0; i < maxQueueCount; i++)
	{
		if (!bitmap.test(i))
		{
			bitmap.set(i);
			return i;
		}
	}
	return 0;
}

void QueueManager::ReturnQueue(int32_t queueNum)
{
	{
		std::scoped_lock lock(bitwiseMutex);
		bitmap.reset(queueNum);
	}
	sema.Notify();
}

uint32_t QueueManager::ConvertQueueProps(uint32_t flags, bool present)
{
	uint32_t flag = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) * GRAPHICS;
	flag |= ((flags & VK_QUEUE_COMPUTE_BIT) != 0) * COMPUTE;
	flag |= ((flags & VK_QUEUE_TRANSFER_BIT) != 0) * TRANSFER;
	flag |= (present * PRESENT);
	return flag;
}