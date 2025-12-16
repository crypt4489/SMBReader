
#include "pch.h"
#include "VKDevice.h"

#include "GlslangCompiler.h"
#include "VKGraph.h"
#include "VKPipelineObject.h"
#include "VKInstance.h"
#include "VKRenderPassBuilder.h"
#include "VKDescriptorLayoutBuilder.h"
#include "VKDescriptorSetBuilder.h"
#include "VKPipelineBuilder.h"
#include "VKSwapChain.h"
#include "VKTexture.h"
#include "VKUtilities.h"


#include <shared_mutex>


struct BufferAlloc
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VKMemoryAllocator alloc;
};

struct ImageMemoryPool
{
	VkDeviceMemory memory;
	VKMemoryAllocator alloc;
};


struct RenderPassData
{
	EntryHandle target;
	VKRenderGraph graph;
};


DescriptorPoolBuilder::DescriptorPoolBuilder(DeviceOwnedAllocator* alloc, size_t _numPoolSizes, VkDescriptorPoolCreateFlags _flags)
	:
	numPoolSizes(_numPoolSizes),
	counter(0),
	flags(_flags),
	poolSizes(reinterpret_cast<VkDescriptorPoolSize*>(alloc->AllocWrapAround(sizeof(VkDescriptorPoolSize) * _numPoolSizes)))
{

}

void DescriptorPoolBuilder::AddUniformPoolSize(uint32_t size)
{
	poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, size };
}

void DescriptorPoolBuilder::AddStoragePoolSize(uint32_t size)
{
	poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, size };
}

void DescriptorPoolBuilder::AddImageSampler(uint32_t size)
{
	poolSizes[counter++] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, size };
}




VKMemoryAllocator::VKMemoryAllocator(const VkDeviceSize _c) : capacity(_c)
{
	auto firstRange = std::make_pair(0U, capacity);
	freeList.emplace_front(firstRange);
}


void VKMemoryAllocator::InsertSorted(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list,
	VkDeviceSize first,
	VkDeviceSize last)
{
	auto prev = list.before_begin();
	auto traverse = list.begin();
	while (traverse != std::end(list) && first > traverse->first)
	{
		prev = traverse++;
	}
	list.emplace_after(prev, first, last);
}

void VKMemoryAllocator::InsertSortedMerged(std::forward_list<std::pair<VkDeviceSize, VkDeviceSize>>& list,
	VkDeviceSize first,
	VkDeviceSize last)
{
	auto prev = list.before_begin();

	auto traverse = list.begin();

	while (traverse != std::end(list) && (first) > traverse->second)
	{
		prev = traverse++;
	}

	while (traverse != std::end(list) && traverse->first <= last)
	{
		first = std::min(first, traverse->first);
		last = std::max(last, traverse->second);

		traverse = list.erase_after(prev);
	}

	list.emplace_after(prev, first, last);
}

std::pair<VkDeviceSize, VkDeviceSize> VKMemoryAllocator::GetBestFit(VkDeviceSize size, VkDeviceSize alignment)
{
	VkDeviceSize maxDiff = UINT64_MAX;
	auto prev = freeList.before_begin();
	auto iter = freeList.begin();
	auto candidate = freeList.end();
	auto candidatePrev = freeList.end();
	VkDeviceSize addressAlignmentMakeUp = 0U;
	while (iter != std::end(freeList))
	{

		VkDeviceSize endingAddress = iter->second;
		VkDeviceSize startingAddress = iter->first;
		VkDeviceSize makeup = 0;
		if (startingAddress & (startingAddress & (alignment - 1)))
			makeup = alignment - (startingAddress & (alignment - 1));

		startingAddress += (makeup); //make up for any alignment considerations



		VkDeviceSize holesize = endingAddress - startingAddress;

		if (holesize >= size)
		{
			VkDeviceSize diff = holesize - size;

			if (diff < maxDiff)
			{
				maxDiff = diff;
				candidate = iter;
				candidatePrev = prev;
				addressAlignmentMakeUp = makeup;
				if (!maxDiff) break; //perfect match
			}
		}

		prev = iter++;
	}

	if (candidate == std::end(freeList))
	{
		return std::make_pair(UINT64_MAX, UINT64_MAX);
	}

	auto ret = *candidate;
	freeList.erase_after(candidatePrev);
	if (addressAlignmentMakeUp)
	{
		InsertSorted(freeList, ret.first, ret.first + addressAlignmentMakeUp);
		ret.first += addressAlignmentMakeUp;
	}
	return ret;
}

VkDeviceSize VKMemoryAllocator::GetMemory(VkDeviceSize size, VkDeviceSize alignment)
{
	auto iter = GetBestFit(size, alignment);

	if (iter.first == UINT64_MAX && iter.second == UINT64_MAX) // too large, no lower bound
	{
		throw std::runtime_error("too large of allocation!");
	}

	VkDeviceSize originaladdr = iter.first;
	VkDeviceSize endaddr = originaladdr + size;
	VkDeviceSize originalSize = iter.second - originaladdr;

	InsertSorted(occupiedList, originaladdr, endaddr); //add new pair to occupied

	if (originalSize != size) // if it is not a perfect size, have new hole
	{
		InsertSorted(freeList, endaddr, iter.second);
	}

	return originaladdr;
}

void VKMemoryAllocator::FreeMemory(VkDeviceSize addr)
{
	auto prev = occupiedList.before_begin();

	auto iter = occupiedList.begin();

	while (iter != std::end(occupiedList) && iter->first != addr)
	{
		prev = iter++;
	}

	if (iter == std::end(occupiedList))
	{
		throw std::runtime_error("Free memory never allocated");
	}

	VkDeviceSize beg = iter->first, end = iter->second;
	occupiedList.erase_after(prev);
	InsertSortedMerged(freeList, beg, end);
}


RenderTarget::RenderTarget(EntryHandle renderPass, uint32_t imageCount, void* data)
{
	renderPassIndex = renderPass;
	count = imageCount;

	EntryHandle* head = reinterpret_cast<EntryHandle*>(data);

	framebufferIndices = head;
	imageViews = std::next(head, imageCount);
}

DeviceOwnedAllocator::DeviceOwnedAllocator() {
	writeHead = 0;
	size = 0;
	memHead = 0;
}

void* DeviceOwnedAllocator::Alloc(size_t inSize)
{
	size_t val, desired, out;
	val = writeHead.load(std::memory_order_relaxed);
	do {
		desired = val + inSize;
		out = val;
	} while (!writeHead.compare_exchange_weak(val, desired, std::memory_order_relaxed,
		std::memory_order_relaxed));

	uintptr_t dest = memHead + out;

	memset((void*)dest, 0, inSize);

	return reinterpret_cast<void*>(dest);
}

void* DeviceOwnedAllocator::AllocWrapAround(size_t inSize)
{
	size_t val, desired, out;
	val = writeHead.load(std::memory_order_relaxed);
	do {
		desired = val + inSize;
		out = val;
		if (desired >= size)
		{
			out = 0;
			desired = inSize;
		}

	} while (!writeHead.compare_exchange_weak(val, desired, std::memory_order_relaxed,
		std::memory_order_relaxed));

	uintptr_t dest = memHead + out;

	memset((void*)dest, 0, inSize);

	return reinterpret_cast<void*>(dest);
}


RecordingBufferObject::RecordingBufferObject(VKDevice* device, VKCommandBuffer buffer) :
	cbBufferHandler(buffer), vkDeviceHandle(device), currLayout(VK_NULL_HANDLE), currPipeline(VK_NULL_HANDLE)
{

}

