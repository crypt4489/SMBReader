#include "VKDevice.h"
#include "VKRenderGraph.h"
#include "VKInstance.h"
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
	VkBuffer buffer = vkDeviceHandle.GetHostBuffer(bufferIndex);
	vkCmdBindVertexBuffers(cbBufferHandler.buffer, firstBindingCount, bindingCount, &buffer, offsets);
}

void RecordingBufferObject::BindingDrawCmd(uint32_t first, uint32_t drawSize)
{
	vkCmdDraw(cbBufferHandler.buffer, drawSize, 1, first, 0);
}

void RecordingBufferObject::BindingIndirectDrawCmd(uint32_t indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset)
{
	VkBuffer buffer = vkDeviceHandle.GetHostBuffer(indirectBufferIndex);
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
	renderPassInfo.renderPass = vkDeviceHandle.GetRenderPass(ref.renderPassIndex);
	renderPassInfo.framebuffer = vkDeviceHandle.GetFrameBuffer(ref.framebufferIndices[imageIndex]);
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

template<typename T_VkType = uintptr_t>
static T_VkType GetVkTypeFromEntry(VKDevice* device, size_t index)
{
	T_VkType ret = (T_VkType)device->entries[index];
	return ret;


}

template<typename T_VkType>
static size_t AddVkTypeToEntry(VKDevice* device, T_VkType handle)
{
	size_t ret = device->indexForEntries;
	device->entries[device->indexForEntries++] = (uintptr_t)handle;
	return ret;
}


template<typename T_Type>
static T_Type AllocTypeFromEntry(VKDevice* device, size_t size)
{
	uintptr_t head = (uintptr_t)device->perDeviceData;
	uintptr_t ret = head + device->perDeviceOffset;
	device->perDeviceOffset += size;
	return (T_Type)ret;
}


VKDevice::VKDevice(VkPhysicalDevice _gpu, VKInstance *_inst) : gpu(_gpu), parentInstance(_inst), device(VK_NULL_HANDLE)
{
}

VKDevice::~VKDevice()
{
	if (device)
	{
		vkDeviceWaitIdle(device);

		shaders.DestroyShaderCache();

		descriptorLayoutCache.DestroyLayoutCache();

		//for (auto& sc : swapChains)
		{
			//sc.DestroySwapChain();
		}

		if (perDeviceData)
		{
			delete[] perDeviceData;
		}

		if (entries)
		{
			delete[] entries;
		}
	}
}

VKDevice& VKDevice::operator=(VKDevice&& _dev) noexcept {
	this->device = _dev.device;
	_dev.device = VK_NULL_HANDLE;
	this->gpu = _dev.gpu;
	_dev.gpu = VK_NULL_HANDLE;
	
	//this->swapChains = std::move(_dev.swapChains);
	this->images = std::move(_dev.images);
	this->imageViews = std::move(_dev.imageViews);
	this->shaders = std::move(_dev.shaders);
	return *this;
};

VKDevice::VKDevice(VKDevice&& _dev)  noexcept
{
	this->device = _dev.device;
	_dev.device = VK_NULL_HANDLE;
	this->gpu = _dev.gpu;
	_dev.gpu = VK_NULL_HANDLE;
	//this->swapChains = std::move(_dev.swapChains);
	this->images = std::move(_dev.images);
	this->imageViews = std::move(_dev.imageViews);
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
	VkSurfaceKHR renderSurface,
	size_t perDeviceDataSize,
	float deviceRatio
)
{

	if (perDeviceDataSize)
	{
		
		size_t entriesalloc = deviceRatio * perDeviceDataSize;

		perDeviceSize = perDeviceDataSize - entriesalloc;
		perDeviceData = (void*)new char[perDeviceSize];
		perDeviceOffset = 0;

		numberOfEntries = entriesalloc / sizeof(uintptr_t);
		entries = new uintptr_t[numberOfEntries];
		indexForEntries = 0;
	}

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


	QueueManager* ptr = AllocTypeFromEntry<QueueManager*>(this, sizeof(QueueManager) * queueIndices.size());

	queueManagersSize = queueIndices.size();

	queueManagers = AddVkTypeToEntry(this, ptr);

	for (const auto queueFamily : queueIndices) {

		uint32_t queueIndex = queueFamily.first;
		uint32_t maxCount = std::get<uint32_t>(queueFamily.second);
		bool present = std::get<bool>(queueFamily.second);
		CreateQueueManager(ptr, queueIndex, maxCount, queueFlags, present);
		ptr = std::next(ptr);
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

static uint32_t FindQueueManagerByCapapbilites(QueueManager* managers, size_t managerSize, uint32_t capabilities)
{
	size_t i = 0;
	for (;i<managerSize; i++)
	{
		if ((managers[i].queueCapabilities & capabilities) == capabilities) {
			return i;
		}
	}

	return ~0ui32;
}

std::tuple<uint32_t, uint32_t, uint32_t, uint32_t> VKDevice::GetQueueHandle(uint32_t capabilites)
{
	QueueManager* ptr = GetVkTypeFromEntry<QueueManager*>(this, queueManagers);
	auto ret = FindQueueManagerByCapapbilites(ptr, queueManagersSize, capabilites);
	uint32_t queueIdx = ptr[ret].GetQueue();
	uint32_t managerIndex = queueManagers;
	managerIndex += ret;
	return { managerIndex, queueIdx, ptr[ret].queueFamilyIndex, ptr[ret].poolIndices[queueIdx] };
}

void VKDevice::CreateQueueManager(QueueManager *manager, uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport)
{
	std::shared_lock lock(deviceLock);
	std::construct_at(manager, std::vector<uint32_t>{}, maxCount, queueIndex, queueFlags, presentsupport, *this);
}

void VKDevice::AllocateCommandPools(uint32_t size)
{
	//commandPools.resize(size);
}

uint32_t VKDevice::CreateCommandPool(QueueIndex& queueIndex)
{
	std::shared_lock lock(deviceLock);
	//uint32_t poolIndex = static_cast<uint32_t>(commandPools.size());
	
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueIndex;

	VkCommandPool pool = nullptr;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

	uint32_t poolIndex = static_cast<uint32_t>(AddVkTypeToEntry(this, pool));

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


	poolIndex = static_cast<uint32_t>(AddVkTypeToEntry(this, descriptorPool));

	
	
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

	uint32_t ret = AddVkTypeToEntry(this, deviceMemory);

	VKAllocator* alloc = AllocTypeFromEntry<VKAllocator*>(this, sizeof(VKAllocator));

	std::construct_at(alloc, poolSize);

	uint32_t unused = AddVkTypeToEntry(this, alloc);

	return ret;
}


uint32_t VKDevice::CreateSwapChain(uint32_t attachmentCount, uint32_t requestedImageCount, uint32_t maxSemaphorePerStage, uint32_t stages)
{

	std::shared_lock lock(deviceLock);
	uint32_t index;

	{
		VKSwapChain* swc = AllocTypeFromEntry<VKSwapChain*>(this, sizeof(VKSwapChain));

		auto swcsupport = parentInstance->GetSwapChainSupport(gpu);

		swc = std::construct_at(swc, this, parentInstance->renderSurface, attachmentCount, requestedImageCount, swcsupport, stages, maxSemaphorePerStage);

		void* swcperdata = AllocTypeFromEntry<void*>(this,
			sizeof(uintptr_t) * swc->imageCount * (1 + swc->attachmentCount)  +
			sizeof(VkImage) * swc->imageCount +
			sizeof(uint32_t) * 2 +
			sizeof(uintptr_t) * (swc->imageCount * (maxSemaphorePerStage * stages) + swc->imageCount)
		);

		swc->SetSWCLocalData(swcperdata);

		index = AddVkTypeToEntry(this, swc);
	}

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

	auto iter = GetVkTypeFromEntry<VkDeviceMemory>(this, memIndex);

	auto alloc = GetVkTypeFromEntry<VKAllocator*>(this, memIndex+1);

	VkDeviceSize addr = alloc->GetMemory(memRequirements.size, memRequirements.alignment);

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
	
	VkBuffer buffer;
	VkDeviceMemory memory;

	std::tie(buffer, memory) = VK::Utils::createBuffer(device, gpu, allocSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), VK_SHARING_MODE_EXCLUSIVE, usage);

	uint32_t ret = AddVkTypeToEntry(this, buffer);

	uint32_t memLoc = AddVkTypeToEntry(this, memory);

	if (createAllocator)
	{
		VKAllocator* alloc = AllocTypeFromEntry<VKAllocator*>(this, sizeof(VKAllocator));

		std::construct_at(alloc, allocSize);

		uint32_t unused = AddVkTypeToEntry(this, alloc);
	}

	return BufferIndex(ret);
}

void VKDevice::ReturnQueueToManager(size_t queueManagerIndex, size_t queueIndex)
{
	auto ptr = GetVkTypeFromEntry<QueueManager*>(this, queueManagers);

	ptr[queueManagerIndex].ReturnQueue(queueIndex);
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

	stagingBuffer = GetVkTypeFromEntry<VkBuffer>(this, hostIndex);
	stagingMemory = GetVkTypeFromEntry<VkDeviceMemory>(this, hostIndex+1);

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

	VkCommandPool pool = GetVkTypeFromEntry<VkCommandPool>(this, static_cast<size_t>(poolIndex));

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

	VkDeviceSize offset = 0U;

	for (auto i = 0U; i < mipLevels; i++) {

		VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, std::get<VkImage>(images[imageIndex]), width >> i, height >> i, i, offset, { 0, 0, 0 });

		offset += static_cast<VkDeviceSize>(sizes[i]);
	}

	VK::Utils::MultiCommands::TransitionImageLayout(cb, std::get<VkImage>(images[imageIndex]), format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(managerIndex, queueIndex);

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

	VkCommandPool pool = GetVkTypeFromEntry<VkCommandPool>(this, static_cast<size_t>(poolIndex));

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
	VK::Utils::TransitionImageLayout(
		device,
		pool, queue,
		std::get<VkImage>(images[imageIndex]), format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);

	ReturnQueueToManager(managerIndex, queueIndex);
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
	auto alloc = GetVkTypeFromEntry<VKAllocator*>(this, std::get<uint32_t>(images[imageIndex])+1);
	alloc->FreeMemory(std::get<VkDeviceSize>(images[imageIndex])); //image, address, and memory type index
	images[imageIndex] = std::tuple(VK_NULL_HANDLE, 0, 0);
}

void VKDevice::WriteToHostBuffer(BufferIndex& hostIndex, void* data, size_t size, size_t offset)
{
	std::shared_lock lock(deviceLock);
	uint32_t index = hostIndex;
	auto deviceMem = GetVkTypeFromEntry<VkDeviceMemory>(this, index + 1);;
	void* datalocal;
	vkMapMemory(device, deviceMem, offset, size, 0, reinterpret_cast<void**>(&datalocal));
	std::memcpy(datalocal, data, size);
	vkUnmapMemory(device, deviceMem);
}

OffsetIndex VKDevice::GetOffsetIntoHostBuffer(BufferIndex& hostIndex, size_t size, uint32_t alignment)
{
	std::shared_lock lock(deviceLock);

	uint32_t index = hostIndex;

	auto alloc = GetVkTypeFromEntry<VKAllocator*>(this, index + 2);
	
	return OffsetIndex(alloc->GetMemory(size, alignment));
}

uint32_t VKDevice::CreateRenderPasses(VKRenderPassBuilder& builder)
{
	std::shared_lock lock(deviceLock);
	
	VkRenderPass ref = VK_NULL_HANDLE;

	if (vkCreateRenderPass(device, &builder.createInfo, nullptr, &ref) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	uint32_t ret;
	
	{
		auto renderPassDataHandle = AllocTypeFromEntry<void*>(this, sizeof(VKRenderGraph) + sizeof(VKPipelineCache));

		ret = AddVkTypeToEntry(this, ref);

		AddVkTypeToEntry(this, renderPassDataHandle);
	}

	CreatePipelineCache(ret);

	CreateRenderGraph(ret);

	return ret;
}

void VKDevice::UpdateRenderGraph(uint32_t renderPass, std::vector<uint32_t>& dynamicOffsets, std::string perGraphDescriptor)
{
	std::shared_lock lock(deviceLock);

	auto graph = GetVkTypeFromEntry<VKRenderGraph*>(this, renderPass + 1);

	if (!perGraphDescriptor.empty())
	{
		graph->descriptorname = perGraphDescriptor;
	}

	for (auto i : dynamicOffsets)
	{
		graph->dynamicOffsets.push_back(i);
	}
}

void VKDevice::CreateRenderGraph(uint32_t renderPass)
{
	std::shared_lock lock(deviceLock);

	auto renderPassData = GetVkTypeFromEntry<VKRenderGraph*>(this, renderPass + 1);

	auto graph = std::construct_at(renderPassData, renderPass);
}

std::vector<uint32_t> VKDevice::CreateSemaphores(uint32_t count)
{
	std::shared_lock lock(deviceLock);

	std::vector<uint32_t> ret(count);

	VkSemaphore sema = nullptr;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (uint32_t i = 0; i < count; i++) 
	{	
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sema) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
		ret[i] = static_cast<uint32_t>(AddVkTypeToEntry(this, sema));
	}

	return ret;
}

std::vector<uint32_t> VKDevice::CreateFences(uint32_t count, VkFenceCreateFlags flags)
{
	std::shared_lock lock(deviceLock);
	std::vector<uint32_t> ret(count);

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = flags;

	VkFence fence = nullptr;

	for (uint32_t i = 0; i < count; i++) {
		if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fences!");
		}

		ret[i] = static_cast<uint32_t>(AddVkTypeToEntry(this, fence));
	}

	return ret;
}

uint32_t VKDevice::BeginFrameForSwapchain(uint32_t swapChainIndex, uint32_t requestedImage)
{
	std::shared_lock lock(deviceLock);
	VKSwapChain& swapChain = GetSwapChain(swapChainIndex);

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
	auto vkcb = GetCommandBuffer(cbIndex);
	VkQueue queue;
	vkGetDeviceQueue(device, vkcb->queueFamilyIndex, vkcb->queueIndex, &queue);

	uint32_t woCount = static_cast<uint32_t>(wait.size());
	uint32_t signalCount = static_cast<uint32_t>(signal.size());

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	std::vector<VkSemaphore> waitSemaphores(woCount, VK_NULL_HANDLE);
	std::vector<VkSemaphore> signalSemaphores(signalCount, VK_NULL_HANDLE);

	uint32_t i;
	for (i = 0; i < woCount; i++)
	{
		waitSemaphores[i] = GetVkTypeFromEntry<VkSemaphore>(this, wait[i]);
	}

	for (i = 0; i < signalCount; i++)
	{
		signalSemaphores[i] = GetVkTypeFromEntry<VkSemaphore>(this, signal[i]);
	}

	submitInfo.waitSemaphoreCount = woCount;
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.data();

	submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalSemaphores.data();

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkcb->buffer;

	VkFence fence = GetVkTypeFromEntry<VkFence>(this, vkcb->fenceIdx);

	if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	return 1;
}

uint32_t VKDevice::SubmitCommandsForSwapChain(uint32_t swapChainIdx, uint32_t frameIndex, uint32_t cbIndex)
{
	//std::shared_lock lock(deviceLock);
	VKSwapChain& swc = GetSwapChain(swapChainIdx);
	auto depends = swc.GetDependenciesForImageIndex(frameIndex);
	std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	std::vector<uint32_t> dummie1{ static_cast<uint32_t>(depends[0]) };

	std::vector<uint32_t> dummie2{ static_cast<uint32_t>(depends[1]) };

	return SubmitCommandBuffer(dummie1, waitStages, dummie2, cbIndex);
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
		auto rbo = GetCommandBuffer(commandBufferIndex);
		queueIndex = rbo->queueIndex;
		queueFamilyIndex = rbo->queueFamilyIndex;
	}
	

	

	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VKSwapChain& swc = GetSwapChain(swapChainIdx);
	auto depends = swc.GetDependenciesForImageIndex(frameIdx);

	uint32_t waitCount = static_cast<uint32_t>(swc.semaphorePerStage);
	std::vector<VkSemaphore> waitSemaphores(waitCount, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < waitCount; i++)
	{
		waitSemaphores[i] = GetVkTypeFromEntry<VkSemaphore>(this, depends[waitCount+i]);
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
		ReturnQueueToManager(managerIndex, queueIndex);
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

	allocInfo.commandPool = GetVkTypeFromEntry<VkCommandPool>(this, poolIndex);
	allocInfo.level = level;
	allocInfo.commandBufferCount = numberOfCommandBuffers;

	std::vector<VkCommandBuffer> l(numberOfCommandBuffers);

	if (vkAllocateCommandBuffers(device, &allocInfo, l.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	std::vector<uint32_t> ret(numberOfCommandBuffers);
	std::vector<VKCommandBuffer*> temp(numberOfCommandBuffers);

	for (uint32_t i = 0; i < numberOfCommandBuffers; i++) {
		auto iter = AllocTypeFromEntry<VKCommandBuffer*>(this, sizeof(VKCommandBuffer));
		iter->buffer = l[i];
		iter->queueFamilyIndex = queueFamilyIndex;
		iter->queueIndex = queueIndex;
		iter->poolIndex = poolIndex;
		ret[i] = AddVkTypeToEntry(this, iter);
		temp[i] = iter;
	}

	if (createFences)
	{
		auto fencesidx = CreateFences(numberOfCommandBuffers, VK_FENCE_CREATE_SIGNALED_BIT);
		
		for (uint32_t i = 0; i < numberOfCommandBuffers; i++) {
			temp[i]->fenceIdx = fencesidx[i];
		}
	}

	return ret;
}

uint32_t VKDevice::GetCommandBufferIndex(uint64_t timeout)
{
	/*
	std::shared_lock lock(deviceLock);
	uint32_t index = 0;
	for (auto& vkcb : commandBuffers)
	{
		if (vkcb.fenceIdx != ~0U)
		{
			VkFence fence = GetVkTypeFromEntry<VkFence>(this, vkcb.fenceIdx);
			VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
			if (res == VK_SUCCESS)
			{
				vkResetFences(device, 1, &fence);
				vkResetCommandBuffer(vkcb.buffer, 0);
				return index;
			}
		}
		else {
			return index;
		}
		index++;
	} */
	return ~0u; 
}

uint32_t VKDevice::RequestWithPossibleBufferResetAndFenceReset(uint64_t timeout, uint32_t bufferIndex, bool reset, bool fenceReset)
{
	std::shared_lock lock(deviceLock);
	auto vkcb = GetCommandBuffer(bufferIndex);

	if (vkcb->fenceIdx == ~0U)
		return bufferIndex;

	VkFence fence = GetVkTypeFromEntry<VkFence>(this, vkcb->fenceIdx);

	VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return ~0U;

	if (fenceReset)
		vkResetFences(device, 1, &fence);
	
	if (reset)
		vkResetCommandBuffer(vkcb->buffer, 0);

	return bufferIndex;
}

int32_t VKDevice::WaitOnCommandBufferAndPossibleResetFence(uint64_t timeout, uint32_t bufferIndex, bool resetfence)
{
	std::shared_lock lock(deviceLock);
	auto vkcb = GetCommandBuffer(bufferIndex);

	if (vkcb->fenceIdx == ~0U)
		return 0;

	VkFence fence = GetVkTypeFromEntry<VkFence>(this, vkcb->fenceIdx);

	VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return -1;

	if (resetfence)
		vkResetFences(device, 1, &fence);

	return 0;
}

uint32_t VKDevice::CreateFrameBuffer(std::vector<size_t>& attachmentIndices, uint32_t renderPassIndex, VkExtent2D& extent)
{
	std::shared_lock lock(deviceLock);

	uint32_t attachmentsCount = static_cast<uint32_t>(attachmentIndices.size());
	std::vector<VkImageView> attachments(attachmentsCount);

	for (uint32_t i = 0; i < attachmentsCount; i++) attachments[i] = imageViews[attachmentIndices[i]];

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = GetRenderPass(renderPassIndex);
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	VkFramebuffer frameBuffer;

	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	}

	uint32_t ret = static_cast<uint32_t>(AddVkTypeToEntry(this, frameBuffer));

	return ret;
}

uint32_t VKDevice::GetFamiliesOfCapableQueues(std::vector<uint32_t>& queueFamilies, uint32_t capabilities)
{
	std::shared_lock lock(deviceLock);
	auto iter = GetVkTypeFromEntry<QueueManager*>(this, queueManagers);
	for (size_t i = 0; i<queueManagersSize&&capabilities; i++)
	{
		uint32_t comp = capabilities;
		capabilities &= ~(iter[i].queueCapabilities & capabilities);
		if (comp != capabilities) queueFamilies.push_back(iter[i].queueFamilyIndex);
	}
	return capabilities;
}

VKCommandBuffer* VKDevice::GetCommandBuffer(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VKCommandBuffer*>(this, index);
}

VkCommandBuffer VKDevice::GetCommandBufferHandle(uint32_t index)
{
	return GetCommandBuffer(index)->buffer;
}

VkImageView VKDevice::GetImageView(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return imageViews[index];
}

VkCommandPool VKDevice::GetCommandPool(uint32_t poolIndex)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkCommandPool>(this, poolIndex);
}

VkDescriptorPool VKDevice::GetDescriptorPool(uint32_t poolIndex)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkDescriptorPool>(this, poolIndex);
}

