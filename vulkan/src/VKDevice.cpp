#include "VKDevice.h"
#include "VKRenderGraph.h"
#include "VKInstance.h"
#include <mutex>
#include <shared_mutex>

RecordingBufferObject::RecordingBufferObject(VKDevice& device, VKCommandBuffer& buffer) :
	cbBufferHandler(buffer), vkDeviceHandle(device), currLayout(VK_NULL_HANDLE), currPipeline(VK_NULL_HANDLE)
{

}

void RecordingBufferObject::BindPipeline(EntryHandle renderTarget, std::string pipelinename)
{
	auto pbo = vkDeviceHandle.GetPipelineCache(renderTarget);
	auto pco = (*pbo)[pipelinename];
	currLayout = pco->pipelineLayout;
	currPipeline = pco->pipeline;
	vkCmdBindPipeline(cbBufferHandler.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currPipeline);
}

void RecordingBufferObject::BindDescriptorSets(std::string descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet, 
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	auto descsets = vkDeviceHandle.descriptorSetCache;
	auto descset = descsets.GetDescriptorSetPerFrame(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, currLayout, 
		firstDescriptorSet, descriptorCount, 
		&descset, dynamicOffsetCount, 
		offsets);
}

void RecordingBufferObject::BindVertexBuffer(EntryHandle bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets)
{
	VkBuffer buffer = vkDeviceHandle.GetHostBuffer(bufferIndex);
	vkCmdBindVertexBuffers(cbBufferHandler.buffer, firstBindingCount, bindingCount, &buffer, offsets);
}

void RecordingBufferObject::BindingDrawCmd(uint32_t first, uint32_t drawSize)
{
	vkCmdDraw(cbBufferHandler.buffer, drawSize, 1, first, 0);
}

void RecordingBufferObject::BindingIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset)
{
	VkBuffer buffer = vkDeviceHandle.GetHostBuffer(indirectBufferIndex);
	vkCmdDrawIndirect(cbBufferHandler.buffer, buffer, indirectBufferOffset, drawCount, sizeof(VkDrawIndirectCommand));
}

void RecordingBufferObject::EndRenderPassCommand()
{
	vkCmdEndRenderPass(cbBufferHandler.buffer);
}