void RecordingBufferObject::BindGraphicsPipeline(EntryHandle pipelinename)
{
	BindPipelineInternal(pipelinename, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void RecordingBufferObject::BindComputePipeline(EntryHandle pipelineId) {
	BindPipelineInternal(pipelineId, VK_PIPELINE_BIND_POINT_COMPUTE);
}

void RecordingBufferObject::BindPipelineInternal(EntryHandle id, VkPipelineBindPoint bindPoint) {
	auto pco = vkDeviceHandle->GetPipelineCacheObject(id);
	currLayout = pco->pipelineLayout;
	currPipeline = pco->pipeline;
	vkCmdBindPipeline(cbBufferHandler.buffer, bindPoint, currPipeline);
}

void RecordingBufferObject::BindDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet, 
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	auto descset = vkDeviceHandle->GetDescriptorSet(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, currLayout, 
		firstDescriptorSet, descriptorCount, 
		&descset, dynamicOffsetCount, 
		offsets);
}

void RecordingBufferObject::BindComputeDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	auto descset = vkDeviceHandle->GetDescriptorSet(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer,
		VK_PIPELINE_BIND_POINT_COMPUTE, currLayout,
		firstDescriptorSet, descriptorCount,
		&descset, dynamicOffsetCount,
		offsets);
}

void RecordingBufferObject::BindVertexBuffer(EntryHandle bufferIndex, uint32_t firstBindingCount, uint32_t bindingCount, size_t* offsets)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(bufferIndex);
	vkCmdBindVertexBuffers(cbBufferHandler.buffer, firstBindingCount, bindingCount, &buffer, offsets);
}

void RecordingBufferObject::BindingDrawCmd(uint32_t first, uint32_t drawSize, uint32_t firstInstance, uint32_t instanceCount)
{
	vkCmdDraw(cbBufferHandler.buffer, drawSize, instanceCount, first, firstInstance);
}

void RecordingBufferObject::BindingDrawIndexedCmd(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(cbBufferHandler.buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RecordingBufferObject::BindingIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(indirectBufferIndex);
	vkCmdDrawIndirect(cbBufferHandler.buffer, buffer, indirectBufferOffset, drawCount, sizeof(VkDrawIndirectCommand));
}

void RecordingBufferObject::EndRenderPassCommand()
{
	vkCmdEndRenderPass(cbBufferHandler.buffer);
}

void RecordingBufferObject::PushConstantsCommand(uint32_t offset, uint32_t size, VkShaderStageFlags flag, void* data)
{
	vkCmdPushConstants(cbBufferHandler.buffer, currLayout, flag, offset, size, data);
}

void RecordingBufferObject::DispatchCommand(uint32_t x, uint32_t y, uint32_t z)
{
	vkCmdDispatch(cbBufferHandler.buffer, x, y, z);
}

void RecordingBufferObject::BeginRenderPassCommand(EntryHandle renderTargetIndex, uint32_t imageIndex,
	VkSubpassContents contents,
	VkRect2D rect,
	VkClearColorValue color,
	VkClearDepthStencilValue depthStencil)
{

	VkRenderPassBeginInfo renderPassInfo{};
	auto ref = vkDeviceHandle->GetRenderTarget(renderTargetIndex);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vkDeviceHandle->GetRenderPass(ref->renderPassIndex);
	renderPassInfo.framebuffer = vkDeviceHandle->GetFrameBuffer(ref->framebufferIndices[imageIndex]);
	renderPassInfo.renderArea = rect;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = color;
	clearValues[1].depthStencil = depthStencil;

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cbBufferHandler.buffer, &renderPassInfo, contents);
}

void RecordingBufferObject::BindIndexBuffer(EntryHandle bufferIndex, uint32_t indexOffset, VkIndexType indexType)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(bufferIndex);
	vkCmdBindIndexBuffer(cbBufferHandler.buffer, buffer, indexOffset, indexType);
}