VKSwapChain& VKDevice::GetSwapChain(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return *GetVkTypeFromEntry<VKSwapChain*>(this, index);
}

VkRenderPass VKDevice::GetRenderPass(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkRenderPass>(this, index);
}

VkFence VKDevice::GetFence(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkFence>(this, index);
}

VkSemaphore VKDevice::GetSemaphore(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkSemaphore>(this, index);
}

VkFramebuffer VKDevice::GetFrameBuffer(uint32_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkFramebuffer>(this, index);
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
	//renderPassPipelineCache.insert({ renderPassIndex, VKPipelineCache(device, GetRenderPass(renderPassIndex), this) });

	auto renderPassData = reinterpret_cast<VKPipelineCache*>((GetVkTypeFromEntry(this, renderPassIndex + 1)+sizeof(VKRenderGraph)));

	auto renderPass = GetVkTypeFromEntry<VkRenderPass>(this, renderPassIndex);

	std::construct_at(renderPassData, device, renderPass, this);
}

VKPipelineCache& VKDevice::GetPipelineCache(uint32_t renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	return *reinterpret_cast<VKPipelineCache*>((GetVkTypeFromEntry(this, renderPassIndex + 1) + sizeof(VKRenderGraph)));;
}

VKRenderGraph& VKDevice::GetRenderGraph(size_t renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	auto renderPassData = GetVkTypeFromEntry<VKRenderGraph*>(this, renderPassIndex + 1);
	return *renderPassData;
}