void RecordingBufferObject::BeginRenderPassCommand(EntryHandle renderTargetIndex, uint32_t imageIndex,
	VkRect2D rect,
	VkClearColorValue color,
	VkClearDepthStencilValue depthStencil)
{

	VkRenderPassBeginInfo renderPassInfo{};
	auto ref = vkDeviceHandle.GetRenderTarget(renderTargetIndex);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vkDeviceHandle.GetRenderPass(ref->renderPassIndex);
	renderPassInfo.framebuffer = vkDeviceHandle.GetFrameBuffer(ref->framebufferIndices[imageIndex]);
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



static size_t FindQueueManagerByCapapbilites(QueueManager* managers, size_t managerSize, uint32_t capabilities)
{
	size_t i = 0;
	for (; i < managerSize; i++)
	{
		if ((managers[i].queueCapabilities & capabilities) == capabilities) {
			return i;
		}
	}

	return ~0ui64;
}


VKDevice::VKDevice(VkPhysicalDevice _gpu, VKInstance* _inst)
	: 
	device(VK_NULL_HANDLE),
	gpu(_gpu),
	parentInstance(_inst),
	queueManagers(),
	queueManagersSize(0),
	descriptorLayoutCache(),
	descriptorSetCache(),
	shaders(),
	deviceLock(),
	entries(nullptr),
	indexForEntries(0),
	numberOfEntries(0),
	perDeviceData(nullptr),
	perDeviceOffset(0),
	perDeviceSize(0),
	deviceLocalCache(nullptr),
	//cacheRead(0),
	cacheWrite(0),
	deviceLocalCacheSize(0)
{

}

VKDevice::~VKDevice()
{
	if (device)
	{
		vkDeviceWaitIdle(device);

		shaders.DestroyShaderCache();

		descriptorLayoutCache.DestroyLayoutCache();

		if (perDeviceData)
		{
			delete[] perDeviceData;
		}

		if (entries)
		{
			delete[] entries;
		}

		if (deviceLocalCache)
		{
			delete[] deviceLocalCache;
		}
	}
}

VKDevice& VKDevice::operator=(VKDevice&& _dev) noexcept {
	this->device = _dev.device;
	this->gpu = _dev.gpu;
	this->parentInstance = _dev.parentInstance;
	
	this->perDeviceData = _dev.perDeviceData;
	this->perDeviceOffset = _dev.perDeviceOffset;
	this->perDeviceSize = _dev.perDeviceSize;

	this->entries = _dev.entries;
	this->indexForEntries = _dev.indexForEntries;
	this->numberOfEntries = _dev.numberOfEntries;

	//this->cacheRead = _dev.cacheRead;
	this->cacheWrite = _dev.cacheWrite;
	this->deviceLocalCache = _dev.deviceLocalCache;
	this->deviceLocalCacheSize = _dev.deviceLocalCacheSize;

	_dev.device = VK_NULL_HANDLE;
	_dev.gpu = VK_NULL_HANDLE;
	_dev.parentInstance = nullptr;

	_dev.perDeviceData = nullptr;
	_dev.perDeviceOffset = ~0ui64;
	_dev.perDeviceSize = ~0ui64;

	_dev.entries = nullptr;
	_dev.indexForEntries = ~0ui64;
	_dev.numberOfEntries = ~0ui64;

	//_dev.cacheRead = ~0ui64;
	_dev.cacheWrite = ~0ui64;
	_dev.deviceLocalCache = nullptr;
	_dev.deviceLocalCacheSize = ~0ui64;

	return *this;
};

VKDevice::VKDevice(VKDevice&& _dev)  noexcept
{
	this->device = _dev.device;
	this->gpu = _dev.gpu;
	this->parentInstance = _dev.parentInstance;

	this->perDeviceData = _dev.perDeviceData;
	this->perDeviceOffset = _dev.perDeviceOffset;
	this->perDeviceSize = _dev.perDeviceSize;

	this->entries = _dev.entries;
	this->indexForEntries = _dev.indexForEntries;
	this->numberOfEntries = _dev.numberOfEntries;

	//this->cacheRead = _dev.cacheRead;
	this->cacheWrite = _dev.cacheWrite;
	this->deviceLocalCache = _dev.deviceLocalCache;
	this->deviceLocalCacheSize = _dev.deviceLocalCacheSize;

	_dev.device = VK_NULL_HANDLE;
	_dev.gpu = VK_NULL_HANDLE;
	_dev.parentInstance = nullptr;

	_dev.perDeviceData = nullptr;
	_dev.perDeviceOffset = ~0ui64;
	_dev.perDeviceSize = ~0ui64;

	_dev.entries = nullptr;
	_dev.indexForEntries = ~0ui64;
	_dev.numberOfEntries = ~0ui64;

	//_dev.cacheRead = 0ui64;
	_dev.cacheWrite = 0ui64;
	_dev.deviceLocalCache = nullptr;
	_dev.deviceLocalCacheSize = ~0ui64;
};

//allocations

EntryHandle VKDevice::AddVkTypeToEntry(void* handle)
{
	size_t ret = indexForEntries;
	entries[indexForEntries++] = reinterpret_cast<uintptr_t>(handle);
	return EntryHandle(ret);
}


void* VKDevice::AllocTypeFromEntry(size_t size)
{
	uintptr_t head = reinterpret_cast<uintptr_t>(perDeviceData);
	uintptr_t ret = head + perDeviceOffset;
	perDeviceOffset += size;
	return reinterpret_cast<void*>(ret);
}

void* VKDevice::GetVkTypeFromEntry(EntryHandle index)
{
	void* ret = reinterpret_cast<void*>(entries[index()]);
	return ret;
}


void* VKDevice::AllocFromDeviceCache(size_t size)
{
	if ((cacheWrite + size) >= deviceLocalCacheSize)
	{
		cacheWrite = 0;
	}

	uintptr_t head = reinterpret_cast<uintptr_t>(deviceLocalCache) + cacheWrite;

	cacheWrite += size;

	return reinterpret_cast<void*>(head);
}

//CREATORS



EntryHandle VKDevice::CreateCommandPool(QueueIndex& queueIndex)
{
	std::shared_lock lock(deviceLock);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueIndex;

	VkCommandPool pool = nullptr;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool!");
	}

	EntryHandle poolIndex = AddVkTypeToEntry(pool);

	return poolIndex;
}

EntryHandle VKDevice::CreateDesciptorPool(DescriptorPoolBuilder& builder, uint32_t maxSets)
{
	std::shared_lock lock(deviceLock);
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	uint32_t poolSizeCount = static_cast<uint32_t>(builder.poolSizesSize);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = builder.poolSizes;
	poolInfo.maxSets = maxSets;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}


	EntryHandle poolIndex = AddVkTypeToEntry(descriptorPool);

	return poolIndex;
}

DescriptorPoolBuilder VKDevice::CreateDescriptorPoolBuilder(size_t poolSize)
{

	VkDescriptorPoolSize* sizes = reinterpret_cast<VkDescriptorPoolSize*>(AllocFromDeviceCache(sizeof(VkDescriptorPoolSize) * poolSize));

	DescriptorPoolBuilder builder(sizes, poolSize);

	return builder;
}

DescriptorSetBuilder VKDevice::CreateDescriptorSetBuilder(EntryHandle poolIndex, std::string layoutname, uint32_t numberofsets)
{
	std::shared_lock lock(deviceLock);
	DescriptorSetBuilder dsb{ this, &descriptorSetCache, numberofsets };
	auto ref = descriptorLayoutCache.GetLayout(layoutname);
	dsb.AllocDescriptorSets(GetDescriptorPool(poolIndex), ref, numberofsets);
	return dsb;
}

DescriptorSetLayoutBuilder VKDevice::CreateDescriptorSetLayoutBuilder(uint32_t bindingCount)
{
	return { this, &descriptorLayoutCache, bindingCount };
}