void RecordingBufferObject::BindPipelineBarrierCommand(RBOPipelineBarrierArgs* args)
{
	vkCmdPipelineBarrier(cbBufferHandler.buffer,
		args->srcStageMask, args->dstStageMask,
		args->dependencyFlags,
		args->memoryBarrierCount,
		args->pMemoryBarriers, args->bufferMemoryBarrierCount,
		args->pBufferMemoryBarriers, args->imageMemoryBarrierCount,
		args->pImageMemoryBarriers);


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


void RecordingBufferObject::BeginRecordingCommand(VkCommandBufferInheritanceInfo *info, VkCommandBufferUsageFlags flags)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = flags;
	beginInfo.pInheritanceInfo = info;

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

void RecordingBufferObject::CommandBufferReset()
{
	vkResetCommandBuffer(cbBufferHandler.buffer, 0);
}

void RecordingBufferObject::ExecuteSecondaryCommands(EntryHandle* handles, uint32_t count)
{
	VkCommandBuffer* lbuffers = reinterpret_cast<VkCommandBuffer*>(vkDeviceHandle->AllocFromDeviceCache(sizeof(VkCommandBuffer) * count));
	for (uint32_t i = 0; i < count; i++)
	{
		VKCommandBuffer* lbuffer = vkDeviceHandle->GetCommandBuffer(handles[i]);
		lbuffers[i] = lbuffer->buffer;
	}

	vkCmdExecuteCommands(cbBufferHandler.buffer, count, lbuffers);
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
	deviceLock(),
	entries(nullptr),
	indexForEntries(0),
	numberOfEntries(0),
	deviceDataAlloc(),
	deviceCacheAlloc()
{

}

//allocations

EntryHandle VKDevice::AddVkTypeToEntry(void* handle)
{
	size_t ret = indexForEntries.fetch_add(1);
	entries[ret] = reinterpret_cast<uintptr_t>(handle);
	return EntryHandle(ret);
}


void* VKDevice::AllocFromPerDeviceData(size_t size)
{
	return deviceDataAlloc.Alloc(size);
}

void* VKDevice::GetVkTypeFromEntry(EntryHandle index)
{
	if (index() >= indexForEntries) {
		throw std::runtime_error("Entry Index Overflow");
	}
	void* ret = reinterpret_cast<void*>(entries[index()]);

	return ret;
}


void* VKDevice::AllocFromDeviceCache(size_t size)
{
	return deviceCacheAlloc.AllocWrapAround(size);;
}

//CREATORS

EntryHandle VKDevice::CompileShader(char* data, VkShaderStageFlags flags)
{
	VkShaderModule mod = VK_NULL_HANDLE;

	mod = GLSLANG::CompileShader(device, data, flags);

	EntryHandle ret;

	ShaderHandle* modHandle = reinterpret_cast<ShaderHandle*>(AllocFromPerDeviceData(sizeof(ShaderHandle)));

	modHandle->sMod = mod;
	modHandle->flags = flags;

	ret = AddVkTypeToEntry((void*)modHandle);

	return ret;
}

struct TexelBufferView
{
	EntryHandle bufferHandle;
	EntryHandle viewHandle;
};

EntryHandle VKDevice::CreateBufferView(EntryHandle bufferHandle, VkFormat format, size_t rangeSize, size_t offset)
{
	VkBufferView view;
	VkBuffer buffer = GetBufferHandle(bufferHandle);
	VkBufferViewCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	info.buffer = buffer;
	info.offset = offset;
	info.range = rangeSize;
	info.format = format;

	if (vkCreateBufferView(device, &info, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer view!");
	}

	EntryHandle poolIndex = AddVkTypeToEntry(view);

	return poolIndex;
}

EntryHandle VKDevice::CreateBufferViewFromImagePool(EntryHandle poolIndex, VkFormat format, size_t rangeSize, size_t offset)
{
	ImageMemoryPool* pool = reinterpret_cast<ImageMemoryPool*>(GetVkTypeFromEntry(poolIndex));

	VkBuffer buffer;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = rangeSize;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	EntryHandle buffHandle = AddVkTypeToEntry(buffer);

	EntryHandle viewHandle = CreateBufferView(buffHandle, format, rangeSize, 0);

	TexelBufferView* viewData = reinterpret_cast<TexelBufferView*>(AllocFromPerDeviceData(sizeof(TexelBufferView)));

	viewData->bufferHandle = buffHandle;
	viewData->viewHandle = viewHandle;

	return AddVkTypeToEntry(viewData);
}

EntryHandle VKDevice::CreateCommandPool(QueueIndex queueIndex)
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

EntryHandle VKDevice::CreateComputeGraph(uint32_t dynamicCount, uint32_t maxPipelineCount, uint32_t descriptorCount, uint32_t maxFramesInFlight)
{
	std::shared_lock lock(deviceLock);
	
	auto graph = reinterpret_cast<VKComputeGraph*>(AllocFromPerDeviceData(sizeof(VKComputeGraph)));

	graph = std::construct_at(graph, &deviceDataAlloc, dynamicCount, descriptorCount, maxPipelineCount, maxFramesInFlight, this);

	EntryHandle ret = AddVkTypeToEntry(graph);

	return ret;
}

EntryHandle VKDevice::CreateDesciptorPool(DescriptorPoolBuilder* builder, uint32_t maxSets)
{
	std::shared_lock lock(deviceLock);
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	uint32_t poolSizeCount = static_cast<uint32_t>(builder->numPoolSizes);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = builder->poolSizes;
	poolInfo.maxSets = maxSets;
	poolInfo.flags = builder->flags;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	EntryHandle poolIndex = AddVkTypeToEntry(descriptorPool);

	return poolIndex;
}

DescriptorPoolBuilder VKDevice::CreateDescriptorPoolBuilder(size_t poolSize, VkDescriptorPoolCreateFlags flags)
{
	DescriptorPoolBuilder builder(&deviceCacheAlloc, poolSize, flags);

	return builder;
}

DescriptorSetBuilder* VKDevice::CreateDescriptorSetBuilder(EntryHandle poolIndex, EntryHandle descriptorLayout, uint32_t numberofsets)
{
	std::shared_lock lock(deviceLock);

	DescriptorSetBuilder* data = reinterpret_cast<DescriptorSetBuilder*>(AllocFromDeviceCache(sizeof(DescriptorSetBuilder)));

	DescriptorSetBuilder *dsb = std::construct_at(data, this, numberofsets);
	auto ref = GetDescriptorSetLayout(descriptorLayout);
	dsb->AllocDescriptorSets(GetDescriptorPool(poolIndex), ref, numberofsets);
	return dsb;
}

DescriptorSetBuilder* VKDevice::UpdateDescriptorSet(EntryHandle descriptorHandle)
{
	std::shared_lock lock(deviceLock);

	DescriptorSetBuilder* data = reinterpret_cast<DescriptorSetBuilder*>(AllocFromDeviceCache(sizeof(DescriptorSetBuilder)));

	DescriptorSetBuilder* dsb = std::construct_at(data, this, descriptorHandle);

	return dsb;
}

DescriptorSetLayoutBuilder* VKDevice::CreateDescriptorSetLayoutBuilder(uint32_t bindingCount)
{
	DescriptorSetLayoutBuilder* builder = reinterpret_cast<DescriptorSetLayoutBuilder*>(AllocFromDeviceCache(sizeof(DescriptorSetLayoutBuilder)));

	builder = std::construct_at(builder, this, bindingCount);

	return builder;
}

struct DescriptorSetAlloc
{
	uint32_t numberOfSets;
	VkDescriptorSet* sets;
};

EntryHandle VKDevice::CreateDescriptorSet(VkDescriptorSet* set, uint32_t numberOfSets) {
	
	DescriptorSetAlloc* alloc = reinterpret_cast<DescriptorSetAlloc*>(AllocFromPerDeviceData(sizeof(DescriptorSetAlloc)));
	alloc->numberOfSets = numberOfSets;
	alloc->sets = set;
	EntryHandle ret;

	ret = AddVkTypeToEntry(alloc);
	

	return ret;
}

EntryHandle VKDevice::CreateDescriptorSetLayout(DescriptorSetLayoutBuilder* builder)
{
	VkDescriptorSetLayout descLay = builder->CreateDescriptorSetLayout();

	EntryHandle ret;

	ret = AddVkTypeToEntry(descLay);
	
	return ret;
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

EntryHandle VKDevice::CreateFrameBuffer(EntryHandle* attachmentIndices, uint32_t attachmentsCount, EntryHandle renderPassIndex, VkExtent2D extent)
{
	std::shared_lock lock(deviceLock);

	VkImageView* attachments = reinterpret_cast<VkImageView*>(AllocFromDeviceCache(sizeof(VkImageView) * attachmentsCount));

	for (uint32_t i = 0; i < attachmentsCount; i++) {
		attachments[i] = GetImageViewByHandle(attachmentIndices[i]);
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

EntryHandle VKDevice::CreateHostBuffer(VkDeviceSize allocSize, bool coherent, VkBufferUsageFlags usage)
{
	std::shared_lock lock(deviceLock);

	VkBuffer buffer;
	VkDeviceMemory memory;

	std::tie(buffer, memory) = VK::Utils::createBuffer(device, gpu, allocSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | (coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), VK_SHARING_MODE_EXCLUSIVE, usage);


	BufferAlloc* alloc = nullptr;
		
	alloc = reinterpret_cast<BufferAlloc*>(AllocFromPerDeviceData(sizeof(BufferAlloc)));

	alloc->buffer = buffer;
	alloc->memory = memory;

	std::construct_at(&alloc->alloc, allocSize);

	EntryHandle ret = AddVkTypeToEntry(alloc);

	return ret;
}


EntryHandle VKDevice::CreateDeviceBuffer(VkDeviceSize allocSize, VkBufferUsageFlags usage)
{
	std::shared_lock lock(deviceLock);

	VkBuffer buffer;
	VkDeviceMemory memory;

	std::tie(buffer, memory) = VK::Utils::createBuffer(device, gpu, allocSize,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, usage);


	BufferAlloc* alloc = nullptr;

	alloc = reinterpret_cast<BufferAlloc*>(AllocFromPerDeviceData(sizeof(BufferAlloc)));

	alloc->buffer = buffer;
	alloc->memory = memory;

	std::construct_at(&alloc->alloc, allocSize);


	EntryHandle ret = AddVkTypeToEntry(alloc);

	return ret;
}

struct ImageAllocation
{
	VkImage imageHandle;
	size_t deviceMemoryAddress;
	EntryHandle memIndex;
};

EntryHandle VKDevice::CreateImage(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps, VkImageLayout layout, VkImageTiling tiling, VkImageCreateFlags cflags, EntryHandle memIndex)
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
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = layout;
	imageInfo.usage = flags;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
	imageInfo.flags = cflags;

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	auto iter = reinterpret_cast<ImageMemoryPool*>(GetVkTypeFromEntry(memIndex));

	VkDeviceSize addr = iter->alloc.GetMemory(memRequirements.size, memRequirements.alignment);

	vkBindImageMemory(device, image, iter->memory, addr);

	ImageAllocation* alloc = reinterpret_cast<ImageAllocation*>(AllocFromPerDeviceData(sizeof(ImageAllocation)));

	alloc->imageHandle = image;
	alloc->deviceMemoryAddress = addr;
	alloc->memIndex = memIndex;

	EntryHandle ret;

	ret = AddVkTypeToEntry(alloc);
	
	return ret;
}


EntryHandle VKDevice::CreateStorageImage(
	uint32_t width, uint32_t height,
	uint32_t mipLevels, VkFormat type,
	EntryHandle memIndex,
	EntryHandle hostIndex, VkImageAspectFlags flags, VkImageLayout layout, bool createSampler)
{
	std::shared_lock lock(deviceLock);
	

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, type, 1,
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, memIndex
	);

	EntryHandle viewIndex = CreateImageView(imageIndex, mipLevels, type, flags);

	

	VKTexture* tex = reinterpret_cast<VKTexture*>(AllocFromPerDeviceData(sizeof(VKTexture)));

	tex = std::construct_at(tex, imageIndex, &viewIndex, 1, nullptr, 0);

	if (createSampler)
	{
		EntryHandle samplerIndex = CreateSampler(mipLevels);
		tex->samplerIndex[0] = samplerIndex;
	}

	EntryHandle ret = AddVkTypeToEntry(tex);

	auto queueDetails = GetQueueHandle(GRAPHICS | TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VkImage image = GetImageByHandle(imageIndex);

	VkCommandPool pool = GetCommandPool(poolIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, type, VK_IMAGE_LAYOUT_UNDEFINED, layout, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(managerIndex, queueIndex);

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

	ImageMemoryPool* alloc = reinterpret_cast<ImageMemoryPool*>(AllocFromPerDeviceData(sizeof(ImageMemoryPool)));

	std::construct_at(&alloc->alloc, poolSize);

	alloc->memory = deviceMemory;

	EntryHandle ret = AddVkTypeToEntry(alloc);
	
	return ret;
}

EntryHandle VKDevice::CreateImageView(
	EntryHandle imageIndex, uint32_t mipLevels,
	VkFormat type, VkImageAspectFlags aspectMask)
{
	std::shared_lock lock(deviceLock);
	VkImageViewCreateInfo viewInfo{};

	VkImage image = GetImageByHandle(imageIndex);

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



	ret = AddVkTypeToEntry(imageView);
	

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

	
	ret = AddVkTypeToEntry(imageView);
	

	return ret;
}

void VKDevice::CreateLogicalDevice(
	const char** instanceLayers,
	uint32_t layerCount,
	const char** deviceExtensions,
	uint32_t deviceExtCount,
	uint32_t queueFlags,
	VkPhysicalDeviceFeatures2* features,
	VkSurfaceKHR renderSurface,
	size_t perDeviceDataSize,
	size_t perEntriesSize,
	size_t perCacheSize,
	size_t driverPerSize,
	size_t driverPerCache
)
{
	deviceDriverAllocator = new VKAllocationCB();
	deviceDriverAllocator->commandDataSize = driverPerCache;
	deviceDriverAllocator->commandData = new unsigned char[driverPerCache];
	deviceDriverAllocator->instanceDataSize = driverPerSize;
	deviceDriverAllocator->instanceData = new unsigned char[driverPerSize];


	deviceCacheAlloc.size = perCacheSize;
	deviceCacheAlloc.memHead = (uintptr_t)new char[perCacheSize];
	

	deviceDataAlloc.size = perDeviceDataSize - perEntriesSize;
	deviceDataAlloc.memHead = (uintptr_t)new char[deviceDataAlloc.size];
	

	numberOfEntries = perEntriesSize / sizeof(uintptr_t);
	entries = new uintptr_t[numberOfEntries];
	

	std::unordered_map<uint32_t, std::tuple<uint32_t, bool>> queueIndices;
	uint32_t familyCount;
	VkQueueFamilyProperties* famProps;
	QueueIndex index, maxCount;

	famProps = QueueFamilyDetails(&familyCount);

	GetQueueByMask(&index, &maxCount, famProps, familyCount, queueFlags);

	queueIndices[index] = { maxCount, false };

	if (renderSurface)
	{
		GetPresentQueue(&index, &maxCount, famProps, familyCount, renderSurface);
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
	logDeviceInfo.enabledExtensionCount = deviceExtCount;
	logDeviceInfo.enabledLayerCount = layerCount;
	logDeviceInfo.ppEnabledExtensionNames = deviceExtensions;
	logDeviceInfo.ppEnabledLayerNames = instanceLayers;
	logDeviceInfo.pNext = features;

	auto callbacks = (*deviceDriverAllocator)();

	VkResult res = vkCreateDevice(gpu, &logDeviceInfo, &callbacks, &device);

	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create logical GPU, mate!");
	}

	QueueManager* ptr = (QueueManager*)AllocFromPerDeviceData(sizeof(QueueManager) * queueIndices.size());

	queueManagersSize = queueIndices.size();


	queueManagers = AddVkTypeToEntry(ptr);
	

	for (const auto queueFamily : queueIndices) {

		uint32_t queueIndex = queueFamily.first;
		uint32_t maxCount = std::get<uint32_t>(queueFamily.second);
		bool present = std::get<bool>(queueFamily.second);
		CreateQueueManager(ptr, queueIndex, maxCount, queueFlags, present);
		ptr = std::next(ptr);
	}



	VkDeviceGroupPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR;
	info.swapchainCount = 1;
	info.mode = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
	info.pDeviceMasks = &deviceMask;
}

EntryHandle VKDevice::CreateMemoryBarrier(VkAccessFlags src, VkAccessFlags dst)
{
	std::shared_lock lock(deviceLock);
	VkMemoryBarrier* barrier = reinterpret_cast<VkMemoryBarrier*>(AllocFromPerDeviceData(sizeof(VkMemoryBarrier)));

	barrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier->pNext = nullptr;
	barrier->srcAccessMask = src;
	barrier->dstAccessMask = dst;
	

	EntryHandle ret = AddVkTypeToEntry(barrier);

	return ret;
}

EntryHandle VKDevice::CreateBufferMemoryBarrier(VkAccessFlags src, VkAccessFlags dst, uint32_t srcQFI, uint32_t dstQFI, EntryHandle bufferIndex, size_t offset, size_t size)
{
	std::shared_lock lock(deviceLock);
	VkBufferMemoryBarrier* barrier = reinterpret_cast<VkBufferMemoryBarrier*>(AllocFromPerDeviceData(sizeof(VkBufferMemoryBarrier)));
	VkBuffer buffer = GetBufferHandle(bufferIndex);

	barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier->pNext = nullptr;
	barrier->srcAccessMask = src;
	barrier->dstAccessMask = dst;
	barrier->srcQueueFamilyIndex = srcQFI;
	barrier->dstQueueFamilyIndex = dstQFI;
	barrier->buffer = buffer;
	barrier->offset = offset;
	barrier->size = size;

	EntryHandle ret = AddVkTypeToEntry(barrier);

	return ret;
}

EntryHandle VKDevice::CreateImageMemoryBarrier(VkAccessFlags src, VkAccessFlags dst, uint32_t srcQFI, uint32_t dstQFI, VkImageLayout oldLayout, VkImageLayout newLayout, EntryHandle imageIndex, VkImageSubresourceRange subresourceRange)
{
	std::shared_lock lock(deviceLock);
	VkImageMemoryBarrier* barrier = reinterpret_cast<VkImageMemoryBarrier*>(AllocFromPerDeviceData(sizeof(VkImageMemoryBarrier)));
	VkImage image = GetImageByTexture(imageIndex);

	barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier->pNext = nullptr;
	barrier->srcAccessMask = src;
	barrier->dstAccessMask = dst;
	barrier->srcQueueFamilyIndex = srcQFI;
	barrier->dstQueueFamilyIndex = dstQFI;
	barrier->oldLayout = oldLayout;
	barrier->newLayout = newLayout;
	barrier->image = image;
	barrier->subresourceRange.aspectMask = subresourceRange.aspectMask;
	barrier->subresourceRange.baseArrayLayer = subresourceRange.baseArrayLayer;
	barrier->subresourceRange.baseMipLevel = subresourceRange.baseMipLevel;
	barrier->subresourceRange.layerCount = subresourceRange.layerCount;
	barrier->subresourceRange.levelCount = subresourceRange.levelCount;
	

	EntryHandle ret = AddVkTypeToEntry(barrier);

	return ret;
}



VKGraphicsPipelineBuilder* VKDevice::CreateGraphicsPipelineBuilder(EntryHandle renderPassIndex, uint32_t colorCount, uint32_t descLayoutCount, uint32_t dynamicStateCount, uint32_t pushConstantRangeCount)
{
	std::shared_lock lock(deviceLock);

	auto renderPass = GetRenderPass(renderPassIndex);

	auto renderPassData = reinterpret_cast<VKGraphicsPipelineBuilder*>(AllocFromDeviceCache(sizeof(VKGraphicsPipelineBuilder)));

	return std::construct_at(renderPassData, renderPass, this, colorCount, descLayoutCount, dynamicStateCount, pushConstantRangeCount);
}

VKComputePipelineBuilder* VKDevice::CreateComputePipelineBuilder(size_t numberOfDescriptors, uint32_t pushConstantRangeCount)
{
	std::shared_lock lock(deviceLock);

	auto computePB = reinterpret_cast<VKComputePipelineBuilder*>(AllocFromDeviceCache(sizeof(VKComputePipelineBuilder)));

	return std::construct_at(computePB, this, numberOfDescriptors, pushConstantRangeCount);
}

EntryHandle VKDevice::CreateGraphicsPipelineObject(VKGraphicsPipelineObjectCreateInfo* info)
{
	EntryHandle ret;

	VKGraphicsPipelineObject* objLoc = reinterpret_cast<VKGraphicsPipelineObject*>(AllocFromPerDeviceData(sizeof(VKGraphicsPipelineObject)));

	objLoc = std::construct_at(objLoc, info, &deviceDataAlloc);

	ret = AddVkTypeToEntry(objLoc);

	return ret;
}


EntryHandle VKDevice::CreateComputePipelineObject(VKComputePipelineObjectCreateInfo* info)
{
	EntryHandle ret;

	VKComputePipelineObject* objLoc = reinterpret_cast<VKComputePipelineObject*>(AllocFromPerDeviceData(sizeof(VKComputePipelineObject)));

	objLoc = std::construct_at(objLoc, info, &deviceDataAlloc);

	ret = AddVkTypeToEntry(objLoc);

	return ret;
}

void VKDevice::CreateQueueManager(QueueManager* manager, uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport)
{
	std::shared_lock lock(deviceLock);

	void* queueManagerData = reinterpret_cast<void*>(AllocFromPerDeviceData(sizeof(size_t) * maxCount));

	std::construct_at(manager, 
		nullptr, 0, 
		maxCount, queueIndex, 
		queueFlags, presentsupport, 
		*this, queueManagerData);
}

EntryHandle VKDevice::CreatePipelineCacheObject(PipelineCacheObject* obj)
{
	std::shared_lock lock(deviceLock);

	PipelineCacheObject* perObj = reinterpret_cast<PipelineCacheObject*>(AllocFromPerDeviceData(sizeof(PipelineCacheObject)));

	perObj->descLayout = obj->descLayout;
	perObj->pipeline = obj->pipeline;
	perObj->pipelineLayout = obj->pipelineLayout;

	EntryHandle ret;

	ret = AddVkTypeToEntry(perObj);
	
	return ret;
}

#define MAX_PIPELINE_OBJECTS 50
#define MAX_DYNAMIC_OFFSETS 15

EntryHandle VKDevice::CreateRenderTargetData(EntryHandle renderTarget, uint32_t descriptorCount, uint32_t maxFramesInFlight)
{
	std::shared_lock lock(deviceLock);

	auto renderPassDataHandle = reinterpret_cast<RenderPassData*>(AllocFromPerDeviceData(sizeof(RenderPassData)));

	renderPassDataHandle->target = renderTarget;

	auto graph = std::construct_at(&renderPassDataHandle->graph, &deviceDataAlloc, MAX_DYNAMIC_OFFSETS, descriptorCount, MAX_PIPELINE_OBJECTS, maxFramesInFlight, this);

	return AddVkTypeToEntry(renderPassDataHandle);
}

EntryHandle VKDevice::CreateRenderPasses(VKRenderPassBuilder& builder)
{
	std::shared_lock lock(deviceLock);

	VkRenderPass ref = VK_NULL_HANDLE;

	if (vkCreateRenderPass(device, &builder.createInfo, nullptr, &ref) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}

	return AddVkTypeToEntry(ref);
}

VKRenderPassBuilder VKDevice::CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs)
{
	return { this, numAttaches, numDeps, numDescs };
}

EntryHandle VKDevice::CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount)
{
	std::shared_lock lock(deviceLock);
	auto renderTarget = reinterpret_cast<RenderTarget*>(AllocFromPerDeviceData(sizeof(RenderTarget)));
	
	EntryHandle ret;
	
	ret = AddVkTypeToEntry(renderTarget);

	void* data = AllocFromPerDeviceData(sizeof(EntryHandle) * framebufferCount * 2);
	
	std::construct_at(renderTarget, renderPassIndex, framebufferCount, data);

	return ret;
}

EntryHandle* VKDevice::CreateReusableCommandBuffers(
	uint32_t numberOfCommandBuffers, bool createFences, uint32_t capabilites, VkCommandBufferLevel level)
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
		auto iter = reinterpret_cast<VKCommandBuffer*>(AllocFromPerDeviceData(sizeof(VKCommandBuffer)));
		iter->buffer = l[i];
		iter->queueFamilyIndex = queueFamilyIndex;
		iter->queueIndex = queueIndex;
		iter->poolIndex = poolIndex;
		iter->fenceIdx = EntryHandle();
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
	char* imageData,
	uint32_t* imageSizes,
	uint32_t blobSize,
	uint32_t width, uint32_t height,
	uint32_t mipLevels, VkFormat type,
	EntryHandle memIndex,
	EntryHandle hostIndex, VkImageAspectFlags flags)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(GRAPHICS | TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(blobSize);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;


	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(hostIndex));

	stagingBuffer = alloc->buffer;
	stagingMemory = alloc->memory;

	char* data;
	auto& pixels = imageData;
	vkMapMemory(device, stagingMemory, 0, imagesSize, 0, reinterpret_cast<void**>(&data));
	
	std::memcpy(data, pixels, imagesSize);
		
	vkUnmapMemory(device, stagingMemory);

	VkFormat format = type;

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, type, 1,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, memIndex);


	VkImage image = GetImageByHandle(imageIndex);

	VkCommandPool pool = GetCommandPool(poolIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

	VkDeviceSize offset = 0UL;

	if (mipLevels > 1)

	{
		for (auto i = 0U; i < mipLevels; i++) {

			VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, image, width >> i, height >> i, i, offset, { 0, 0, 0 });

			offset += static_cast<VkDeviceSize>(imageSizes[i]);
		}
	}
	else
	{
		VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, image, width, height, 0, 0, { 0, 0, 0 });
	}

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(managerIndex, queueIndex);

	EntryHandle viewIndex = CreateImageView(imageIndex, mipLevels, type, flags);

	EntryHandle samplerIndex = CreateSampler(mipLevels);

	VKTexture* tex = reinterpret_cast<VKTexture*>(AllocFromPerDeviceData(sizeof(VKTexture)));

	tex = std::construct_at(tex, imageIndex, &viewIndex, 1, &samplerIndex, 1);

	EntryHandle ret;

	

	ret = AddVkTypeToEntry(tex);
	

	return ret;
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

	
	ret = AddVkTypeToEntry(sampler);

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

EntryHandle VKDevice::CreateShader(char* data, size_t dataSize, VkShaderStageFlags flags)
{
	VkShaderModule mod = VK_NULL_HANDLE;

	mod = VK::Utils::createShaderModule(device, data, dataSize);

	ShaderHandle* modHandle = reinterpret_cast<ShaderHandle*>(AllocFromPerDeviceData(sizeof(ShaderHandle)));

	modHandle->sMod = mod;

	modHandle->flags = flags;

	EntryHandle ret = AddVkTypeToEntry((void*)modHandle);

	return ret;
}

EntryHandle VKDevice::CreateSwapChain(uint32_t attachmentCount, uint32_t requestedImageCount, uint32_t maxFramesInFlight, uint32_t renderTargetCount)
{

	std::shared_lock lock(deviceLock);

	VKSwapChain* swc = reinterpret_cast<VKSwapChain*>(AllocFromPerDeviceData(sizeof(VKSwapChain)));

	auto swcsupport = parentInstance->GetSwapChainSupport(gpu);

	swc = std::construct_at(swc, this, parentInstance->renderSurface, &deviceDataAlloc, attachmentCount, requestedImageCount, maxFramesInFlight, swcsupport, renderTargetCount);

	EntryHandle index = AddVkTypeToEntry(swc);
	return index;
}


//DESTRUCTORS

void VKDevice::DestroyBuffer(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(handle));
	if (!alloc) return;
	VkBuffer ret = alloc->buffer;
	auto deviceMem = alloc->memory;
	vkDestroyBuffer(device, ret, nullptr);
	vkFreeMemory(device, deviceMem, nullptr);
	entries[handle()] = 0;
}
void VKDevice::DestroyCommandBuffer(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VKCommandBuffer* buff = GetCommandBuffer(handle);
	if (!buff) return;
	if (buff->fenceIdx != EntryHandle()) {
		VkFence fence = GetFence(buff->fenceIdx);
		vkDestroyFence(device, fence, nullptr);
		entries[buff->fenceIdx()] = 0;
	}
	entries[handle()] = 0;
}