DescriptorSetBuilder VKDevice::CreateDescriptorSetBuilder(uint32_t poolIndex, std::string layoutname, uint32_t numberofsets)
{
	std::shared_lock lock(deviceLock);
	DescriptorSetBuilder dsb{device, &descriptorSetCache};
	auto ref = descriptorLayoutCache.GetLayout(layoutname);
	dsb.AllocDescriptorSets(GetDescriptorPool(poolIndex), ref, numberofsets);
	return dsb;
}

DescriptorSetLayoutBuilder VKDevice::CreateDescriptorSetLayoutBuilder()
{
	return { device, &descriptorLayoutCache };
}

RecordingBufferObject VKDevice::GetRecordingBufferObject(uint32_t commandBufferIndex)
{
	std::shared_lock lock(deviceLock);
	return { *this, *GetCommandBuffer(commandBufferIndex) };
}

uint32_t VKDevice::CreateRenderTarget(uint32_t renderPassIndex, uint32_t framebufferCount)
{
	std::shared_lock lock(deviceLock);
	auto renderTarget = AllocTypeFromEntry<RenderTarget*>(this, sizeof(RenderTarget));
	std::construct_at(renderTarget, renderPassIndex, framebufferCount);
	return AddVkTypeToEntry(this, renderTarget);
}

RenderTarget& VKDevice::GetRenderTarget(uint32_t renderTargetIndex)
{
	std::shared_lock lock(deviceLock);
	return *GetVkTypeFromEntry<RenderTarget*>(this, renderTargetIndex);
}

VkBuffer VKDevice::GetHostBuffer(size_t index)
{
	std::shared_lock lock(deviceLock);
	return GetVkTypeFromEntry<VkBuffer>(this, index);	
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