EntryHandle* VKDevice::CreateFences(uint32_t count, VkFenceCreateFlags flags)
{
	std::shared_lock lock(deviceLock);

	EntryHandle* ret = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * count));

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = flags;

	VkFence fence = nullptr;

	for (uint32_t i = 0; i < count; i++) {
		if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fences!");
		}

		ret[i] = AddVkTypeToEntry(fence);
	}

	return ret;
}

EntryHandle VKDevice::CreateFrameBuffer(EntryHandle* attachmentIndices, uint32_t attachmentsCount, EntryHandle renderPassIndex, VkExtent2D& extent)
{
	std::shared_lock lock(deviceLock);

	VkImageView* attachments = reinterpret_cast<VkImageView*>(AllocFromDeviceCache(sizeof(VkImageView) * attachmentsCount));

	for (uint32_t i = 0; i < attachmentsCount; i++) {
		attachments[i] = GetImageView(attachmentIndices[i]);
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = GetRenderPass(renderPassIndex);
	framebufferInfo.attachmentCount = attachmentsCount;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	VkFramebuffer frameBuffer;

	if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	}

	EntryHandle ret = AddVkTypeToEntry(frameBuffer);

	return ret;
}

EntryHandle VKDevice::CreateHostBuffer(VkDeviceSize allocSize, bool coherent, bool createAllocator, VkBufferUsageFlags usage)
{
	std::shared_lock lock(deviceLock);

	VkBuffer buffer;
	VkDeviceMemory memory;

	std::tie(buffer, memory) = VK::Utils::createBuffer(device, gpu, allocSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), VK_SHARING_MODE_EXCLUSIVE, usage);

	EntryHandle ret;

	{
		ret = AddVkTypeToEntry(buffer);

		EntryHandle memLoc = AddVkTypeToEntry(memory);
	}

	if (createAllocator)
	{
		VKAllocator* alloc = reinterpret_cast<VKAllocator*>(AllocTypeFromEntry(sizeof(VKAllocator)));

		std::construct_at(alloc, allocSize);

		EntryHandle unused = AddVkTypeToEntry(alloc);
	}

	return ret;
}

EntryHandle VKDevice::CreateImage(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps, EntryHandle memIndex)
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

	auto iter = reinterpret_cast<VkDeviceMemory>(GetVkTypeFromEntry(memIndex));

	auto alloc = reinterpret_cast<VKAllocator*>(GetVkTypeFromEntry(memIndex + 1));

	VkDeviceSize addr = alloc->GetMemory(memRequirements.size, memRequirements.alignment);

	vkBindImageMemory(device, image, iter, addr);

	EntryHandle ret;

	{
		size_t* ptr = reinterpret_cast<size_t*>(AllocTypeFromEntry(sizeof(size_t) * 3));

		ptr[0] = reinterpret_cast<uintptr_t>(image);
		ptr[1] = addr;
		ptr[2] = memIndex;

		ret = AddVkTypeToEntry(ptr);
	}

	return ret;
}


EntryHandle VKDevice::CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex)
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

	EntryHandle ret = AddVkTypeToEntry(deviceMemory);

	VKAllocator* alloc = reinterpret_cast<VKAllocator*>(AllocTypeFromEntry(sizeof(VKAllocator)));

	std::construct_at(alloc, poolSize);

	EntryHandle unused = AddVkTypeToEntry(alloc);

	return ret;
}

EntryHandle VKDevice::CreateImageView(
	EntryHandle imageIndex, uint32_t mipLevels,
	VkFormat type, VkImageAspectFlags aspectMask)
{
	std::shared_lock lock(deviceLock);
	VkImageViewCreateInfo viewInfo{};

	VkImage image = GetImage(imageIndex);

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

	EntryHandle ret;

	{
		ret = AddVkTypeToEntry(imageView);
	}

	return ret;
}