void VKDevice::DestroyFence(EntryHandle handle)
{
	VkFence fence = GetFence(handle);
	vkDestroyFence(device, fence, nullptr);
	entries[handle()] = 0;
}

void VKDevice::DestroyDescriptorPool(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VkDescriptorPool pool = GetDescriptorPool(handle);
	if (!pool) return;
	vkDestroyDescriptorPool(device, pool, nullptr);
	entries[handle()] = 0;
}

void VKDevice::DestroyDescriptorLayout(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VkDescriptorSetLayout lay = GetDescriptorSetLayout(handle);
	if (!lay) return;
	vkDestroyDescriptorSetLayout(device, lay, nullptr);
	entries[handle()] = 0;
}

void VKDevice::DestroyDevice()
{
	vkDeviceWaitIdle(device);

	QueueManager* ptr = reinterpret_cast<QueueManager*>(GetVkTypeFromEntry(queueManagers));

	for (size_t j = 0; j < queueManagersSize; j++)
	{
		EntryHandle* handles = ptr->poolIndices;
		for (size_t i = 0; i < ptr->maxQueueCount; i++)
		{
			VkCommandPool pool = GetCommandPool(handles[i]);
			vkDestroyCommandPool(device, pool, nullptr);
			entries[handles[i]()] = 0;
		}
	}

	vkDestroyDevice(device, nullptr);

	deviceDriverAllocator->Delete();

	delete deviceDriverAllocator;

	if (deviceDataAlloc.memHead)
	{
		delete[] (void*)deviceDataAlloc.memHead;
	}

	if (entries)
	{
		delete[] entries;
	}

	if (deviceCacheAlloc.memHead)
	{
		delete[](void*)deviceCacheAlloc.memHead;
	}
}