EntryHandle VKDevice::CreateImageView(
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

	EntryHandle ret;

	{
		ret = AddVkTypeToEntry(imageView);
	}

	return ret;
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
#define CACHESIZE 2 * 1024
	if (perDeviceDataSize)
	{
		size_t entriesalloc = static_cast<size_t>(deviceRatio * perDeviceDataSize);

		deviceLocalCacheSize = CACHESIZE;
		deviceLocalCache = (void*)new char[deviceLocalCacheSize];

		perDeviceSize = perDeviceDataSize - entriesalloc;
		perDeviceData = (void*)new char[perDeviceSize];

		numberOfEntries = entriesalloc / sizeof(uintptr_t);
		entries = new uintptr_t[numberOfEntries];
	}

	std::unordered_map<uint32_t, std::tuple<uint32_t, bool>> queueIndices;
	uint32_t familyCount;
	VkQueueFamilyProperties* famProps;
	QueueIndex index, maxCount;

	famProps = QueueFamilyDetails(&familyCount);

	GetQueueByMask(index, maxCount, famProps, familyCount, queueFlags);

	queueIndices[index] = { maxCount, false };

	if (renderSurface)
	{
		GetPresentQueue(index, maxCount, famProps, familyCount, renderSurface);
		queueIndices[index] = { maxCount, true };
	}

	VkDeviceQueueCreateInfo* queueCreateInfos = reinterpret_cast<VkDeviceQueueCreateInfo*>(AllocFromDeviceCache(sizeof(VkDeviceQueueCreateInfo) * familyCount));

	auto result = std::max_element(queueIndices.begin(), queueIndices.end(), [](auto& ref, auto& ref2) {
		return std::get<uint32_t>(ref.second) < std::get<uint32_t>(ref2.second);
		});


	uint32_t prioSize = std::get<uint32_t>((*result).second);

	float* queuePriorites = reinterpret_cast<float*>(AllocFromDeviceCache(sizeof(float) * prioSize));

	for (uint32_t i = 0; i < prioSize; i++)
	{
		queuePriorites[i] = 1.0f;
	}

	uint32_t i = 0;

	for (const auto& queueFamily : queueIndices) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily.first;
		queueCreateInfo.queueCount = std::get<uint32_t>(queueFamily.second);
		queueCreateInfo.pQueuePriorities = queuePriorites;
		queueCreateInfos[i++] = queueCreateInfo;
	}

	VkDeviceCreateInfo logDeviceInfo{};
	logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logDeviceInfo.queueCreateInfoCount = i;
	logDeviceInfo.pQueueCreateInfos = queueCreateInfos;
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


	QueueManager* ptr = (QueueManager*)AllocTypeFromEntry(sizeof(QueueManager) * queueIndices.size());

	queueManagersSize = queueIndices.size();

	queueManagers = AddVkTypeToEntry(ptr);

	for (const auto queueFamily : queueIndices) {

		uint32_t queueIndex = queueFamily.first;
		uint32_t maxCount = std::get<uint32_t>(queueFamily.second);
		bool present = std::get<bool>(queueFamily.second);
		CreateQueueManager(ptr, queueIndex, maxCount, queueFlags, present);
		ptr = std::next(ptr);
	}
}


void VKDevice::CreatePipelineCache(EntryHandle renderPassIndex)
{
	std::shared_lock lock(deviceLock);

	auto renderPassData = GetPipelineCache(renderPassIndex);

	auto renderPass = GetRenderPass(renderPassIndex);

	std::construct_at(renderPassData, device, renderPass, this);
}

EntryHandle VKDevice::CreatePipelineObject(VKPipelineObjectCreateInfo* info)
{
	EntryHandle ret;

	VKPipelineObject* objLoc = reinterpret_cast<VKPipelineObject*>(AllocTypeFromEntry(sizeof(VKPipelineObject)));

	uint32_t* dynData = reinterpret_cast<uint32_t*>(AllocTypeFromEntry(sizeof(uint32_t) * info->maxDynCap));

	info->data = dynData;

	objLoc = std::construct_at(objLoc, info);

	{
		ret = AddVkTypeToEntry(objLoc);
	}

	return ret;
}

void VKDevice::CreateQueueManager(QueueManager* manager, uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport)
{
	std::shared_lock lock(deviceLock);

	void* queueManagerData = reinterpret_cast<void*>(AllocTypeFromEntry(sizeof(size_t) * maxCount));

	std::construct_at(manager, 
		nullptr, 0, 
		maxCount, queueIndex, 
		queueFlags, presentsupport, 
		*this, queueManagerData);
}

#define MAX_PIPELINE_OBJECTS 50
#define MAX_DYNAMIC_OFFSETS 15

void VKDevice::CreateRenderGraph(EntryHandle renderPass)
{
	std::shared_lock lock(deviceLock);

	auto renderPassData = reinterpret_cast<VKRenderGraph*>(GetVkTypeFromEntry(renderPass + 1));

	auto alloc = AllocTypeFromEntry((sizeof(uint32_t) * MAX_DYNAMIC_OFFSETS) + (sizeof(VKPipelineObject*) * MAX_PIPELINE_OBJECTS));

	auto graph = std::construct_at(renderPassData, renderPass, alloc, MAX_DYNAMIC_OFFSETS, MAX_PIPELINE_OBJECTS);
}

EntryHandle VKDevice::CreateRenderPasses(VKRenderPassBuilder& builder)
{
	std::shared_lock lock(deviceLock);

	VkRenderPass ref = VK_NULL_HANDLE;

	if (vkCreateRenderPass(device, &builder.createInfo, nullptr, &ref) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	EntryHandle ret;

	{
		auto renderPassDataHandle = AllocTypeFromEntry(sizeof(VKRenderGraph) + sizeof(VKPipelineCache));

		ret = AddVkTypeToEntry(ref);

		AddVkTypeToEntry(renderPassDataHandle);
	}

	CreatePipelineCache(ret);

	CreateRenderGraph(ret);

	return ret;
}

VKRenderPassBuilder VKDevice::CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs)
{
	return { this, numAttaches, numDeps, numDescs };
}

EntryHandle VKDevice::CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount)
{
	std::shared_lock lock(deviceLock);
	auto renderTarget = reinterpret_cast<RenderTarget*>(AllocTypeFromEntry(sizeof(RenderTarget)));
	void* data = AllocTypeFromEntry(sizeof(size_t) * framebufferCount * 2);
	std::construct_at(renderTarget, renderPassIndex, framebufferCount, data);
	return AddVkTypeToEntry(renderTarget);
}

EntryHandle* VKDevice::CreateReusableCommandBuffers(
	uint32_t numberOfCommandBuffers, VkCommandBufferLevel level, bool createFences, uint32_t capabilites)
{

	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(capabilites);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

	allocInfo.commandPool = GetCommandPool(poolIndex);
	allocInfo.level = level;
	allocInfo.commandBufferCount = numberOfCommandBuffers;

	VkCommandBuffer* l = reinterpret_cast<VkCommandBuffer*>(AllocFromDeviceCache(sizeof(VkCommandBuffer) * numberOfCommandBuffers));

	if (vkAllocateCommandBuffers(device, &allocInfo, l) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	EntryHandle* ret = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * numberOfCommandBuffers));

	VKCommandBuffer** temp = reinterpret_cast<VKCommandBuffer**>(AllocFromDeviceCache(sizeof(VKCommandBuffer*) * numberOfCommandBuffers));

	for (uint32_t i = 0; i < numberOfCommandBuffers; i++) {
		auto iter = reinterpret_cast<VKCommandBuffer*>(AllocTypeFromEntry(sizeof(VKCommandBuffer)));
		iter->buffer = l[i];
		iter->queueFamilyIndex = queueFamilyIndex;
		iter->queueIndex = queueIndex;
		iter->poolIndex = poolIndex;
		ret[i] = AddVkTypeToEntry(iter);
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

EntryHandle VKDevice::CreateSampledImage(
	std::vector<std::vector<char>>& imageData,
	std::vector<uint32_t>& imageSizes,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, VkFormat type,
	EntryHandle memIndex,
	EntryHandle hostIndex)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(GRAPHICS | TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VkDeviceSize imagesSize = std::accumulate(imageSizes.begin(), imageSizes.end(), 0UL);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	stagingBuffer = reinterpret_cast<VkBuffer>(GetVkTypeFromEntry(hostIndex));
	stagingMemory = reinterpret_cast<VkDeviceMemory>(GetVkTypeFromEntry(hostIndex + 1));

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

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, type, 1,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memIndex);


	VkImage image = GetImage(imageIndex);

	VkCommandPool pool = GetCommandPool(poolIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

	VkDeviceSize offset = 0UL;

	for (auto i = 0U; i < mipLevels; i++) {

		VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, image, width >> i, height >> i, i, offset, { 0, 0, 0 });

		offset += static_cast<VkDeviceSize>(sizes[i]);
	}

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(managerIndex, queueIndex);

	return imageIndex;
}

EntryHandle VKDevice::CreateSampler(uint32_t mipLevels)
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

	VkSampler sampler = VK_NULL_HANDLE;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}

	EntryHandle ret;

	{
		ret = AddVkTypeToEntry(sampler);
	}

	return ret;
}

EntryHandle* VKDevice::CreateSemaphores(uint32_t count)
{
	std::shared_lock lock(deviceLock);

	EntryHandle* ret = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * count));

	VkSemaphore sema = nullptr;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (uint32_t i = 0; i < count; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &sema) != VK_SUCCESS) {
			throw std::runtime_error("failed to create semaphores!");
		}
		ret[i] = AddVkTypeToEntry(sema);
	}

	return ret;
}

EntryHandle VKDevice::CreateSwapChain(uint32_t attachmentCount, uint32_t requestedImageCount, uint32_t maxSemaphorePerStage, uint32_t stages)
{

	std::shared_lock lock(deviceLock);
	EntryHandle index;

	{
		VKSwapChain* swc = reinterpret_cast<VKSwapChain*>(AllocTypeFromEntry(sizeof(VKSwapChain)));

		auto swcsupport = parentInstance->GetSwapChainSupport(gpu);

		swc = std::construct_at(swc, this, parentInstance->renderSurface, attachmentCount, requestedImageCount, swcsupport, stages, maxSemaphorePerStage);

		swc->SetSWCLocalData(AllocTypeFromEntry(swc->CalculateSwapChainMemoryUsage()));

		index = AddVkTypeToEntry(swc);
	}

	return index;
}


//DESTRUCTORS


void VKDevice::DestroyImage(EntryHandle imageIndex)
{
	std::shared_lock lock(deviceLock);

	auto image = reinterpret_cast<VkImage*>(GetVkTypeFromEntry(imageIndex));

	uintptr_t ptr = (uintptr_t)image;
	auto addr = *reinterpret_cast<VkDeviceSize*>(ptr + sizeof(VkImage));
	auto memIndex = *reinterpret_cast<EntryHandle*>(ptr + sizeof(VkImage) + sizeof(VkDeviceSize));

	vkDestroyImage(device, *image, nullptr);

	auto alloc = reinterpret_cast<VKAllocator*>(GetVkTypeFromEntry(memIndex + 1));

	alloc->FreeMemory(addr);
}