void VKDevice::DestroyImage(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	auto image = reinterpret_cast<ImageAllocation*>(GetVkTypeFromEntry(handle));

	if (!image) return;

	vkDestroyImage(device, image->imageHandle, nullptr);

	auto alloc = reinterpret_cast<ImageMemoryPool*>(GetVkTypeFromEntry(image->memIndex));

	alloc->alloc.FreeMemory(image->deviceMemoryAddress);
	entries[handle()] = 0;
}

void VKDevice::DestroyImagePool(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	auto iter = reinterpret_cast<ImageMemoryPool*>(GetVkTypeFromEntry(handle));
	if (!iter) return;
	vkFreeMemory(device, iter->memory, nullptr);
	entries[handle()] = 0;
}

void VKDevice::DestroyRenderPass(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	VkRenderPass pass = GetRenderPass(handle);

	if (!pass) return;

	vkDestroyRenderPass(device, pass, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestroyRenderTarget(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	RenderTarget* target = GetRenderTarget(handle);
	uint32_t i = target->count;
	for (uint32_t j = 0; j<i; j++)
	{
		DestroyImageView(target->imageViews[j]);
		VkFramebuffer fb = GetFrameBuffer(target->framebufferIndices[j]);
		vkDestroyFramebuffer(device, fb, nullptr);
	}

	entries[handle()] = 0;
}

void VKDevice::ResetRenderTarget(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	RenderTarget* target = GetRenderTarget(handle);
	uint32_t i = target->count;
	for (uint32_t j = 0; j < i; j++)
	{
		DestroyImageView(target->imageViews[j]);
		VkFramebuffer fb = GetFrameBuffer(target->framebufferIndices[j]);
		vkDestroyFramebuffer(device, fb, nullptr);
	}
}

void VKDevice::DestroyImageView(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	VkImageView view = reinterpret_cast<VkImageView>(GetVkTypeFromEntry(handle));

	if (!view) return;

	vkDestroyImageView(device, view, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestroyPipelineCacheObject(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	PipelineCacheObject* obj = GetPipelineCacheObject(handle);

	if (!obj) return;

	vkDestroyPipeline(device, obj->pipeline, nullptr);

	vkDestroyPipelineLayout(device, obj->pipelineLayout, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestorySampler(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	VkSampler sampler = reinterpret_cast<VkSampler>(GetVkTypeFromEntry(handle));

	if (!sampler) return;

	vkDestroySampler(device, sampler, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestroySemaphore(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);

	VkSemaphore sema = GetSemaphore(handle);

	if (!sema) return;

	vkDestroySemaphore(device, sema, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestroyShader(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
		
	ShaderHandle* shader = GetShader(handle);

	if (!shader) return;

	vkDestroyShaderModule(device, shader->sMod, nullptr);

	entries[handle()] = 0;
}

void VKDevice::DestroyTexture(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VKTexture* tex = GetTexture(handle);

	for (int i = 0; tex->samplerIndex[i] != EntryHandle(); i++)
		DestorySampler(tex->samplerIndex[i]);

	for (int i = 0; tex->viewIndex[i] != EntryHandle(); i++)
		DestroyImageView(tex->viewIndex[i]);
	DestroyImage(tex->imageIndex);
	entries[handle()] = 0;
}


//GETTERS
VKCommandBuffer* VKDevice::GetCommandBuffer(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKCommandBuffer*>(GetVkTypeFromEntry(handle));
}

VkCommandBuffer VKDevice::GetCommandBufferHandle(EntryHandle handle)
{
	auto ref = GetCommandBuffer(handle);
	if (!ref) return VK_NULL_HANDLE;
	return ref->buffer;
}

VkCommandPool VKDevice::GetCommandPool(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkCommandPool>(GetVkTypeFromEntry(handle));
}

VKComputeGraph* VKDevice::GetComputeGraph(EntryHandle handle) 
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKComputeGraph*>(GetVkTypeFromEntry(handle));
}

VkDescriptorPool VKDevice::GetDescriptorPool(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkDescriptorPool>(GetVkTypeFromEntry(handle));
}

VkDescriptorSet VKDevice::GetDescriptorSet(EntryHandle handle, uint32_t index)
{
	std::shared_lock lock(deviceLock);
	DescriptorSetAlloc* set = reinterpret_cast<DescriptorSetAlloc*>(GetVkTypeFromEntry(handle));
	if (!set) return VK_NULL_HANDLE;
	if (set->numberOfSets <= index) throw std::runtime_error("Come on!");
	return set->sets[index];

}

VkDescriptorSet* VKDevice::GetDescriptorSets(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	DescriptorSetAlloc* set = reinterpret_cast<DescriptorSetAlloc*>(GetVkTypeFromEntry(handle));
	return set->sets;
}

VkDescriptorSetLayout VKDevice::GetDescriptorSetLayout(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkDescriptorSetLayout>(GetVkTypeFromEntry(handle));
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

VkFence VKDevice::GetFence(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkFence>(GetVkTypeFromEntry(handle));
}

VkFramebuffer VKDevice::GetFrameBuffer(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkFramebuffer>(GetVkTypeFromEntry(handle));
}

VkBuffer VKDevice::GetBufferHandle(EntryHandle index)
{
	std::shared_lock lock(deviceLock);

	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(index));

	if (!alloc) return VK_NULL_HANDLE;

	return alloc->buffer;
}

VkImage VKDevice::GetImageByHandle(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return *reinterpret_cast<VkImage*>(GetVkTypeFromEntry(handle));
}

VkImage VKDevice::GetImageByTexture(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VKTexture* tex = GetTexture(handle);
	if (!tex) return VK_NULL_HANDLE;
	return GetImageByHandle(tex->imageIndex);
}

VkImageView VKDevice::GetImageViewByHandle(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkImageView>(GetVkTypeFromEntry(handle));
}

VkImageView VKDevice::GetImageViewByTexture(EntryHandle handle, int imageViewIndex)
{
	std::shared_lock lock(deviceLock);
	VKTexture* tex = GetTexture(handle);
	if (!tex) return VK_NULL_HANDLE;
	return GetImageViewByHandle(tex->viewIndex[imageViewIndex]);
}


VkMemoryBarrier* VKDevice::GetMemoryBarrier(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VkMemoryBarrier* barrier = reinterpret_cast<VkMemoryBarrier*>(GetVkTypeFromEntry(handle));
	return barrier;
}

VkBufferMemoryBarrier* VKDevice::GetBufferMemoryBarrier(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VkBufferMemoryBarrier* barrier = reinterpret_cast<VkBufferMemoryBarrier*>(GetVkTypeFromEntry(handle));
	return barrier;
}

VkImageMemoryBarrier* VKDevice::GetImageMemoryBarrier(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	VkImageMemoryBarrier* barrier = reinterpret_cast<VkImageMemoryBarrier*>(GetVkTypeFromEntry(handle));
	return barrier;
}

PipelineCacheObject* VKDevice::GetPipelineCacheObject(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<PipelineCacheObject*>(GetVkTypeFromEntry(handle));
}

VKPipelineObject* VKDevice::GetPipelineObject(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKPipelineObject*>(GetVkTypeFromEntry(handle));
}

VKComputePipelineObject* VKDevice::GetComputePipelineObject(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKComputePipelineObject*>(GetVkTypeFromEntry(handle));
}

int32_t VKDevice::GetPresentQueue(QueueIndex* queueIdx,
	QueueIndex* maxQueueCount,
	VkQueueFamilyProperties* famProps,
	uint32_t famPropsCount,
	VkSurfaceKHR renderSurface)
{
	for (uint32_t i = 0; i<famPropsCount; i++)
	{
		VkQueueFamilyProperties *props = &famProps[i];
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, renderSurface, &presentSupport);

		if (presentSupport)
		{
			*maxQueueCount = QueueIndex(props->queueCount);
			*queueIdx = QueueIndex(i);
			return 0;
		}
		i++;
	}
	return -1;
}

int32_t VKDevice::GetQueueByMask(QueueIndex* queueIdx,
	QueueIndex* maxQueueCount,
	VkQueueFamilyProperties* famProps,
	uint32_t famPropsCount,
	uint32_t queueMask)
{
	uint32_t i = 0;
	for (uint32_t i = 0; i < famPropsCount; i++)
	{
		VkQueueFamilyProperties* props = &famProps[i];
		if ((props->queueFlags & queueMask) == queueMask) {
			*queueIdx = QueueIndex(i);
			*maxQueueCount = QueueIndex(props->queueCount);
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
	EntryHandle managerIndex = queueManagers;
	managerIndex = managerIndex + ret;
	return { managerIndex, queueIdx, ptr[ret].queueFamilyIndex, ptr[ret].poolIndices[queueIdx] };
}

RecordingBufferObject VKDevice::GetRecordingBufferObject(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return { this, *GetCommandBuffer(handle) };
}

VKRenderGraph* VKDevice::GetRenderGraph(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	auto renderPassData = reinterpret_cast<RenderPassData*>(GetVkTypeFromEntry(handle));
	if (!renderPassData) return nullptr;
	return &renderPassData->graph;
}

VkRenderPass VKDevice::GetRenderPass(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	auto renderPassData = reinterpret_cast<VkRenderPass>(GetVkTypeFromEntry(handle));
	return renderPassData;
}

RenderTarget* VKDevice::GetRenderTarget(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	auto data = reinterpret_cast<RenderTarget*>(GetVkTypeFromEntry(handle));
	return data;
}


RenderTarget* VKDevice::GetRenderTargetByGraph(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	auto data = reinterpret_cast<RenderPassData*>(GetVkTypeFromEntry(handle));
	return GetRenderTarget(data->target);
}


VkSampler VKDevice::GetSamplerByHandle(EntryHandle handle)
{
	return reinterpret_cast<VkSampler>(GetVkTypeFromEntry(handle));
}

VkSampler VKDevice::GetSamplerByTexture(EntryHandle handle, int samplerIndex)
{
	std::shared_lock lock(deviceLock);
	VKTexture* tex = GetTexture(handle);
	if (!tex) return VK_NULL_HANDLE;
	return GetSamplerByHandle(tex->samplerIndex[samplerIndex]);
}

VkSemaphore VKDevice::GetSemaphore(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VkSemaphore>(GetVkTypeFromEntry(handle));
}

ShaderHandle* VKDevice::GetShader(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	
	ShaderHandle* data = reinterpret_cast<ShaderHandle*>(GetVkTypeFromEntry(handle));
	
	return data;
}

VKSwapChain* VKDevice::GetSwapChain(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKSwapChain*>(GetVkTypeFromEntry(handle));
}

VKTexture* VKDevice::GetTexture(EntryHandle handle)
{
	std::shared_lock lock(deviceLock);
	return reinterpret_cast<VKTexture*>(GetVkTypeFromEntry(handle));
}

//ACTIONS

uint32_t VKDevice::BeginFrameForSwapchain(EntryHandle swapChainIndex, uint32_t currentFrame)
{
	std::shared_lock lock(deviceLock);
	VKSwapChain* swapChain = GetSwapChain(swapChainIndex);

	uint32_t imageIndex = swapChain->AcquireNextSwapChainImage2(UINT64_MAX, currentFrame);

	return imageIndex;
}

VkShaderStageFlagBits VKDevice::ConvertShaderFlags(const std::string& filename)
{
	if (filename.find(".frag") != std::string::npos)
	{
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	else if (filename.find(".vert") != std::string::npos)
	{
		return VK_SHADER_STAGE_VERTEX_BIT;
	}
	else if (filename.find(".comp") != std::string::npos)
	{
		return VK_SHADER_STAGE_COMPUTE_BIT;
	}

	return VK_SHADER_STAGE_ALL;
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
	uint32_t memoryTypeIndex = VK::Utils::findMemoryType(gpu, memRequirements.memoryTypeBits, memProps);

	vkDestroyImage(device, image, nullptr);

	return std::make_pair(memoryTypeIndex, memRequirements.alignment);
}

size_t VKDevice::GetMemoryFromBuffer(EntryHandle hostIndex, size_t size, uint32_t alignment)
{
	std::shared_lock lock(deviceLock);

	auto alloc = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(hostIndex));

	return alloc->alloc.GetMemory(size, alignment);
}

uint32_t VKDevice::PresentSwapChain(EntryHandle swapChainIdx, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle commandBufferIndex)
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

	uint32_t waitCount = 1;
	
	VkSemaphore* waitSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * waitCount));

	for (uint32_t i = 0; i < waitCount; i++)
	{
		waitSemaphores[i] = GetSemaphore(swc->signalSemaphores[imageIndex]);
	}

	

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = waitCount;
	presentInfo.pWaitSemaphores = waitSemaphores;

	VkResult results[2];

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swc->swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = results;


	if (swc->presentationFences)
	{
		VkFence fence = GetFence(swc->presentationFences[frameInFlight]);
		VkSwapchainPresentFenceInfoEXT info{};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
		info.swapchainCount = 1;
		info.pFences = &fence;

		vkResetFences(device, 1, &fence);

		presentInfo.pNext = &info;

	}

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

uint32_t VKDevice::SubmitCommandsForSwapChain(EntryHandle swapChainIdx, uint32_t frameIndex, uint32_t imageIndex, EntryHandle cbIndex)
{
	std::shared_lock lock(deviceLock);
	VKSwapChain* swc = GetSwapChain(swapChainIdx);

	VkPipelineStageFlags* waitStages = reinterpret_cast<VkPipelineStageFlags*>(AllocFromDeviceCache(sizeof(VkPipelineStageFlags)));

	waitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	uint32_t ret =  SubmitCommandBuffer(&swc->waitSemaphores[frameIndex], waitStages, &swc->signalSemaphores[imageIndex], 1, 1, cbIndex);

	return ret;
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
	VkImage image = GetImageByHandle(imageIndex);
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


void VKDevice::UpdateRenderGraph(EntryHandle renderPass, uint32_t* dynamicOffsets, uint32_t dos, EntryHandle *perGraphDescriptor, uint32_t descriptorCount, uint32_t* dynamicPerSet)
{
//	std::shared_lock lock(deviceLock);

	auto graph = GetRenderGraph(renderPass);
	for (uint32_t i = 0; i < descriptorCount; i++)
	{
		graph->descriptorId[i] = perGraphDescriptor[i];
		graph->dynamicsPerSet[i] = dynamicPerSet[i];
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
	auto deviceMem = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(hostIndex));
	void* datalocal;
	vkMapMemory(device, deviceMem->memory, offset, size, 0, reinterpret_cast<void**>(&datalocal));
	std::memcpy(datalocal, data, size);
	vkUnmapMemory(device, deviceMem->memory);
}

void VKDevice::WriteToDeviceBuffer(EntryHandle deviceIndex, EntryHandle stagingBufferIndex, void* data, size_t size, size_t offset, int copies, int stride)
{
	std::shared_lock lock(deviceLock);
	auto queueDetails = GetQueueHandle(TRANSFER);

	uint32_t managerIndex = std::get<0>(queueDetails);
	uint32_t queueIndex = std::get<1>(queueDetails);
	uint32_t queueFamilyIndex = std::get<2>(queueDetails);
	EntryHandle poolIndex = std::get<3>(queueDetails);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);


	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;


	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(GetVkTypeFromEntry(stagingBufferIndex));

	stagingBuffer = alloc->buffer;
	stagingMemory = alloc->memory;
	VkDeviceSize allocOffset = alloc->alloc.GetMemory(size, 64);

	void* ldata;
	
	vkMapMemory(device, stagingMemory, allocOffset, size, 0, &ldata);
	
	std::memcpy(ldata, data, size);
		
	vkUnmapMemory(device, stagingMemory);

	VkCommandPool pool = GetCommandPool(poolIndex);

	VkBuffer outBuffer = GetBufferHandle(deviceIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::CopyBuffer(device, pool, queue, stagingBuffer, outBuffer, size, allocOffset, offset, copies, stride);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(managerIndex, queueIndex);

	alloc->alloc.FreeMemory(allocOffset);

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
	queueSema(),
	bitwiseMutex()
{

	assert(maxQueueCount <= 16);

	poolIndices = reinterpret_cast<EntryHandle*>(data);

	queueCountCV = _mqc;

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
	{
		std::unique_lock guard(queueSema);
		queueCV.wait(guard, [this]() { return queueCountCV > 0; });
		queueCountCV--;
	}

	std::unique_lock lock(bitwiseMutex);
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
		std::scoped_lock lock(bitwiseMutex);
		bitmap.reset(queueNum);
	}

	{
		std::scoped_lock guard(queueSema);
		queueCountCV++;
	}
	queueCV.notify_one();
}

uint32_t QueueManager::ConvertQueueProps(uint32_t flags, bool present)
{
	uint32_t flag = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) * GRAPHICS;
	flag |= ((flags & VK_QUEUE_COMPUTE_BIT) != 0) * COMPUTE;
	flag |= ((flags & VK_QUEUE_TRANSFER_BIT) != 0) * TRANSFER;
	flag |= (present * PRESENT);
	return flag;
}