void VKDevice::DestroyImageView(EntryHandle imageViewIndex)
{
	std::shared_lock lock(deviceLock);

	VkImageView view = reinterpret_cast<VkImageView>(GetVkTypeFromEntry(imageViewIndex));

	vkDestroyImageView(device, view, nullptr);
}

void VKDevice::DestorySampler(EntryHandle samplerIndex)
{
	std::shared_lock lock(deviceLock);

	VkSampler sampler = reinterpret_cast<VkSampler>(GetVkTypeFromEntry(samplerIndex));

	vkDestroySampler(device, sampler, nullptr);
}


//GETTERS
VKCommandBuffer* VKDevice::GetCommandBuffer(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKCommandBuffer*>(GetVkTypeFromEntry(index));
}

VkCommandBuffer VKDevice::GetCommandBufferHandle(EntryHandle index)
{
	return GetCommandBuffer(index)->buffer;
}

VkCommandPool VKDevice::GetCommandPool(EntryHandle poolIndex)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkCommandPool>(GetVkTypeFromEntry(poolIndex));
}

VkDescriptorPool VKDevice::GetDescriptorPool(EntryHandle poolIndex)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkDescriptorPool>(GetVkTypeFromEntry(poolIndex));
}

uint32_t VKDevice::GetFamiliesOfCapableQueues(uint32_t** queueFamilies, uint32_t *size, uint32_t capabilities)
{
	std::shared_lock lock(deviceLock);
	auto iter = reinterpret_cast<QueueManager*>(GetVkTypeFromEntry(queueManagers));

	uint32_t* out = *queueFamilies;
	uint32_t index = 0;
	for (size_t i = 0; i < queueManagersSize && capabilities; i++)
	{
		uint32_t comp = capabilities;
		capabilities &= ~(iter[i].queueCapabilities & capabilities);
		if (comp != capabilities) out[index++] = iter[i].queueFamilyIndex;
	}

	*size = index;

	return capabilities;
}

VkFence VKDevice::GetFence(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkFence>(GetVkTypeFromEntry(index));
}

VkFramebuffer VKDevice::GetFrameBuffer(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkFramebuffer>(GetVkTypeFromEntry(index));
}

VkBuffer VKDevice::GetHostBuffer(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkBuffer>(GetVkTypeFromEntry(index));
}

VkImage VKDevice::GetImage(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return *reinterpret_cast<VkImage*>(GetVkTypeFromEntry(index));
}

VkImageView VKDevice::GetImageView(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkImageView>(GetVkTypeFromEntry(index));
}

VKPipelineCache* VKDevice::GetPipelineCache(EntryHandle renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKPipelineCache*>((reinterpret_cast<uintptr_t>(GetRenderGraph(renderPassIndex)) + sizeof(VKRenderGraph)));
}

VKPipelineObject* VKDevice::GetPipelineObject(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKPipelineObject*>(GetVkTypeFromEntry(index));
}



int32_t VKDevice::GetPresentQueue(QueueIndex& queueIdx,
	QueueIndex& maxQueueCount,
	VkQueueFamilyProperties* famProps,
	uint32_t famPropsCount,
	VkSurfaceKHR& renderSurface)
{
	for (uint32_t i = 0; i<famPropsCount; i++)
	{
		VkQueueFamilyProperties *props = &famProps[i];
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, renderSurface, &presentSupport);

		if (presentSupport)
		{
			maxQueueCount = QueueIndex(props->queueCount);
			queueIdx = QueueIndex(i);
			return 0;
		}
		i++;
	}
	return -1;
}

int32_t VKDevice::GetQueueByMask(QueueIndex& queueIdx,
	QueueIndex& maxQueueCount,
	VkQueueFamilyProperties* famProps,
	uint32_t famPropsCount,
	uint32_t queueMask)
{
	uint32_t i = 0;
	for (uint32_t i = 0; i < famPropsCount; i++)
	{
		VkQueueFamilyProperties* props = &famProps[i];
		if ((props->queueFlags & queueMask) == queueMask) {
			queueIdx = QueueIndex(i);
			maxQueueCount = QueueIndex(props->queueCount);
			return 0;
		}
		i++;
	}
	return -1;
}

std::tuple<uint32_t, uint32_t, uint32_t, EntryHandle> VKDevice::GetQueueHandle(uint32_t capabilites)
{
	QueueManager* ptr = reinterpret_cast<QueueManager*>(GetVkTypeFromEntry(queueManagers));
	auto ret = FindQueueManagerByCapapbilites(ptr, queueManagersSize, capabilites);
	uint32_t queueIdx = ptr[ret].GetQueue();
	uint32_t managerIndex = queueManagers;
	managerIndex += ret;
	return { managerIndex, queueIdx, ptr[ret].queueFamilyIndex, ptr[ret].poolIndices[queueIdx] };
}

RecordingBufferObject VKDevice::GetRecordingBufferObject(EntryHandle commandBufferIndex)
{
	std::shared_lock lock(deviceLock);
	return { *this, *GetCommandBuffer(commandBufferIndex) };
}

VKRenderGraph* VKDevice::GetRenderGraph(EntryHandle renderPassIndex)
{
	std::shared_lock lock(deviceLock);
	auto renderPassData = reinterpret_cast<VKRenderGraph*>(GetVkTypeFromEntry(renderPassIndex + 1));
	return renderPassData;
}

VkRenderPass VKDevice::GetRenderPass(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkRenderPass>(GetVkTypeFromEntry(index));
}

RenderTarget* VKDevice::GetRenderTarget(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<RenderTarget*>(GetVkTypeFromEntry(index));
}


VkSampler VKDevice::GetSampler(EntryHandle index)
{
	return reinterpret_cast<VkSampler>(GetVkTypeFromEntry(index));
}

VkSemaphore VKDevice::GetSemaphore(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkSemaphore>(GetVkTypeFromEntry(index));
}

VKSwapChain* VKDevice::GetSwapChain(EntryHandle index)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKSwapChain*>(GetVkTypeFromEntry(index));
}

//ACTIONS

uint32_t VKDevice::BeginFrameForSwapchain(EntryHandle swapChainIndex, uint32_t requestedImage)
{
	std::shared_lock lock(deviceLock);
	VKSwapChain* swapChain = GetSwapChain(swapChainIndex);

	uint32_t imageIndex = swapChain->AcquireNextSwapChainImage(UINT64_MAX, requestedImage);

	return imageIndex;
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

OffsetIndex VKDevice::GetOffsetIntoHostBuffer(EntryHandle hostIndex, size_t size, uint32_t alignment)
{
	std::shared_lock lock(deviceLock);

	auto alloc = reinterpret_cast<VKAllocator*>(GetVkTypeFromEntry(hostIndex + 2));

	return OffsetIndex(alloc->GetMemory(size, alignment));
}

uint32_t VKDevice::PresentSwapChain(EntryHandle swapChainIdx, uint32_t frameIdx, EntryHandle commandBufferIndex)
{
	std::shared_lock lock(deviceLock);
	VkQueue queue;
	uint32_t managerIndex, queueIndex, queueFamilyIndex;
	if (commandBufferIndex == ~0ui64)
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

	VKSwapChain* swc = GetSwapChain(swapChainIdx);
	auto depends = swc->GetDependenciesForImageIndex(frameIdx);

	uint32_t waitCount = static_cast<uint32_t>(swc->semaphorePerStage);
	
	VkSemaphore* waitSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * waitCount));

	for (uint32_t i = 0; i < waitCount; i++)
	{
		waitSemaphores[i] = GetSemaphore(depends[waitCount + i]);
	}


	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = waitCount;
	presentInfo.pWaitSemaphores = waitSemaphores;

	VkSwapchainKHR swapChains[] = { swc->swapChain };
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

	if (commandBufferIndex == ~0ui64)
	{
		ReturnQueueToManager(managerIndex, queueIndex);
	}

	return 1;
}

VkQueueFamilyProperties* VKDevice::QueueFamilyDetails(uint32_t *size)
{
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, size, nullptr);

	VkQueueFamilyProperties* ret = reinterpret_cast<VkQueueFamilyProperties*>(AllocFromDeviceCache(sizeof(VkQueueFamilyProperties) * *size));

	vkGetPhysicalDeviceQueueFamilyProperties(gpu, size, ret);

	return ret;
}

EntryHandle VKDevice::RequestWithPossibleBufferResetAndFenceReset(uint64_t timeout, EntryHandle bufferIndex, bool reset, bool fenceReset)
{
	std::shared_lock lock(deviceLock);
	auto vkcb = GetCommandBuffer(bufferIndex);

	if (vkcb->fenceIdx == ~0ui64)
		return bufferIndex;

	VkFence fence = GetFence(vkcb->fenceIdx);

	VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return EntryHandle();

	if (fenceReset)
		vkResetFences(device, 1, &fence);

	if (reset)
		vkResetCommandBuffer(vkcb->buffer, 0);

	return bufferIndex;
}

void VKDevice::ReturnQueueToManager(size_t queueManagerIndex, size_t queueIndex)
{
	auto ptr = reinterpret_cast<QueueManager*>(GetVkTypeFromEntry(queueManagers));

	ptr[queueManagerIndex].ReturnQueue(queueIndex);
}

uint32_t VKDevice::SubmitCommandBuffer(
	EntryHandle* wait,
	VkPipelineStageFlags* waitStages,
	EntryHandle* signal,
	uint32_t waitCount,
	uint32_t signalCount,
	EntryHandle cbIndex)
{
	std::shared_lock lock(deviceLock);
	auto vkcb = GetCommandBuffer(cbIndex);
	VkQueue queue;
	vkGetDeviceQueue(device, vkcb->queueFamilyIndex, vkcb->queueIndex, &queue);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore* waitSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * waitCount));
	VkSemaphore* signalSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * signalCount));

	uint32_t i;
	for (i = 0; i < waitCount; i++)
	{
		waitSemaphores[i] = GetSemaphore(wait[i]);
	}

	for (i = 0; i < signalCount; i++)
	{
		signalSemaphores[i] = GetSemaphore(signal[i]);
	}

	submitInfo.waitSemaphoreCount = waitCount;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalSemaphores;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkcb->buffer;

	VkFence fence = GetFence(vkcb->fenceIdx);

	if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	return 1;
}

uint32_t VKDevice::SubmitCommandsForSwapChain(EntryHandle swapChainIdx, uint32_t frameIndex, EntryHandle cbIndex)
{
	//std::shared_lock lock(deviceLock);
	VKSwapChain* swc = GetSwapChain(swapChainIdx);
	auto depends = swc->GetDependenciesForImageIndex(frameIndex);

	VkPipelineStageFlags* waitStages = reinterpret_cast<VkPipelineStageFlags*>(AllocFromDeviceCache(sizeof(VkPipelineStageFlags) * swc->semaphorePerStage));

	waitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	return SubmitCommandBuffer(depends, waitStages, &depends[1], 1, 1, cbIndex);
}


void VKDevice::TransitionImageLayout(EntryHandle imageIndex,
	VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
	uint32_t mips, uint32_t layers)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkCommandPool pool = GetCommandPool(poolIndex);
	VkImage image = GetImage(imageIndex);
	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);
	VK::Utils::TransitionImageLayout(
		device,
		pool, queue,
		image, format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1, 1
	);

	ReturnQueueToManager(managerIndex, queueIndex);
}


void VKDevice::UpdateRenderGraph(EntryHandle renderPass, uint32_t* dynamicOffsets, uint32_t dos, std::string perGraphDescriptor)
{
	std::shared_lock lock(deviceLock);

	auto graph = reinterpret_cast<VKRenderGraph*>(GetVkTypeFromEntry(renderPass + 1));

	if (!perGraphDescriptor.empty())
	{
		graph->descriptorname = perGraphDescriptor;
	}

	for (uint32_t i = 0; i<dos; i++)
	{
		graph->AddDynamicOffset(dynamicOffsets[i]);
	}
}


int32_t VKDevice::WaitOnCommandBufferAndPossibleResetFence(uint64_t timeout, EntryHandle bufferIndex, bool resetfence)
{
	std::shared_lock lock(deviceLock);
	auto vkcb = GetCommandBuffer(bufferIndex);

	if (vkcb->fenceIdx == ~0ui64)
		return 0;

	VkFence fence = GetFence(vkcb->fenceIdx);

	VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return -1;

	if (resetfence)
		vkResetFences(device, 1, &fence);

	return 0;
}


void VKDevice::WaitOnDevice()
{
	vkDeviceWaitIdle(device);
}

void VKDevice::WriteToHostBuffer(EntryHandle hostIndex, void* data, size_t size, size_t offset)
{
	std::shared_lock lock(deviceLock);
	auto deviceMem = reinterpret_cast<VkDeviceMemory>(GetVkTypeFromEntry(hostIndex + 1));
	void* datalocal;
	vkMapMemory(device, deviceMem, offset, size, 0, reinterpret_cast<void**>(&datalocal));
	std::memcpy(datalocal, data, size);
	vkUnmapMemory(device, deviceMem);
}

/*---------------------------------------------------------*/

QueueManager::QueueManager(
	uint32_t* _cqs, uint32_t _cqss, 
	int32_t _mqc, uint32_t _qfi, 
	uint32_t _queueCapabilities, bool present, 
	VKDevice& _d, void *data) :
	bitmap(0U),
	maxQueueCount(_mqc),
	queueFamilyIndex(_qfi),
	queueCapabilities(ConvertQueueProps(_queueCapabilities, present)),
	device(_d),
	queueSema(Semaphore(_mqc)),
	bitwiseMutex(Semaphore())
{

	assert(maxQueueCount <= 16);

	poolIndices = reinterpret_cast<EntryHandle*>(data);

	QueueIndex index = QueueIndex{ _qfi };

	for (int32_t i = 0; i<_mqc; i++)
	{
		poolIndices[i] = _d.CreateCommandPool(index);
	}

	for (size_t i = 0; i < _cqss; i++)
	{
		bitmap.set(_cqs[i]);
	}
}

uint32_t QueueManager::GetQueue()
{
	queueSema.Wait();
	SemaphoreGuard lock(bitwiseMutex);
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

void QueueManager::ReturnQueue(size_t queueNum)
{
	{
		SemaphoreGuard lock(bitwiseMutex);
		bitmap.reset(queueNum);
	}
	queueSema.Notify();
}

uint32_t QueueManager::ConvertQueueProps(uint32_t flags, bool present)
{
	uint32_t flag = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) * GRAPHICS;
	flag |= ((flags & VK_QUEUE_COMPUTE_BIT) != 0) * COMPUTE;
	flag |= ((flags & VK_QUEUE_TRANSFER_BIT) != 0) * TRANSFER;
	flag |= (present * PRESENT);
	return flag;
}