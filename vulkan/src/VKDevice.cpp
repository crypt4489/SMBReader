
#include "pch.h"
#include "VKDevice.h"

#include "GlslangCompiler.h"

#include "VKInstance.h"
#include "VKRenderPassBuilder.h"
#include "VKDescriptorLayoutBuilder.h"
#include "VKDescriptorSetBuilder.h"
#include "VKPipelineBuilder.h"
#include "VKSwapChain.h"
#include "VKTexture.h"
#include "VKUtilities.h"

#include <cassert>

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
		if (startingAddress & (alignment - 1))
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

void VKMemoryAllocator::Reset()
{
	occupiedList.clear();
	freeList.clear();
	auto firstRange = std::make_pair(0U, capacity);
	freeList.emplace_front(firstRange);
}

VKMemoryAllocatorDetails VKMemoryAllocator::GetMemoryAllocDetails()
{
	VKMemoryAllocatorDetails details{ 0, capacity };

	auto iter = occupiedList.begin();

	while (iter != std::end(occupiedList))
	{
		VkDeviceSize allocSize = iter->second - iter->first;
		details.allocSize += allocSize;
		iter++;
	}

	return details;
}


RenderTarget::RenderTarget(EntryHandle renderPass, uint32_t imageCount, uint32_t _wSize, uint32_t _hSize, uint32_t _wOff, uint32_t _hOff, void* data)
{
	renderPassIndex = renderPass;
	count = imageCount;

	EntryHandle* head = reinterpret_cast<EntryHandle*>(data);

	framebufferIndices = head;

	width = _wSize;
	height = _hSize;
	wOffset = _wOff;
	hOffset = _hOff;
}

DeviceOwnedAllocator::DeviceOwnedAllocator() {
	writeHead = 0;
	size = 0;
	memHead = 0;
}

void* DeviceOwnedAllocator::Alloc(size_t inSize)
{
	size_t val, desired, out;
	val = writeHead.load(std::memory_order_acquire);
	do 
	{
		desired = val + inSize;

		if (desired >= size)
		{
			return nullptr;
		}

		out = val;

	} while (!writeHead.compare_exchange_weak(val, desired, std::memory_order_release, std::memory_order_relaxed));

	uintptr_t dest = memHead + out;

	memset((void*)dest, 0, inSize);

	return reinterpret_cast<void*>(dest);
}

void* DeviceOwnedAllocator::AllocWrapAround(size_t inSize)
{
	size_t val, desired, out;
	val = writeHead.load(std::memory_order_acquire);
	do 
	{
		desired = val + inSize;
		
		out = val;

		if (desired >= size)
		{
			out = 0;
			desired = inSize;
		}

		if (desired > size)
		{
			return nullptr;
		}

	} while (!writeHead.compare_exchange_weak(val, desired, std::memory_order_release, std::memory_order_relaxed));

	uintptr_t dest = memHead + out;

	memset((void*)dest, 0, inSize);

	return reinterpret_cast<void*>(dest);
}


RecordingBufferObject::RecordingBufferObject(VKDevice* device, VKCommandBuffer buffer) :
	cbBufferHandler(buffer), vkDeviceHandle(device), currLayout(VK_NULL_HANDLE), currPipeline(VK_NULL_HANDLE)
{

}

int RecordingBufferObject::BindGraphicsPipeline(EntryHandle pipelinename)
{
	int retCode = BindPipelineInternal(pipelinename, VK_PIPELINE_BIND_POINT_GRAPHICS);
	return retCode;
}

int RecordingBufferObject::BindComputePipeline(EntryHandle pipelineId) 
{
	int retCode = BindPipelineInternal(pipelineId, VK_PIPELINE_BIND_POINT_COMPUTE);
	return retCode;
}

int RecordingBufferObject::BindPipelineInternal(EntryHandle id, VkPipelineBindPoint bindPoint) 
{
	PipelineCacheObject* pco = vkDeviceHandle->GetPipelineCacheObject(id);
	currLayout = pco->pipelineLayout;
	currPipeline = pco->pipeline;
	vkCmdBindPipeline(cbBufferHandler.buffer, bindPoint, currPipeline);
	return 0;
}

int RecordingBufferObject::BindGraphicsDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet, 
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	VkDescriptorSet descset = vkDeviceHandle->GetDescriptorSet(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer, 
		VK_PIPELINE_BIND_POINT_GRAPHICS, currLayout, 
		firstDescriptorSet, descriptorCount, 
		&descset, dynamicOffsetCount, 
		offsets);
	return 0;
}

int RecordingBufferObject::BindComputeDescriptorSets(EntryHandle descriptorname, uint32_t descriptorNumber, uint32_t descriptorCount, uint32_t firstDescriptorSet,
	uint32_t dynamicOffsetCount, uint32_t* offsets)
{
	VkDescriptorSet descset = vkDeviceHandle->GetDescriptorSet(descriptorname, descriptorNumber);
	vkCmdBindDescriptorSets(
		cbBufferHandler.buffer,
		VK_PIPELINE_BIND_POINT_COMPUTE, currLayout,
		firstDescriptorSet, descriptorCount,
		&descset, dynamicOffsetCount,
		offsets);
	return 0;
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

void RecordingBufferObject::BindingIndexedIndirectDrawCmd(EntryHandle indirectBufferIndex, uint32_t drawCount, size_t indirectBufferOffset)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(indirectBufferIndex);
	vkCmdDrawIndexedIndirect(cbBufferHandler.buffer, buffer, indirectBufferOffset, drawCount, sizeof(VkDrawIndexedIndirectCommand));
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
	VkClearValue* clearVals, uint32_t clearValCount)
{

	VkRenderPassBeginInfo renderPassInfo{};
	auto ref = vkDeviceHandle->GetRenderTarget(renderTargetIndex);
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vkDeviceHandle->GetRenderPass(ref->renderPassIndex);
	renderPassInfo.framebuffer = vkDeviceHandle->GetFrameBuffer(ref->framebufferIndices[imageIndex]);
	renderPassInfo.renderArea = rect;

	renderPassInfo.clearValueCount = clearValCount;
	renderPassInfo.pClearValues = clearVals;

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


int RecordingBufferObject::BeginRecordingCommand(VkCommandBufferInheritanceInfo *info, VkCommandBufferUsageFlags flags)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = flags;
	beginInfo.pInheritanceInfo = info;

	if (vkBeginCommandBuffer(cbBufferHandler.buffer, &beginInfo) != VK_SUCCESS) 
	{
		return -(RBO_FAILED_TO_BEGIN_RECORD);
	}

	return 0;
}

int RecordingBufferObject::EndRecordingCommand()
{
	if (vkEndCommandBuffer(cbBufferHandler.buffer) != VK_SUCCESS) 
	{
		return -(RBO_FAILED_TO_END_RECORD);
	}

	return 0;
}

int RecordingBufferObject::CommandBufferReset()
{
	if (vkResetCommandBuffer(cbBufferHandler.buffer, 0) != VK_SUCCESS)
	{
		return -(RBO_FAILED_TO_RESET_BUFFER);
	}

	return 0;
}

int RecordingBufferObject::ResetCommandPoolForBuffer()
{
	VkCommandPool pool = vkDeviceHandle->GetCommandPool(cbBufferHandler.poolIndex);
	
	if (vkResetCommandPool(vkDeviceHandle->device, pool, 0) != VK_SUCCESS)
	{
		return -(RBO_FAILED_TO_RESET_POOL);
	}

	return 0;
}

int RecordingBufferObject::ExecuteSecondaryCommands(EntryHandle* handles, uint32_t count)
{
	VkCommandBuffer* lbuffers = reinterpret_cast<VkCommandBuffer*>(vkDeviceHandle->AllocFromDeviceCache(sizeof(VkCommandBuffer) * count));
	
	for (uint32_t i = 0; i < count; i++)
	{
		VKCommandBuffer* lbuffer = vkDeviceHandle->GetCommandBuffer(handles[i]);
		lbuffers[i] = lbuffer->buffer;
	}

	vkCmdExecuteCommands(cbBufferHandler.buffer, count, lbuffers);

	return 0;
}

void RecordingBufferObject::FillBuffer(EntryHandle bufferHandle, size_t size, size_t offset, uint32_t val)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(bufferHandle);
	vkCmdFillBuffer(cbBufferHandler.buffer, buffer, offset, size, val);
}

void RecordingBufferObject::UpdateBuffer(EntryHandle bufferHandle, size_t size, size_t offset, void* val)
{
	VkBuffer buffer = vkDeviceHandle->GetBufferHandle(bufferHandle);
	vkCmdUpdateBuffer(cbBufferHandler.buffer, buffer, offset, size, val);
}

void RecordingBufferObject::BindingDrawIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount)
{
	VkBuffer combuffer = vkDeviceHandle->GetBufferHandle(commandBufferIndex);
	VkBuffer cntbuffer = vkDeviceHandle->GetBufferHandle(countBufferIndex);
	vkCmdDrawIndirectCount(cbBufferHandler.buffer, combuffer, commandBufferOffset, cntbuffer, countBufferOffset, maxDrawCount, sizeof(VkDrawIndirectCommand));
}

void RecordingBufferObject::BindingDrawIndexedIndirectCount(EntryHandle commandBufferIndex, EntryHandle countBufferIndex, size_t commandBufferOffset, size_t countBufferOffset, uint32_t maxDrawCount)
{
	VkBuffer combuffer = vkDeviceHandle->GetBufferHandle(commandBufferIndex);
	VkBuffer cntbuffer = vkDeviceHandle->GetBufferHandle(countBufferIndex);
	vkCmdDrawIndexedIndirectCount(cbBufferHandler.buffer, combuffer, commandBufferOffset, cntbuffer, countBufferOffset, maxDrawCount, sizeof(VkDrawIndexedIndirectCommand));
}


void RecordingBufferObject::BeginQuery(EntryHandle queryPoolHandle, uint32_t queryIndex)
{
	VkQueryPool poolHandle = vkDeviceHandle->GetQueryPool(queryPoolHandle);

	vkCmdBeginQuery(cbBufferHandler.buffer, poolHandle, queryIndex, 0);
}

void RecordingBufferObject::EndQuery(EntryHandle queryPoolHandle, uint32_t queryIndex)
{
	VkQueryPool poolHandle = vkDeviceHandle->GetQueryPool(queryPoolHandle);

	vkCmdEndQuery(cbBufferHandler.buffer, poolHandle, queryIndex);
}

void RecordingBufferObject::WriteTimestamp(EntryHandle queryPoolHandle, uint32_t queryIndex, VkPipelineStageFlagBits pipelineStage)
{
	VkQueryPool poolHandle = vkDeviceHandle->GetQueryPool(queryPoolHandle);

	vkCmdWriteTimestamp(cbBufferHandler.buffer, pipelineStage, poolHandle, queryIndex);
}

void RecordingBufferObject::ResetQueries(EntryHandle poolIndex, uint32_t firstQuery, uint32_t queryCount)
{
	VkQueryPool poolHandle = vkDeviceHandle->GetQueryPool(poolIndex);

	vkCmdResetQueryPool(cbBufferHandler.buffer, poolHandle, firstQuery, queryCount);
}

static size_t FindQueueManagerByCapapbilites(QueueManager* managers, size_t managerSize, uint32_t capabilities)
{
	size_t i = 0;
	for (; i < managerSize; i++)
	{
		if ((managers[i].queueCapabilities & capabilities) == capabilities) 
		{
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
	entries(nullptr),
	indexForEntries(0),
	numberOfEntries(0),
	deviceDataAlloc(),
	deviceCacheAlloc()
{

}

//allocations

EntryHandle VKDevice::AddVkTypeToEntry(void* handle, HandleType type)
{
	size_t ret = ~0ull;

	if (freeListTop >= 0)
	{
		ret = handlesFreeList[freeListTop--];
	} 
	else
	{
		if (indexForEntries >= numberOfEntries)
		{
			return EntryHandle();
		}

		ret = indexForEntries++;
	}

	entries[ret].memoryLocation = reinterpret_cast<uintptr_t>(handle);
	entries[ret].type = type;

	return EntryHandle(ret);
}


void* VKDevice::AllocFromPerDeviceData(size_t size)
{
	return deviceDataAlloc.Alloc(size);
}

HandlePoolObject VKDevice::GetVkTypeFromEntry(EntryHandle index)
{
	if (index() >= indexForEntries) 
	{
		return { VulkMaxEnum, 0ull };
	}

	return entries[index()];
}

void VKDevice::ReturnHandleObject(EntryHandle index)
{
	if (index() >= indexForEntries) 
	{
		return;
	}

	entries[index()].memoryLocation = 0;
	entries[index()].type = VulkMaxEnum;

	int freeListLoc = ++freeListTop;

	handlesFreeList[freeListLoc] = index();
}

void VKDevice::AddDeviceErrorCode(int internalErrorCode, VkResult vulkSpecificResult)
{
	int instErrorCodePos = currentErrorCodePos;

	currentErrorCodePos = (currentErrorCodePos + 1) & (errorCodeWrapSize - 1);

	DeviceErrorCodeStruct* errorCode = &errorCodes[instErrorCodePos];

	errorCode->internalErrorCode = internalErrorCode;
	errorCode->vkResult = vulkSpecificResult;
}


void* VKDevice::AllocFromDeviceCache(size_t size)
{
	return deviceCacheAlloc.AllocWrapAround(size);;
}

//CREATORS

EntryHandle VKDevice::CompileShader(char* data, VkShaderStageFlags flags)
{
	VkShaderModule mod = VK_NULL_HANDLE;

	VkResult vkRes = VK_SUCCESS;

	GLSLShaderProgram shaderProgramHold;

	ShaderHandle* modHandle = reinterpret_cast<ShaderHandle*>(AllocFromPerDeviceData(sizeof(ShaderHandle)));

	if (!modHandle)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	int retCode = GLSLANG::CompileShader(device, data, flags, &shaderProgramHold);

	if (retCode < 0)
	{
		return EntryHandle();
	}

	retCode = VK::Utils::CreateShaderModule(device, shaderProgramHold.shaderRawData, shaderProgramHold.shaderRawDataSize, &mod, &vkRes);

	GLSLANG::DeleteShader(&shaderProgramHold);

	if (retCode < 0)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SHADER_MODULE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	modHandle->sMod = mod;
	modHandle->flags = flags;

	EntryHandle ret = AddVkTypeToEntry(modHandle, VulkShaderHandle);

	if (ret == EntryHandle())
	{
		vkDestroyShaderModule(device, mod, nullptr);
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}

	return ret;
}

EntryHandle VKDevice::CreateBufferView(EntryHandle bufferHandle, VkFormat format, size_t rangeSize, size_t offset, uint32_t numberOfAllocs)
{
	BufferView* viewsHandles = (BufferView*)AllocFromPerDeviceData(sizeof(BufferView) + (sizeof(VkBufferView) * numberOfAllocs));

	if (!viewsHandles)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}
	
	viewsHandles->views = (VkBufferView*)(viewsHandles + 1);
	viewsHandles->count = numberOfAllocs;

	VkBuffer buffer = GetBufferHandle(bufferHandle);

	if (!buffer)
	{
		return EntryHandle();
	}
	
	VkBufferViewCreateInfo info{};

	info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	info.buffer = buffer;
	info.offset = offset;
	info.range = rangeSize;
	info.format = format;

	for (uint32_t i = 0; i < numberOfAllocs; i++)
	{
		VkResult vkRes = VK_SUCCESS;

		if ((vkRes = vkCreateBufferView(device, &info, nullptr, &viewsHandles->views[i])) != VK_SUCCESS) 
		{
			
			AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_VIEW_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);

			for (uint32_t j = 0; j < i; j++)
			{
				vkDestroyBufferView(device, viewsHandles->views[j], nullptr);
			}

			return EntryHandle();

		}

		info.offset += rangeSize;
	}

	EntryHandle poolIndex = AddVkTypeToEntry(viewsHandles, VulkBufferView);

	if (poolIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);

		for (uint32_t j = 0; j < numberOfAllocs; j++)
		{
			vkDestroyBufferView(device, viewsHandles->views[j], nullptr);
		}
	}

	return poolIndex;
}

EntryHandle VKDevice::CreateBufferViewFromImagePool(EntryHandle poolIndex, VkFormat format, size_t rangeSize, size_t offset)
{
	ImageMemoryPool* pool = GetImageMemoryPool(poolIndex);

	if (!pool)
	{
		return EntryHandle();
	}

	TexelBufferView* viewData = reinterpret_cast<TexelBufferView*>(AllocFromPerDeviceData(sizeof(TexelBufferView)));

	if (!viewData)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkBuffer buffer;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = rangeSize;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	if ((vkRes = vkBindBufferMemory(device, buffer, pool->memory, offset)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BIND_MEMORY_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		vkDestroyBuffer(device, buffer, nullptr);
		return EntryHandle();
	}

	EntryHandle buffHandle = AddVkTypeToEntry(buffer, VulkBuffer);

	if (buffHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyBuffer(device, buffer, nullptr);
		return EntryHandle();
	}

	EntryHandle viewHandle = CreateBufferView(buffHandle, format, rangeSize, offset, 1);

	if (viewHandle == EntryHandle())
	{
		vkDestroyBuffer(device, buffer, nullptr);
		return EntryHandle();
	}

	viewData->bufferHandle = buffHandle;
	viewData->viewHandle = viewHandle;

	EntryHandle finalBufferViewHandle = AddVkTypeToEntry(viewData, VulkImageBufferView);

	if (finalBufferViewHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyBuffer(device, buffer, nullptr);
		DestroyBufferView(viewHandle);
	}

	return finalBufferViewHandle;
}

EntryHandle VKDevice::CreateCommandPool(QueueIndex queueIndex)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueIndex;

	VkCommandPool pool = nullptr;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateCommandPool(device, &poolInfo, nullptr, &pool)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_COMMAND_POOL_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle poolIndex = AddVkTypeToEntry(pool, VulkCommandPool);

	if (poolIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyCommandPool(device, pool, nullptr);
	}

	return poolIndex;
}

EntryHandle VKDevice::CreateDesciptorPool(DescriptorPoolBuilder* builder, uint32_t maxSets)
{
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	
	uint32_t poolSizeCount = static_cast<uint32_t>(builder->numPoolSizes);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizeCount;
	poolInfo.pPoolSizes = builder->poolSizes;
	poolInfo.maxSets = maxSets;
	poolInfo.flags = builder->flags;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_POOL_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle poolIndex = AddVkTypeToEntry(descriptorPool, VulkDescriptorPool);

	if (poolIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	return poolIndex;
}

DescriptorPoolBuilder VKDevice::CreateDescriptorPoolBuilder(size_t poolSize, VkDescriptorPoolCreateFlags flags)
{
	DescriptorPoolBuilder builder(&deviceCacheAlloc, poolSize, flags);

	return builder;
}

DescriptorSetBuilder* VKDevice::CreateDescriptorSetBuilder(EntryHandle poolIndex, EntryHandle descriptorLayout, uint32_t numberofsets, uint32_t varDescriptorCounts)
{
	DescriptorSetBuilder* data = reinterpret_cast<DescriptorSetBuilder*>(AllocFromDeviceCache(sizeof(DescriptorSetBuilder)));

	DescriptorSetBuilder *dsb = std::construct_at(data, this, numberofsets);
	
	VkDescriptorSetLayout ref = GetDescriptorSetLayout(descriptorLayout);

	dsb->AllocDescriptorSets(GetDescriptorPool(poolIndex), ref, numberofsets, varDescriptorCounts);

	return dsb;
}

DescriptorSetBuilder* VKDevice::UpdateDescriptorSet(EntryHandle descriptorHandle)
{
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

EntryHandle VKDevice::CreateDescriptorSet(VkDescriptorSet* set, uint32_t numberOfSets) 
{
	DescriptorSetAlloc* alloc = reinterpret_cast<DescriptorSetAlloc*>(AllocFromPerDeviceData(sizeof(DescriptorSetAlloc)));

	if (!alloc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}
	
	alloc->numberOfSets = numberOfSets;
	alloc->sets = set;
	
	EntryHandle setHandle;

	setHandle = AddVkTypeToEntry(alloc, VulkDescriptorSet);

	if (setHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}
	
	return setHandle;
}

EntryHandle VKDevice::CreateDescriptorSetLayout(DescriptorSetLayoutBuilder* builder)
{
	VkDescriptorSetLayout descLay = builder->CreateDescriptorSetLayout();

	EntryHandle setLayoutHandle;

	setLayoutHandle = AddVkTypeToEntry(descLay, VulkDescriptorLayout);

	if (setLayoutHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}
	
	return setLayoutHandle;
}


EntryHandle* VKDevice::CreateFences(uint32_t count, VkFenceCreateFlags flags)
{
	EntryHandle* tempFenceIndices = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * count));

	if (!tempFenceIndices)
	{
		AddDeviceErrorCode(DEVICE_CACHE_ALLOC_TOO_LARGE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = flags;

	for (uint32_t i = 0; i < count; i++) 
	{

		VkResult vkRes = VK_SUCCESS;

		VkFence fence = VK_NULL_HANDLE;

		if ((vkRes = vkCreateFence(device, &fenceInfo, nullptr, &fence)) != VK_SUCCESS) 
		{
			AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_FENCE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);

			for (uint32_t j = 0; j < i; j++)
			{
				DestroyFence(tempFenceIndices[j]);
			}

			return nullptr;
		}
	
		tempFenceIndices[i] = AddVkTypeToEntry(fence, VulkFence);

		if (tempFenceIndices[i] == EntryHandle())
		{
			AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);

			for (uint32_t j = 0; j <= i; j++)
			{
				DestroyFence(tempFenceIndices[j]);
			}

			return nullptr;
		}
	}
	
	return tempFenceIndices;
}

EntryHandle VKDevice::CreateFrameBuffer(EntryHandle* attachmentIndices, uint32_t attachmentsCount, EntryHandle renderPassIndex, VkExtent2D extent)
{
	VkImageView* attachments = reinterpret_cast<VkImageView*>(AllocFromDeviceCache(sizeof(VkImageView) * attachmentsCount));

	if (!attachments)
	{
		AddDeviceErrorCode(DEVICE_CACHE_ALLOC_TOO_LARGE, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	for (uint32_t i = 0; i < attachmentsCount; i++) 
	{
		attachments[i] = GetImageViewByHandle(attachmentIndices[i]);
		if (attachments[i] == VK_NULL_HANDLE)
		{
			return EntryHandle();
		}
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = GetRenderPass(renderPassIndex);
	framebufferInfo.attachmentCount = attachmentsCount;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	VkFramebuffer frameBuffer = VK_NULL_HANDLE;

	VkResult vkRes = VK_SUCCESS;

	if ((vkRes = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frameBuffer)) != VK_SUCCESS) 
	{	
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_FRAMEBUFFER_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);

		return EntryHandle();
	}

	EntryHandle fbIndex = AddVkTypeToEntry(frameBuffer, VulkFrameBuffer);

	if (fbIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyFramebuffer(device, frameBuffer, nullptr);
	}

	return fbIndex;
}

EntryHandle VKDevice::CreateHostBuffer(VkDeviceSize allocSize, bool coherent, VkBufferUsageFlags usage)
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkResult vkRes = VK_SUCCESS;

	BufferAlloc* alloc = nullptr;

	alloc = reinterpret_cast<BufferAlloc*>(AllocFromPerDeviceData(sizeof(BufferAlloc)));

	if (!alloc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	int retCode =  VK::Utils::CreateBuffer(
		device, gpu, allocSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
		(coherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0), 
		VK_SHARING_MODE_EXCLUSIVE, usage,
		&buffer, &memory, &vkRes
	);

	if (retCode < 0)
	{
		int minorCode = 0;
		switch (retCode)
		{
		case -1:
			minorCode = DEVICE_VK_TYPE_BUFFER_FAILED;
			break;
		case -2:
			minorCode = DEVICE_VK_TYPE_MEMORY_FAILED;
			break;
		case -3:
			minorCode = DEVICE_VK_TYPE_BIND_MEMORY_FAILED;
			break;
		default:
			break;
		}

		AddDeviceErrorCode(MINOR_CODE_PACK(minorCode) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	alloc->buffer = buffer;
	alloc->memory = memory;

	std::construct_at(&alloc->alloc, allocSize);

	EntryHandle buffIndex = AddVkTypeToEntry(alloc, VulkBuffer);

	if (buffIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkFreeMemory(device, memory, nullptr);
		vkDestroyBuffer(device, buffer, nullptr);
	}

	return buffIndex;
}


EntryHandle VKDevice::CreateDeviceBuffer(VkDeviceSize allocSize, VkBufferUsageFlags usage)
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkResult vkRes = VK_SUCCESS;

	BufferAlloc* alloc = nullptr;

	alloc = reinterpret_cast<BufferAlloc*>(AllocFromPerDeviceData(sizeof(BufferAlloc)));

	if (!alloc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	int retCode = VK::Utils::CreateBuffer(
		device, gpu, allocSize,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, usage,
		&buffer, &memory, &vkRes
	);

	if (retCode < 0)
	{
		int minorCode = 0;
		switch (retCode)
		{
		case -1:
			minorCode = DEVICE_VK_TYPE_BUFFER_FAILED;
			break;
		case -2:
			minorCode = DEVICE_VK_TYPE_MEMORY_FAILED;
			break;
		case -3:
			minorCode = DEVICE_VK_TYPE_BIND_MEMORY_FAILED;
			break;
		default:
			break;
		}

		AddDeviceErrorCode(MINOR_CODE_PACK(minorCode) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	alloc->buffer = buffer;
	alloc->memory = memory;

	std::construct_at(&alloc->alloc, allocSize);

	EntryHandle buffIndex = AddVkTypeToEntry(alloc, VulkBuffer);

	if (buffIndex == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkFreeMemory(device, memory, nullptr);
		vkDestroyBuffer(device, buffer, nullptr);
	}

	return buffIndex;
}

EntryHandle VKDevice::CreateImage(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps, VkImageLayout layout, VkImageTiling tiling, VkImageCreateFlags cflags, VkImageType imageType, EntryHandle memIndex)
{
	VkResult vkRes = VK_SUCCESS;

	VkImage image;

	ImageMemoryPool* imageMemoryPool = GetImageMemoryPool(memIndex);

	if (!imageMemoryPool)
	{
		return EntryHandle();
	}

	ImageAllocation* alloc = reinterpret_cast<ImageAllocation*>(AllocFromPerDeviceData(sizeof(ImageAllocation)));

	if (!alloc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkImageCreateInfo imageInfo{};

	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = imageType;
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

	if ((vkRes = vkCreateImage(device, &imageInfo, nullptr, &image)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_HANDLE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkDeviceSize addr = imageMemoryPool->alloc.GetMemory(memRequirements.size, memRequirements.alignment);

	alloc->imageHandle = image;
	alloc->deviceMemoryAddress = addr;
	alloc->memIndex = memIndex;

	if ((vkRes = vkBindImageMemory(device, image, imageMemoryPool->memory, addr)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BIND_MEMORY_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		vkDestroyImage(device, image, nullptr);
		imageMemoryPool->alloc.FreeMemory(addr);
		return EntryHandle();
	}

	EntryHandle imageHandle = AddVkTypeToEntry(alloc, VulkImageHandle);

	if (imageHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyImage(device, image, nullptr);
		imageMemoryPool->alloc.FreeMemory(addr);
	}
	
	return imageHandle;
}


EntryHandle VKDevice::CreateStorageImage(
	uint32_t width, uint32_t height,
	uint32_t mipLevels, VkFormat type,
	EntryHandle memIndex,
    VkImageAspectFlags flags, VkImageLayout layout)
{
	
	/*

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, type, 1,
		VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, VK_IMAGE_TYPE_2D, memIndex
	);

	EntryHandle viewIndex = CreateImageView(imageIndex, mipLevels, 1, type, flags, VK_IMAGE_VIEW_TYPE_2D);

	

	VKTexture* tex = reinterpret_cast<VKTexture*>(AllocFromPerDeviceData(sizeof(VKTexture)));

	tex = std::construct_at(tex, imageIndex, &viewIndex, 1, nullptr, 0);

	EntryHandle ret = AddVkTypeToEntry(tex, VulkTextureHandle);

	VKDevice::QueueDetails queueDetails = GetQueueHandle(GRAPHICSQUEUE | TRANSFERQUEUE);

	VkQueue queue;
	vkGetDeviceQueue(device, queueDetails.queueFamilyIndex, queueDetails.queueIndex, &queue);

	VkImage image = GetImageByHandle(imageIndex);

	VkCommandPool pool = GetCommandPool(queueDetails.poolIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, type, VK_IMAGE_LAYOUT_UNDEFINED, layout, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(queueDetails.managerIndex, queueDetails.queueIndex);
	*/

	return EntryHandle();
}


EntryHandle VKDevice::CreateImageMemoryPool(VkDeviceSize poolSize, uint32_t memoryTypeIndex)
{
	VkResult vkRes = VK_SUCCESS;

	VkDeviceMemory deviceMemory;

	ImageMemoryPool* alloc = reinterpret_cast<ImageMemoryPool*>(AllocFromPerDeviceData(sizeof(ImageMemoryPool)));

	if (!alloc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = poolSize;
	allocInfo.memoryTypeIndex = memoryTypeIndex;

	if ((vkRes = vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_MEMORY_ALLOCATION_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	std::construct_at(&alloc->alloc, poolSize);

	alloc->memory = deviceMemory;

	EntryHandle imageMemoryPoolHandle = AddVkTypeToEntry(alloc, VulkImageMemoryPool);

	if (imageMemoryPoolHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkFreeMemory(device, deviceMemory, nullptr);
	}
	
	return imageMemoryPoolHandle;
}

EntryHandle VKDevice::CreateImageView(
	EntryHandle imageIndex, uint32_t mipLevels, uint32_t layersCount,
	VkFormat type, VkImageAspectFlags aspectMask, VkImageViewType imageViewType)
{
	VkResult vkRes = VK_SUCCESS;

	VkImageViewCreateInfo viewInfo{};

	VkImage image = GetImageByHandle(imageIndex);

	VkImageView imageView = VK_NULL_HANDLE;

	if (image == VK_NULL_HANDLE)
	{
		return EntryHandle();
	}

	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = imageViewType;
	viewInfo.format = type;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layersCount;

	if ((vkRes = vkCreateImageView(device, &viewInfo, nullptr, &imageView)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_MEMORY_ALLOCATION_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle imageViewHandle = AddVkTypeToEntry(imageView, VulkImageView);

	if (imageViewHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyImageView(device, imageView, nullptr);
	}
	
	return imageViewHandle;
}

EntryHandle VKDevice::CreateImageView(
	VkImage image, uint32_t mipLevels, uint32_t layersCount,
	VkFormat type, VkImageAspectFlags aspectMask, VkImageViewType imageViewType)
{
	VkResult vkRes = VK_SUCCESS;

	VkImageViewCreateInfo viewInfo{};

	VkImageView imageView = VK_NULL_HANDLE;

	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = imageViewType;
	viewInfo.format = type;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layersCount;

	if ((vkRes = vkCreateImageView(device, &viewInfo, nullptr, &imageView)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_MEMORY_ALLOCATION_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle imageViewHandle = AddVkTypeToEntry(imageView, VulkImageView);

	if (imageViewHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyImageView(device, imageView, nullptr);
	}

	return imageViewHandle;
}

int VKDevice::CreateLogicalDevice(
	const char** instanceLayers,
	uint32_t layerCount,
	const char** deviceExtensions,
	uint32_t deviceExtCount,
	VkPhysicalDeviceFeatures2* features,
	QueueIndex* queueIndices,
	uint32_t* queueMaxCounts,
	float* queuePrios,
	uint32_t queueCount,
	size_t perDeviceDataSize,
	size_t perEntriesSize,
	size_t perCacheSize,
	size_t driverPerSize,
	size_t driverPerCache,
	void* driverPoolHead,
	void* devicePoolHead
)
{
	uintptr_t tempDriverHead = (uintptr_t)driverPoolHead;
	uintptr_t tempDeviceHead = (uintptr_t)devicePoolHead;

	deviceDriverAllocator = (VKAllocationCB*)(tempDeviceHead);
	tempDeviceHead += sizeof(VKAllocationCB);

	deviceDriverAllocator->commandDataSize = driverPerCache;
	deviceDriverAllocator->commandData = (uint8_t*)tempDriverHead;

	tempDriverHead += driverPerCache;

	deviceDriverAllocator->instanceDataSize = driverPerSize;
	deviceDriverAllocator->instanceData = (uint8_t*)tempDriverHead;

	tempDriverHead += driverPerSize;

	deviceCacheAlloc.size = perCacheSize;
	deviceCacheAlloc.memHead = tempDeviceHead;

	tempDeviceHead += perCacheSize;

	deviceDataAlloc.size = perDeviceDataSize - perEntriesSize;
	deviceDataAlloc.memHead = tempDeviceHead;

	tempDeviceHead += deviceDataAlloc.size;

	size_t handleEntrySize = perEntriesSize >> 1;
	size_t freeListEntrySize = perEntriesSize >> 1;
	
	numberOfEntries = handleEntrySize / sizeof(HandlePoolObject);
	entries = (HandlePoolObject*)tempDeviceHead;

	tempDeviceHead += handleEntrySize;

	freeListTop = -1;
	handlesFreeList = (size_t*)tempDeviceHead;
	numberOfFreeListEntries = freeListEntrySize / sizeof(int);

	tempDeviceHead += freeListEntrySize;
	
	VkDeviceQueueCreateInfo* queueCreateInfos = reinterpret_cast<VkDeviceQueueCreateInfo*>(AllocFromDeviceCache(sizeof(VkDeviceQueueCreateInfo) * queueCount));

	for (uint32_t i = 0; i<queueCount; i++)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueIndices[i];
		queueCreateInfo.queueCount = queueMaxCounts[i];
		queueCreateInfo.pQueuePriorities = queuePrios;
		queueCreateInfos[i] = queueCreateInfo;
		queuePrios += queueMaxCounts[i];
	}

	VkDeviceCreateInfo logDeviceInfo{};
	logDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logDeviceInfo.queueCreateInfoCount = queueCount;
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
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_LOGICAL_DEVICE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, res);
		return -1;
	}

	return 0;
}

VKGraphicsPipelineBuilder* VKDevice::CreateGraphicsPipelineBuilder(EntryHandle renderPassIndex, uint32_t colorCount, uint32_t descLayoutCount, uint32_t dynamicStateCount, uint32_t pushConstantRangeCount)
{
	VkRenderPass renderPass = GetRenderPass(renderPassIndex);

	VKGraphicsPipelineBuilder* graphicsPipelineData = reinterpret_cast<VKGraphicsPipelineBuilder*>(AllocFromDeviceCache(sizeof(VKGraphicsPipelineBuilder)));

	return std::construct_at(graphicsPipelineData, renderPass, this, colorCount, descLayoutCount, dynamicStateCount, pushConstantRangeCount);
}

VKComputePipelineBuilder* VKDevice::CreateComputePipelineBuilder(size_t numberOfDescriptors, uint32_t pushConstantRangeCount)
{
	VKComputePipelineBuilder* computePB = reinterpret_cast<VKComputePipelineBuilder*>(AllocFromDeviceCache(sizeof(VKComputePipelineBuilder)));

	return std::construct_at(computePB, this, numberOfDescriptors, pushConstantRangeCount);
}

EntryHandle VKDevice::CreateQueueManager(uint32_t queueIndex, uint32_t maxCount, uint32_t queueFlags, bool presentsupport)
{
	void* queueManagerData = reinterpret_cast<void*>(AllocFromPerDeviceData(sizeof(EntryHandle) * maxCount));

	QueueManager* queueManagerItself = reinterpret_cast<QueueManager*>(AllocFromPerDeviceData(sizeof(QueueManager)));

	if (!queueManagerData || !queueManagerItself)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	std::construct_at(queueManagerItself,
		nullptr, 0,
		maxCount, queueIndex,
		queueFlags, presentsupport,
		this, queueManagerData);

	EntryHandle queueManagerHandle = AddVkTypeToEntry(queueManagerItself, VulkQueueManager);

	if (queueManagerHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		queueManagerItself->DestroyManager();
	}

	return queueManagerHandle;
}

EntryHandle VKDevice::CreateQueryPool(VkQueryType queryType, uint32_t numberOfQueries)
{
	EntryHandle queryPoolHandle;

	VkQueryPool vkPoolHandle = VK_NULL_HANDLE;

	VkResult vkRes = VK_SUCCESS;

	VkQueryPoolCreateInfo createInfo{};

	createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	createInfo.queryCount = numberOfQueries;
	createInfo.queryType = queryType;
	createInfo.pipelineStatistics = 0;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;

	if ((vkRes = vkCreateQueryPool(device, &createInfo, nullptr, &vkPoolHandle)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_QUERY_POOL_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	queryPoolHandle = AddVkTypeToEntry(vkPoolHandle, VulkQueryPool);

	if (queryPoolHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyQueryPool(device, vkPoolHandle, nullptr);
	}

	return queryPoolHandle;
}

EntryHandle VKDevice::CreatePipelineCacheObject(PipelineCacheObject* obj)
{
	PipelineCacheObject* perObj = reinterpret_cast<PipelineCacheObject*>(AllocFromPerDeviceData(sizeof(PipelineCacheObject)));

	if (!perObj)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	perObj->descLayout = obj->descLayout;
	perObj->pipeline = obj->pipeline;
	perObj->pipelineLayout = obj->pipelineLayout;

	EntryHandle pipeObjHandle;

	pipeObjHandle = AddVkTypeToEntry(perObj, VulkPipelineCacheObject);

	if (pipeObjHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}
	
	return pipeObjHandle;
}

EntryHandle VKDevice::CreateRenderPasses(VKRenderPassBuilder& builder)
{
	VkResult vkRes = VK_SUCCESS;

	VkRenderPass vkRenderPassHandle = VK_NULL_HANDLE;

	if ((vkRes = vkCreateRenderPass(device, &builder.createInfo, nullptr, &vkRenderPassHandle)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_RENDER_PASS_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle renderPassHandle = AddVkTypeToEntry(vkRenderPassHandle, VulkRenderPassHandle);

	if (renderPassHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroyRenderPass(device, vkRenderPassHandle, nullptr);
	}

	return renderPassHandle;
}

VKRenderPassBuilder VKDevice::CreateRenderPassBuilder(uint32_t numAttaches, uint32_t numDeps, uint32_t numDescs)
{
	return { this, numAttaches, numDeps, numDescs };
}

EntryHandle VKDevice::CreateRenderTarget(EntryHandle renderPassIndex, uint32_t framebufferCount, uint32_t width, uint32_t height, uint32_t wOffset, uint32_t hOffset)
{
	auto renderTarget = reinterpret_cast<RenderTarget*>(AllocFromPerDeviceData(sizeof(RenderTarget)));

	void* data = AllocFromPerDeviceData(sizeof(EntryHandle) * framebufferCount);

	if (!renderTarget || !data)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	std::construct_at(renderTarget, renderPassIndex, framebufferCount, width, height, wOffset, hOffset, data);
	
	EntryHandle renderTargetHandle = AddVkTypeToEntry(renderTarget, VulkRenderTarget);

	if (renderTargetHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}
	
	return renderTargetHandle;
}

EntryHandle* VKDevice::CreateReusableCommandBuffers(
	EntryHandle managerIndex, 
	uint32_t numberOfCommandBuffers, 
	bool createFences, 
	VkCommandBufferLevel level
)
{
	VkResult vkRes = VK_SUCCESS;

	EntryHandle* intHandles = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * numberOfCommandBuffers));

	VKCommandBuffer** temp = reinterpret_cast<VKCommandBuffer**>(AllocFromDeviceCache(sizeof(VKCommandBuffer*) * numberOfCommandBuffers));

	VKCommandBuffer* cbObjects = reinterpret_cast<VKCommandBuffer*>(AllocFromPerDeviceData(sizeof(VKCommandBuffer) * numberOfCommandBuffers));

	if (!intHandles || !temp || !cbObjects)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	VkCommandBuffer* l = reinterpret_cast<VkCommandBuffer*>(AllocFromDeviceCache(sizeof(VkCommandBuffer) * numberOfCommandBuffers));

	if (!l)
	{
		AddDeviceErrorCode(DEVICE_CACHE_ALLOC_TOO_LARGE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	VKDevice::QueueDetails queueDetails = GetQueueHandle(managerIndex);

	VkCommandPool pool = GetCommandPool(queueDetails.poolIndex);

	VkCommandBufferAllocateInfo allocInfo{};

	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = pool;
	allocInfo.level = level;
	allocInfo.commandBufferCount = numberOfCommandBuffers;

	if ((vkRes = vkAllocateCommandBuffers(device, &allocInfo, l)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_COMMAND_BUFFER_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return nullptr;
	}

	for (uint32_t i = 0; i < numberOfCommandBuffers; i++) 
	{	
		cbObjects[i].buffer = l[i];
		cbObjects[i].queueFamilyIndex = queueDetails.queueFamilyIndex;
		cbObjects[i].queueIndex = queueDetails.queueIndex;
		cbObjects[i].poolIndex = queueDetails.poolIndex;
		cbObjects[i].fenceIdx = EntryHandle();

		intHandles[i] = AddVkTypeToEntry(&cbObjects[i], VulkVKCommandBuffer);

		if (intHandles[i] == EntryHandle())
		{
			AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
			vkFreeCommandBuffers(device, pool, numberOfCommandBuffers, l);
			return nullptr;
		}
		
		temp[i] = &cbObjects[i];
	}
	

	if (createFences)
	{
		EntryHandle* fencesidx = CreateFences(numberOfCommandBuffers, VK_FENCE_CREATE_SIGNALED_BIT);

		if (!fencesidx)
		{
			vkFreeCommandBuffers(device, pool, numberOfCommandBuffers, l);
			return nullptr;
		}

		for (uint32_t i = 0; i < numberOfCommandBuffers; i++) 
		{
			temp[i]->fenceIdx = fencesidx[i];
		}
	}

	return intHandles;
}

EntryHandle VKDevice::CreateImageHandle(
	uint32_t blobSize,
	uint32_t width, uint32_t height, uint32_t layers,
	uint32_t mipLevels, VkFormat imageFormat,
	EntryHandle memIndex,
	VkImageAspectFlags flags,
	VkImageType imageType
)
{
	VKTexture* tex = reinterpret_cast<VKTexture*>(AllocFromPerDeviceData(sizeof(VKTexture)));

	if (!tex)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(blobSize);

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, imageFormat, layers,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, 0, imageType, memIndex);

	if (imageIndex == EntryHandle())
	{
		return imageIndex;
	}
	
	VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D;

	switch (imageType)
	{
	case VK_IMAGE_TYPE_2D:
		break;
	case VK_IMAGE_TYPE_3D:
		imageViewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	}

	EntryHandle viewIndex = CreateImageView(imageIndex, mipLevels, layers, imageFormat, flags, imageViewType);

	if (viewIndex == EntryHandle())
	{
		DestroyImage(imageIndex);
		return viewIndex;
	}

	tex = std::construct_at(tex, imageIndex, &viewIndex, 1, nullptr, 0);

	EntryHandle texImageHandle = AddVkTypeToEntry(tex, VulkTextureHandle);

	if (texImageHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		DestroyImageView(viewIndex);
		DestroyImage(imageIndex);
	}

	return texImageHandle;
}

EntryHandle VKDevice::CreateCubeMapedImageHandle(
	uint32_t blobSize,
	uint32_t width, uint32_t height, uint32_t layers,
	uint32_t mipLevels, VkFormat imageFormat,
	EntryHandle memIndex,
	VkImageAspectFlags flags
)
{
	VKTexture* tex = reinterpret_cast<VKTexture*>(AllocFromPerDeviceData(sizeof(VKTexture)));

	if (!tex)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(blobSize);

	EntryHandle imageIndex = CreateImage(
		width, height, mipLevels, imageFormat, layers,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT,
		1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_TYPE_2D, memIndex);

	if (imageIndex == EntryHandle())
	{
		return imageIndex;
	}

	VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;

	EntryHandle viewIndex = CreateImageView(imageIndex, mipLevels, layers, imageFormat, flags, imageViewType);

	if (viewIndex == EntryHandle())
	{
		DestroyImage(imageIndex);
		return viewIndex;
	}

	tex = std::construct_at(tex, imageIndex, &viewIndex, 1, nullptr, 0);

	EntryHandle texImageHandle = AddVkTypeToEntry(tex, VulkTextureHandle);

	if (texImageHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		DestroyImageView(viewIndex);
		DestroyImage(imageIndex);
	}

	return texImageHandle;

}

EntryHandle VKDevice::CreateSampler(uint32_t mipLevels)
{
	VkSampler sampler = VK_NULL_HANDLE;

	VkResult vkRes = VK_SUCCESS;

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

	if ((vkRes = vkCreateSampler(device, &samplerInfo, nullptr, &sampler)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SAMPLER_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	EntryHandle samplerHandle = AddVkTypeToEntry(sampler, VulkImageSampler);

	if (samplerHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
		vkDestroySampler(device, sampler, nullptr);
	}

	return samplerHandle;
}

EntryHandle* VKDevice::CreateSemaphores(uint32_t count)
{
	VkResult vkRes = VK_SUCCESS;

	EntryHandle* semaphoreHandles = reinterpret_cast<EntryHandle*>(AllocFromDeviceCache(sizeof(EntryHandle) * count));

	if (!semaphoreHandles)
	{
		AddDeviceErrorCode(DEVICE_CACHE_ALLOC_TOO_LARGE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	VkSemaphore vkSemaHandle = nullptr;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (uint32_t i = 0; i < count; i++)
	{
		if ((vkRes = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &vkSemaHandle)) != VK_SUCCESS) 
		{
			AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SEMAPHORE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
			return nullptr;
		}

		semaphoreHandles[i] = AddVkTypeToEntry(vkSemaHandle, VulkSemaphores);

		if (semaphoreHandles[i] == EntryHandle())
		{
			AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
			for (uint32_t j = 0; j <= i; j++)
			{
				vkDestroySemaphore(device, vkSemaHandle, nullptr);
			}
			return nullptr;
		}
	}

	return semaphoreHandles;
}

EntryHandle VKDevice::CreateShader(char* data, size_t dataSize, VkShaderStageFlags flags)
{
	VkShaderModule mod = VK_NULL_HANDLE;
	
	VkResult vkRes = VK_SUCCESS;

	ShaderHandle* modHandle = reinterpret_cast<ShaderHandle*>(AllocFromPerDeviceData(sizeof(ShaderHandle)));

	if (!modHandle)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	int retCode = VK::Utils::CreateShaderModule(device, data, dataSize, &mod, &vkRes);

	if (retCode < 0)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SHADER_MODULE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return EntryHandle();
	}

	modHandle->sMod = mod;

	modHandle->flags = flags;

	EntryHandle shaderModHandle = AddVkTypeToEntry(modHandle, VulkShaderHandle);

	if (shaderModHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}

	return shaderModHandle;
}

EntryHandle VKDevice::CreateSwapChain(uint32_t requestedImageCount, uint32_t maxFramesInFlight, VkFormat requestedFormat, EntryHandle renderSurfaceIndex)
{
	VKSwapChain* swc = reinterpret_cast<VKSwapChain*>(AllocFromPerDeviceData(sizeof(VKSwapChain)));

	if (!swc)
	{
		AddDeviceErrorCode(DEVICE_STORAGE_EXHAUSTED, VK_RESULT_MAX_ENUM);
		return EntryHandle();
	}

	auto swcsupport = parentInstance->GetSwapChainSupport(gpu, renderSurfaceIndex);

	std::construct_at(swc, this, parentInstance->GetRenderSurface(renderSurfaceIndex), &deviceDataAlloc, requestedImageCount, maxFramesInFlight, swcsupport, requestedFormat);

	EntryHandle swcHandle = AddVkTypeToEntry(swc, VulkSwapChain);

	if (swcHandle == EntryHandle())
	{
		AddDeviceErrorCode(DEVICE_HANDLE_ENTRIES_EXHAUSTION, VK_RESULT_MAX_ENUM);
	}

	return swcHandle;
}


//DESTRUCTORS

void VKDevice::DestroyBuffer(EntryHandle handle)
{
	BufferAlloc* alloc = GetBufferAlloc(handle);

	if (!alloc) 
		return;
	
	VkBuffer deviceBuffer = alloc->buffer;
	VkDeviceMemory deviceMem = alloc->memory;
	
	vkDestroyBuffer(device, deviceBuffer, nullptr);
	vkFreeMemory(device, deviceMem, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyBufferView(EntryHandle handle)
{
	BufferView* viewHandles = GetBufferViewContainer(handle);

	if (!viewHandles)
		return;

	for (uint32_t i = 0; i < viewHandles->count; i++)
	{
		vkDestroyBufferView(device, viewHandles->views[i], nullptr);
	}

	ReturnHandleObject(handle);
}

void VKDevice::DestroyTexelBufferView(EntryHandle handle)
{
	TexelBufferView* tbv = GetTexelBufferView(handle);

	if (!tbv) 
		return;

	DestroyBuffer(tbv->bufferHandle);
	DestroyBufferView(tbv->viewHandle);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyCommandBuffer(EntryHandle handle)
{
	VKCommandBuffer* buff = GetCommandBuffer(handle);

	if (!buff) 
		return;
	
	if (buff->fenceIdx != EntryHandle()) 
	{
		DestroyFence(buff->fenceIdx);
	}
	
	ReturnHandleObject(handle);
}

void VKDevice::DestroyCommandPool(EntryHandle handle)
{
	VkCommandPool pool = GetCommandPool(handle);

	if (!pool) 
		return;

	vkDestroyCommandPool(device, pool, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyFence(EntryHandle handle)
{
	VkFence fence = GetFence(handle);

	if (!fence) 
		return;

	vkDestroyFence(device, fence, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyFrameBuffer(EntryHandle handle)
{
	VkFramebuffer fb = GetFrameBuffer(handle);

	if (!fb) 
		return;

	vkDestroyFramebuffer(device, fb, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyDescriptorPool(EntryHandle handle)
{
	VkDescriptorPool pool = GetDescriptorPool(handle);

	if (!pool) 
		return;

	vkDestroyDescriptorPool(device, pool, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyDescriptorLayout(EntryHandle handle)
{
	VkDescriptorSetLayout lay = GetDescriptorSetLayout(handle);

	if (!lay) 
		return;
	
	vkDestroyDescriptorSetLayout(device, lay, nullptr);
	
	ReturnHandleObject(handle);
}

void VKDevice::DestroyDevice()
{
	vkDeviceWaitIdle(device);

	for (size_t i = 0; i < indexForEntries; i++)
	{
		EntryHandle handle(i);

		HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

		if (!objHandle.memoryLocation || objHandle.type == VulkMaxEnum)
			continue;

		switch (objHandle.type)
		{
		case VulkBuffer:
			DestroyBuffer(handle);
			break;

		case VulkImageHandle:
			DestroyImage(handle);
			break;

		case VulkImageView:
			DestroyImageView(handle);
			break;

		case VulkImageSampler:
			DestroySampler(handle);
			break;

		case VulkTextureHandle:
			DestroyTexture(handle);
			break;

		case VulkBufferView:
			 DestroyBufferView(handle);
			break;

		case VulkImageBufferView:
			DestroyTexelBufferView(handle);
			break;

		case VulkCommandPool:
			DestroyCommandPool(handle);
			break;

		case VulkComputeGraph:
			//DestroyComputeGraph(handle);
			break;

		case VulkDescriptorPool:
		     DestroyDescriptorPool(handle);
			break;

		case VulkDescriptorSet:
			break;

		case VulkDescriptorLayout:
			 DestroyDescriptorLayout(handle);
			break;

		case VulkFence:
			 DestroyFence(handle);
			break;

		case VulkFrameBuffer:
			DestroyFrameBuffer(handle);
			break;

		case VulkImageMemoryPool:
		    DestroyImagePool(handle);
			break;

		case VulkPipelineCacheObject:
			DestroyPipelineCacheObject(handle);
			break;

		case VulkComputePipeline:
			// DestroyComputePipeline(handle);
			break;

		case VulkGraphicsPipeline:
			// DestroyGraphicsPipeline(handle);
			break;

		case VulkMemoryBarrier:
			// DestroyMemoryBarrier(handle);
			break;

		case VulkBufferMemoryBarrier:
			// DestroyBufferMemoryBarrier(handle);
			break;

		case VulkImageMemoryBarrier:
			// DestroyImageMemoryBarrier(handle);
			break;

		case VulkRenderTargetData:
			// DestroyRenderTargetData(handle);
			break;

		case VulkRenderPassHandle:
			 DestroyRenderPass(handle);
			break;

		case VulkRenderTarget:
			 DestroyRenderTarget(handle);
			break;

		case VulkVKCommandBuffer:
			DestroyCommandBuffer(handle);
			break;

		case VulkSemaphores:
			DestroySemaphore(handle);
			break;

		case VulkShaderHandle:
			 DestroyShader(handle);
			break;

		case VulkSwapChain:
			 DestroySwapChain(handle);
			break;

		case VulkQueueManager:
			// DestroyQueueManager(handle);
			break;

		case VulkQueryPool:
			DestoryQueryPool(handle);
			break;
		case VulkMaxEnum:
		default:
			// Invalid handle type
			break;
		}
	}

	vkDestroyDevice(device, nullptr);
}

void VKDevice::DestroyImage(EntryHandle handle)
{
	ImageAllocation* image = GetImageAllocation(handle);

	if (!image)
		return;

	vkDestroyImage(device, image->imageHandle, nullptr);

	ImageMemoryPool* memPool = GetImageMemoryPool(image->memIndex);

	if (memPool)
		memPool->alloc.FreeMemory(image->deviceMemoryAddress);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyImagePool(EntryHandle handle)
{
	ImageMemoryPool* pool = GetImageMemoryPool(handle);

	if (!pool)
		return;

	vkFreeMemory(device, pool->memory, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyRenderPass(EntryHandle handle)
{
	VkRenderPass pass = GetRenderPass(handle);

	if (!pass) 
		return;

	vkDestroyRenderPass(device, pass, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyRenderTarget(EntryHandle handle)
{
	RenderTarget* target = GetRenderTarget(handle);

	for (uint32_t j = 0; j < target->count; j++)
	{
		DestroyFrameBuffer(target->framebufferIndices[j]);
	}

	ReturnHandleObject(handle);
}

void VKDevice::DestoryQueryPool(EntryHandle handle)
{
	VkQueryPool pool = GetQueryPool(handle);

	if (!pool)
		return;

	vkDestroyQueryPool(device, pool, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyImageView(EntryHandle handle)
{
	VkImageView view = GetImageViewByHandle(handle);

	if (!view)
		return;

	vkDestroyImageView(device, view, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyPipelineCacheObject(EntryHandle handle)
{
	PipelineCacheObject* obj = GetPipelineCacheObject(handle);

	if (!obj) 
		return;

	vkDestroyPipeline(device, obj->pipeline, nullptr);

	vkDestroyPipelineLayout(device, obj->pipelineLayout, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroySampler(EntryHandle handle)
{
	VkSampler sampler = GetSamplerByHandle(handle);

	if (!sampler)
		return;

	vkDestroySampler(device, sampler, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroySemaphore(EntryHandle handle)
{
	VkSemaphore sema = GetSemaphore(handle);

	if (!sema) 
		return;

	vkDestroySemaphore(device, sema, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroyShader(EntryHandle handle)
{
	ShaderHandle* shader = GetShader(handle);

	if (!shader) 
		return;

	vkDestroyShaderModule(device, shader->sMod, nullptr);

	ReturnHandleObject(handle);
}

void VKDevice::DestroySwapChain(EntryHandle handle)
{
	VKSwapChain* swc = GetSwapChain(handle);

	swc->DestroySwapChain();

	ReturnHandleObject(handle);
}

void VKDevice::DestroyTexture(EntryHandle handle)
{
	VKTexture* tex = GetTexture(handle);

	for (int i = 0; tex->samplerIndex[i] != EntryHandle(); i++)
		DestroySampler(tex->samplerIndex[i]);

	for (int i = 0; tex->viewIndex[i] != EntryHandle(); i++)
		DestroyImageView(tex->viewIndex[i]);

	DestroyImage(tex->imageIndex);
	
	ReturnHandleObject(handle);
}


//GETTERS

BufferAlloc* VKDevice::GetBufferAlloc(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkBuffer || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_ALLOC_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(objHandle.memoryLocation);

	return alloc;
}

VkBuffer VKDevice::GetBufferHandle(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkBuffer || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	BufferAlloc* alloc = reinterpret_cast<BufferAlloc*>(objHandle.memoryLocation);

	return alloc->buffer;
}

VkBufferView VKDevice::GetBufferView(EntryHandle handle, uint32_t index)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkBufferView || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_VIEW_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	BufferView* viewHandles = reinterpret_cast<BufferView*>(objHandle.memoryLocation);

	return viewHandles->views[index];
}

BufferView* VKDevice::GetBufferViewContainer(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkBufferView || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_BUFFER_VIEW_CONTAINER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	BufferView* viewHandles = reinterpret_cast<BufferView*>(objHandle.memoryLocation);

	return viewHandles;
}

VKCommandBuffer* VKDevice::GetCommandBuffer(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkVKCommandBuffer || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_COMMAND_BUFFER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VKCommandBuffer*>(objHandle.memoryLocation);
}

VkCommandBuffer VKDevice::GetCommandBufferHandle(EntryHandle handle)
{
	VKCommandBuffer* cbContainer = GetCommandBuffer(handle);
	
	if (!cbContainer)
		return VK_NULL_HANDLE;
	
	return cbContainer->buffer;
}

VkCommandPool VKDevice::GetCommandPool(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkCommandPool || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_COMMAND_POOL_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkCommandPool>(objHandle.memoryLocation);
}

VkDescriptorPool VKDevice::GetDescriptorPool(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkDescriptorPool || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_POOL_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkDescriptorPool>(objHandle.memoryLocation);
}

VkDescriptorSet VKDevice::GetDescriptorSet(EntryHandle handle, uint32_t index)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkDescriptorSet || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_SET_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	DescriptorSetAlloc* set = reinterpret_cast<DescriptorSetAlloc*>(objHandle.memoryLocation);

	if (set->numberOfSets <= index) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_SET_FAILED) | DEVICE_VK_TYPE_INDEX_OUT_OF_BOUNDS, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return set->sets[index];

}

VkDescriptorSet* VKDevice::GetDescriptorSets(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkDescriptorSet || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_SET_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	DescriptorSetAlloc* set = reinterpret_cast<DescriptorSetAlloc*>(objHandle.memoryLocation);

	return set->sets;
}

VkDescriptorSetLayout VKDevice::GetDescriptorSetLayout(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkDescriptorLayout || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_DESCRIPTOR_LAYOUT_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkDescriptorSetLayout>(objHandle.memoryLocation);
}

VkFence VKDevice::GetFence(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkFence || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_FENCE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkFence>(objHandle.memoryLocation);
}

VkFramebuffer VKDevice::GetFrameBuffer(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkFrameBuffer || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_FRAMEBUFFER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkFramebuffer>(objHandle.memoryLocation);
}

ImageAllocation* VKDevice::GetImageAllocation(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageHandle || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_HANDLE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	ImageAllocation* image = reinterpret_cast<ImageAllocation*>(objHandle.memoryLocation);

	return image;
}

VkImage VKDevice::GetImageByHandle(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageHandle || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_HANDLE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	ImageAllocation* imageAllocation = reinterpret_cast<ImageAllocation*>(objHandle.memoryLocation);

	return imageAllocation->imageHandle;
}

VkImage VKDevice::GetImageByTexture(EntryHandle handle)
{
	VKTexture* tex = GetTexture(handle);
	
	if (!tex) 
		return VK_NULL_HANDLE;

	return GetImageByHandle(tex->imageIndex);
}

ImageMemoryPool* VKDevice::GetImageMemoryPool(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageMemoryPool || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_MEMORY_POOL_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	ImageMemoryPool* pool = reinterpret_cast<ImageMemoryPool*>(objHandle.memoryLocation);

	return pool;
}

VkImageView VKDevice::GetImageViewByHandle(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageView || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_VIEW_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkImageView>(objHandle.memoryLocation);
}

VkImageView VKDevice::GetImageViewByTexture(EntryHandle handle, int imageViewIndex)
{
	VKTexture* tex = GetTexture(handle);

	if (!tex) 
		return VK_NULL_HANDLE;

	return GetImageViewByHandle(tex->viewIndex[imageViewIndex]);
}

PipelineCacheObject* VKDevice::GetPipelineCacheObject(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkPipelineCacheObject || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_PIPELINE_CACHE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	return reinterpret_cast<PipelineCacheObject*>(objHandle.memoryLocation);
}

int VKDevice::GetPresentQueue(
	QueueIndex* queueIdx,
	uint32_t* maxQueueCount,
	VkSurfaceKHR renderSurface,
	VkQueueFamilyProperties* famProps)
{
	uint32_t famPropsCount = 0;
	QueueFamilyDetails(famProps, &famPropsCount);

	for (uint32_t i = 0; i<famPropsCount; i++)
	{
		VkQueueFamilyProperties *props = &famProps[i];
		VkBool32 presentSupport = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, renderSurface, &presentSupport);

		if (presentSupport)
		{
			*maxQueueCount = props->queueCount;
			*queueIdx = QueueIndex(i);
			return 0;
		}
	}

	return -1;
}

int VKDevice::GetQueueByMask(
	QueueIndex* queueIdx,
	uint32_t* maxQueueCount,
	uint32_t queueMask,
	VkQueueFamilyProperties* famProps)
{
	uint32_t famPropsCount = 0;
	QueueFamilyDetails(famProps, &famPropsCount);

	for (uint32_t i = 0; i < famPropsCount; i++)
	{
		VkQueueFamilyProperties* props = &famProps[i];
		if ((props->queueFlags & queueMask) == queueMask) 
		{
			*queueIdx = QueueIndex(i);
			*maxQueueCount = props->queueCount;
			return 0;
		}
	}

	return -1;
}

VkQueryPool VKDevice::GetQueryPool(EntryHandle poolHandle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(poolHandle);

	if (objHandle.type != VulkQueryPool || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_QUERY_POOL_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	VkQueryPool queryPool = reinterpret_cast<VkQueryPool>(objHandle.memoryLocation);

	return queryPool;
}

VKDevice::QueueDetails VKDevice::GetQueueHandle(EntryHandle queueManagerIndex)
{
	QueueManager* manager = GetQueueManager(queueManagerIndex);

	if (!manager)
	{
		return { ~0ui32, ~0ui32, EntryHandle() };
	}

	uint32_t queueIdx = manager->GetQueue();

	return { queueIdx,  manager->queueFamilyIndex,  manager->poolIndices[queueIdx] };
}

QueueManager* VKDevice::GetQueueManager(EntryHandle queueManagerIndex)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(queueManagerIndex);

	if (objHandle.type != VulkQueueManager || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_QUEUE_MANAGER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	QueueManager* queueManager = reinterpret_cast<QueueManager*>(objHandle.memoryLocation);

	return queueManager;
}

RecordingBufferObject VKDevice::GetRecordingBufferObject(EntryHandle handle)
{
	VKCommandBuffer* buffer = GetCommandBuffer(handle);

	return { this, *buffer };
}


VkRenderPass VKDevice::GetRenderPass(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkRenderPassHandle || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_RENDER_PASS_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	auto renderPass = reinterpret_cast<VkRenderPass>(objHandle.memoryLocation);

	return renderPass;
}

RenderTarget* VKDevice::GetRenderTarget(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkRenderTarget || !objHandle.memoryLocation)
		return nullptr;

	auto data = reinterpret_cast<RenderTarget*>(objHandle.memoryLocation);

	return data;
}

VkSampler VKDevice::GetSamplerByHandle(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageSampler || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SAMPLER_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkSampler>(objHandle.memoryLocation);
}

VkSampler VKDevice::GetSamplerByTexture(EntryHandle handle, int samplerIndex)
{
	VKTexture* tex = GetTexture(handle);

	if (!tex) 
		return VK_NULL_HANDLE;

	return GetSamplerByHandle(tex->samplerIndex[samplerIndex]);
}

VkSemaphore VKDevice::GetSemaphore(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkSemaphores || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SEMAPHORE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VkSemaphore>(objHandle.memoryLocation);
}

ShaderHandle* VKDevice::GetShader(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkShaderHandle || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_SHADER_MODULE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}
	
	return reinterpret_cast<ShaderHandle*>(objHandle.memoryLocation);
}

VKSwapChain* VKDevice::GetSwapChain(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkSwapChain || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_SWAPCHAIN_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return VK_NULL_HANDLE;
	}

	return reinterpret_cast<VKSwapChain*>(objHandle.memoryLocation);
}

TexelBufferView* VKDevice::GetTexelBufferView(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkImageBufferView || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_TEXEL_BUFFER_VIEW_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	TexelBufferView* alloc = reinterpret_cast<TexelBufferView*>(objHandle.memoryLocation);

	return alloc;
}

VKTexture* VKDevice::GetTexture(EntryHandle handle)
{
	HandlePoolObject objHandle = GetVkTypeFromEntry(handle);

	if (objHandle.type != VulkTextureHandle || !objHandle.memoryLocation)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_TEXTURE_FAILED) | DEVICE_VK_TYPE_INCORRECT_TYPE_ON_RETRIEVE, VK_RESULT_MAX_ENUM);
		return nullptr;
	}

	return reinterpret_cast<VKTexture*>(objHandle.memoryLocation);
}

//ACTIONS


int VKDevice::AssignSamplerToTexture(EntryHandle textureIndex, EntryHandle samplerIndex)
{
	VKTexture* tex = GetTexture(textureIndex);

	if (!tex) 
		return -1;

	tex->samplerIndex[0] = samplerIndex;

	return 0;
}

uint32_t VKDevice::BeginFrameForSwapchain(EntryHandle swapChainIndex, EntryHandle acquireSemaphoreHandle, uint32_t currentFrame)
{
	VKSwapChain* swapChain = GetSwapChain(swapChainIndex);

	VkSemaphore acquireSemaphore = GetSemaphore(acquireSemaphoreHandle);

	if (!swapChain || !acquireSemaphore)
		return ~0ui32;

	uint32_t imageIndex = swapChain->AcquireNextSwapChainImage2(UINT64_MAX, acquireSemaphore, currentFrame);

	return imageIndex;
}

VkShaderStageFlagBits VKDevice::ConvertShaderFlags(const char* filename, int nameLength)
{
	int offset = 0;

	if (strncmp(&filename[nameLength  - 4], ".spv", 4) == 0)
	{
		offset = 4;
	}

	if (strncmp(&filename[nameLength - offset - 4], "frag", 4) == 0)
	{
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	else if (strncmp(&filename[nameLength - offset - 4], "vert", 4) == 0)
	{
		return VK_SHADER_STAGE_VERTEX_BIT;
	}
	else if (strncmp(&filename[nameLength - offset - 4], "comp", 4) == 0)
	{
		return VK_SHADER_STAGE_COMPUTE_BIT;
	}
	else
	{
		AddDeviceErrorCode(DEVICE_VK_TYPE_SHADER_CONVERT_FLAGS_FAILED, VK_RESULT_MAX_ENUM);
	}

	return VK_SHADER_STAGE_ALL;
}

std::pair<uint32_t, VkDeviceSize> VKDevice::FindImageMemoryIndexForPool(uint32_t width,
	uint32_t height, uint32_t mipLevels,
	VkFormat type, uint32_t layers,
	VkImageUsageFlags flags, uint32_t sampleCount,
	VkMemoryPropertyFlags memProps)
{
	VkResult vkRes = VK_SUCCESS;
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

	if ((vkRes = vkCreateImage(device, &imageInfo, nullptr, &image)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_IMAGE_HANDLE_FAILED) | DEVICE_VK_TYPE_CREATION_FAILED, vkRes);
		return { 0, 0 };
	}

	VkMemoryRequirements memRequirements;
	
	vkGetImageMemoryRequirements(device, image, &memRequirements);
	
	uint32_t memoryTypeIndex = VK::Utils::findMemoryType(gpu, memRequirements.memoryTypeBits, memProps);

	vkDestroyImage(device, image, nullptr);

	return std::make_pair(memoryTypeIndex, memRequirements.alignment);
}

size_t VKDevice::GetMemoryFromBuffer(EntryHandle hostIndex, size_t size, uint32_t alignment)
{
	BufferAlloc* alloc = GetBufferAlloc(hostIndex);

	if (!alloc)
		return ~0ull;

	return alloc->alloc.GetMemory(size, alignment);
}

int VKDevice::PresentSwapChainCommandBufferInline(EntryHandle swapChainIdx, EntryHandle* presentWaitSemaphores, uint32_t presentWaitSemaphoreCount, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle commandBufferIndex)
{
	VkQueue queue;
	uint32_t  queueIndex = ~0, queueFamilyIndex = ~0;
	
	VKCommandBuffer* rbo = GetCommandBuffer(commandBufferIndex);
	queueIndex = rbo->queueIndex;
	queueFamilyIndex = rbo->queueFamilyIndex;
	
	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VKSwapChain* swc = GetSwapChain(swapChainIdx);
	
	VkSemaphore* waitSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * presentWaitSemaphoreCount));

	for (uint32_t i = 0; i < presentWaitSemaphoreCount; i++)
	{
		waitSemaphores[i] = GetSemaphore(presentWaitSemaphores[i]);
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = presentWaitSemaphoreCount;
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

	int retCode = 0;

	if (result != VK_SUCCESS) 
	{
		AddDeviceErrorCode(DEVICE_VK_TYPE_SWC_PRESENT_FAILED, result);
		retCode = -1;
	}

	return retCode;
}

int VKDevice::PresentSwapChainSeparatePresentQueue(EntryHandle swapChainIdx, EntryHandle* presentWaitSemaphores, uint32_t presentWaitSemaphoreCount, uint32_t imageIndex, uint32_t frameInFlight, EntryHandle queueManagerIndex)
{
	VkQueue queue;
	uint32_t queueIndex = ~0, queueFamilyIndex = ~0;
	
	VKDevice::QueueDetails queueDetails = GetQueueHandle(queueManagerIndex);
	queueIndex = queueDetails.queueIndex;
	queueFamilyIndex = queueDetails.queueFamilyIndex;

	vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &queue);

	VKSwapChain* swc = GetSwapChain(swapChainIdx);

	VkSemaphore* waitSemaphores = reinterpret_cast<VkSemaphore*>(AllocFromDeviceCache(sizeof(VkSemaphore) * presentWaitSemaphoreCount));

	for (uint32_t i = 0; i < presentWaitSemaphoreCount; i++)
	{
		waitSemaphores[i] = GetSemaphore(presentWaitSemaphores[i]);
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = presentWaitSemaphoreCount;
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

	int retCode = 0;

	if (result != VK_SUCCESS)
	{
		AddDeviceErrorCode(DEVICE_VK_TYPE_SWC_PRESENT_FAILED, result);
		retCode = -1;
	}

	ReturnQueueToManager(queueManagerIndex, queueIndex);

	return retCode;
}

void VKDevice::QueueFamilyDetails(VkQueueFamilyProperties* famProps, uint32_t* size)
{
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, size, nullptr);

	vkGetPhysicalDeviceQueueFamilyProperties(gpu, size, famProps);

	return;
}

uint32_t VKDevice::QueueFamilyDetailsCount()
{
	uint32_t totalCount = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &totalCount, nullptr);

	return totalCount;
}

int VKDevice::ReturnQueueToManager(EntryHandle queueManagerIndex, uint32_t queueIndex)
{
	QueueManager* manager = GetQueueManager(queueManagerIndex);

	if (!manager)
		return -1;

	manager->ReturnQueue(queueIndex);

	return 0;
}

int VKDevice::SubmitCommandBuffer(
	EntryHandle* wait,
	VkPipelineStageFlags* waitStages,
	EntryHandle* signal,
	uint32_t waitCount,
	uint32_t signalCount,
	EntryHandle cbIndex)
{
	
	VKCommandBuffer* vkcb = GetCommandBuffer(cbIndex);
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

	VkResult vkRes = VK_SUCCESS;

	int retCode = 0;

	if ((vkRes = vkQueueSubmit(queue, 1, &submitInfo, fence)) != VK_SUCCESS) 
	{
		AddDeviceErrorCode(DEVICE_VK_TYPE_COMMNAD_BUFFER_SUBMIT_FAILED, vkRes);
		retCode = -1;
	}

	return retCode;
}


int VKDevice::TransitionImageLayout(EntryHandle imageIndex,
	VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
	uint32_t mips, uint32_t layers, EntryHandle queueManagerIndex)
{
	
	VkImage image = GetImageByHandle(imageIndex);

	if (!image)
		return -1;

	VKDevice::QueueDetails queueDetails = GetQueueHandle(queueManagerIndex);

	VkCommandPool pool = GetCommandPool(queueDetails.poolIndex);
	
	VkQueue queue = VK_NULL_HANDLE;
	
	vkGetDeviceQueue(device, queueDetails.queueFamilyIndex, queueDetails.queueIndex, &queue);

	VkCommandBuffer commandBuffer = VK::Utils::BeginOneTimeCommands(device, pool);
	
	VK::Utils::MultiCommands::TransitionImageLayout(
		commandBuffer,
		image, format,
		oldLayout, newLayout,
		mips, layers
	);

	VK::Utils::EndOneTimeCommands(device, queue, pool, commandBuffer);

	ReturnQueueToManager(queueManagerIndex, queueDetails.queueIndex);

	return 0;
}

int VKDevice::CommandBufferWaitOn(uint64_t timeout, EntryHandle bufferIndex)
{
	VKCommandBuffer* vkcb = GetCommandBuffer(bufferIndex);

	if (vkcb->fenceIdx == ~0ui64)
		return 0;

	VkFence fence = GetFence(vkcb->fenceIdx);

	VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);

	if (res == VK_TIMEOUT)
		return -1;

	return 0;
}

int VKDevice::ResetRenderTarget(EntryHandle handle)
{
	RenderTarget* target = GetRenderTarget(handle);

	if (!target)
		return -1;

	for (uint32_t j = 0; j < target->count; j++)
	{
		DestroyFrameBuffer(target->framebufferIndices[j]);
	}

	return 0;
}

int VKDevice::CommandBufferResetFence(EntryHandle bufferIndex)
{
	VKCommandBuffer* vkcb = GetCommandBuffer(bufferIndex);

	VkFence fence = GetFence(vkcb->fenceIdx);

	if (!fence)
		return -1;

	vkResetFences(device, 1, &fence);

	return 0;
}


void VKDevice::WaitOnDevice()
{
	vkDeviceWaitIdle(device);
}

int VKDevice::WriteToHostBuffer(EntryHandle hostIndex, void* data, size_t size, size_t offset, int copies, size_t stride)
{
	BufferAlloc* deviceMem = GetBufferAlloc(hostIndex);

	if (!deviceMem)
		return -1;

	void* datalocal;
	vkMapMemory(device, deviceMem->memory, offset, size, 0, reinterpret_cast<void**>(&datalocal));
	uintptr_t iter = (uintptr_t)datalocal;
	for (int i = 0; i < copies; i++)
	{
		std::memcpy((void*)iter, data, size);
		iter += stride;
	}
	vkUnmapMemory(device, deviceMem->memory);

	return 0;
}


int VKDevice::WriteToHostBufferBatch(EntryHandle hostIndex, void** dataPoints, size_t* sizes, size_t* offsets, size_t range, size_t minOffset, size_t numCopies)
{
	BufferAlloc* deviceMem = GetBufferAlloc(hostIndex);

	if (!deviceMem)
		return -1;

	void* datalocal;
	vkMapMemory(device, deviceMem->memory, minOffset, range, 0, reinterpret_cast<void**>(&datalocal));
	uintptr_t iter = (uintptr_t)datalocal;
	for (int i = 0; i < numCopies; i++)
	{
		std::memcpy((void*)(iter+offsets[i]), dataPoints[i], sizes[i]);
	}
	vkUnmapMemory(device, deviceMem->memory);

	return 0;
}

int VKDevice::ReadHostBuffer(void* dest, EntryHandle hostIndex, size_t size, size_t offset)
{
	BufferAlloc* deviceMem = GetBufferAlloc(hostIndex);

	if (!deviceMem)
		return -1;

	void* datalocal;
	vkMapMemory(device, deviceMem->memory, offset, size, 0, &datalocal);

	std::memcpy(dest, datalocal, size);
	
	vkUnmapMemory(device, deviceMem->memory);

	return 0;
}

int VKDevice::WriteToDeviceBuffer(
	EntryHandle deviceIndex, 
	EntryHandle stagingBufferIndex, 
	void* data, 
	size_t size, size_t offset, 
	int copies, size_t stride,
	EntryHandle queueManagerIndex
)
{
	BufferAlloc* stagingBufferAlloc = GetBufferAlloc(deviceIndex);

	if (!stagingBufferAlloc)
		return -1;

	VKDevice::QueueDetails queueDetails = GetQueueHandle(queueManagerIndex);

	VkQueue queue;
	vkGetDeviceQueue(device, queueDetails.queueFamilyIndex, queueDetails.queueIndex, &queue);

	VkBuffer stagingBuffer = stagingBufferAlloc->buffer;
	VkDeviceMemory stagingMemory = stagingBufferAlloc->memory;
	VkDeviceSize allocOffset = stagingBufferAlloc->alloc.GetMemory(size, 64);

	void* ldata;
	
	vkMapMemory(device, stagingMemory, allocOffset, size, 0, &ldata);
	
	std::memcpy(ldata, data, size);
		
	vkUnmapMemory(device, stagingMemory);

	VkCommandPool pool = GetCommandPool(queueDetails.poolIndex);

	VkBuffer outBuffer = GetBufferHandle(deviceIndex);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::CopyBuffer(cb, stagingBuffer, outBuffer, size, allocOffset, offset, copies, stride);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(queueManagerIndex, queueDetails.queueIndex);

	stagingBufferAlloc->alloc.FreeMemory(allocOffset);

	return 0;
}


int VKDevice::WriteToDeviceBufferBatch(EntryHandle deviceIndex, EntryHandle stagingBufferIndex, void** data, size_t* sizes, size_t* offsets, size_t cumulativesize, int entries, RecordingBufferObject* rbo)
{
	VkBufferCopy* stagingoffset = (VkBufferCopy*)AllocFromDeviceCache(sizeof(VkBufferCopy) * entries);

	BufferAlloc* stagingBufferAlloc = GetBufferAlloc(stagingBufferIndex);

	if (!stagingBufferAlloc)
		return -1;

	VkBuffer outBuffer = GetBufferHandle(deviceIndex);

	if (!outBuffer)
		return -1;

	VkBuffer stagingBuffer = stagingBufferAlloc->buffer;
	VkDeviceMemory stagingMemory = stagingBufferAlloc->memory;

	VkDeviceSize allocOffset = stagingBufferAlloc->alloc.GetMemory(cumulativesize + (entries * 256), 256);

	char* ldata;

	vkMapMemory(device, stagingMemory, allocOffset, cumulativesize + (entries * 256), 0, reinterpret_cast<void**>(&ldata));

	size_t localOffset = 0;

	for (int i = 0; i < entries; i++)
	{
		std::memcpy(ldata + localOffset, data[i], sizes[i]);
		
		stagingoffset[i].srcOffset = allocOffset + localOffset;
		stagingoffset[i].dstOffset = offsets[i];
		stagingoffset[i].size = sizes[i];
		
		localOffset += sizes[i];

		localOffset = (localOffset + 255) & ~(255ui64);
	}
	
	vkUnmapMemory(device, stagingMemory);

	VK::Utils::MultiCommands::CopyBufferBatch(rbo->cbBufferHandler.buffer, stagingBuffer, outBuffer, stagingoffset, entries);

	return 0;
}

int VKDevice::UploadImageData(EntryHandle textureIndex, 
	char* imageData, size_t totalImageDataSize, EntryHandle stagingBufferIndex, 
	int width, int height, int layers,
	int mipLevels, VkFormat format, EntryHandle queueManagerIndex
)
{
	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(totalImageDataSize);

	BufferAlloc* stagingBufferAlloc = GetBufferAlloc(stagingBufferIndex);

	if (!stagingBufferAlloc)
		return -1;

	VkImage image = GetImageByTexture(textureIndex);

	if (!image)
		return -1;
		
	VkBuffer stagingBuffer = stagingBufferAlloc->buffer;
	VkDeviceMemory stagingMemory = stagingBufferAlloc->memory;
	VkDeviceSize offsetAlloc = stagingBufferAlloc->alloc.GetMemory(imagesSize, 4);

	VKDevice::QueueDetails queueDetails = GetQueueHandle(queueManagerIndex);

	VkCommandPool pool = GetCommandPool(queueDetails.poolIndex);

	VkQueue queue;
	vkGetDeviceQueue(device, queueDetails.queueFamilyIndex, queueDetails.queueIndex, &queue);

	char* data;
	auto& pixels = imageData;
	vkMapMemory(device, stagingMemory, offsetAlloc, imagesSize, 0, reinterpret_cast<void**>(&data));

	std::memcpy(data, pixels, imagesSize);

	vkUnmapMemory(device, stagingMemory);

	VkCommandBuffer cb = VK::Utils::BeginOneTimeCommands(device, pool);

	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);

	VkDeviceSize offset = 0UL;

	for (auto i = 0U; i < mipLevels; i++) 
	{
		VkDeviceSize individualSize = VK::Utils::GetRawImageSizeFromFormat(format, width >> i, height >> i);

		VK::Utils::MultiCommands::CopyBufferToImage(cb, stagingBuffer, image, width >> i, height >> i, i, offset, { 0, 0, 0 }, 0, layers);

		offset += individualSize;
	}
	
	VK::Utils::MultiCommands::TransitionImageLayout(cb, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, 1);

	VK::Utils::EndOneTimeCommands(device, queue, pool, cb);

	ReturnQueueToManager(queueManagerIndex, queueDetails.queueIndex);

	stagingBufferAlloc->alloc.FreeMemory(offsetAlloc);

	return 0;
}

int VKDevice::UploadImageData(EntryHandle textureIndex,
	char* imageData, size_t totalImageDataSize,
	EntryHandle stagingBufferIndex,
	int width, int height,
	int mipLevels, int layers, VkFormat format, RecordingBufferObject* rbo
)
{
	VkDeviceSize imagesSize = static_cast<VkDeviceSize>(totalImageDataSize);

	BufferAlloc* stagingBufferAlloc = GetBufferAlloc(stagingBufferIndex);

	if (!stagingBufferAlloc)
		return -1;

	VkBuffer stagingBuffer = stagingBufferAlloc->buffer;
	VkDeviceMemory stagingMemory = stagingBufferAlloc->memory;
	VkDeviceSize offsetAlloc = stagingBufferAlloc->alloc.GetMemory(imagesSize, 16);

	char* data;
	auto& pixels = imageData;
	vkMapMemory(device, stagingMemory, offsetAlloc, imagesSize, 0, reinterpret_cast<void**>(&data));

	std::memcpy(data, pixels, imagesSize);

	vkUnmapMemory(device, stagingMemory);

	VkImage image = GetImageByTexture(textureIndex);

	VK::Utils::MultiCommands::TransitionImageLayout(rbo->cbBufferHandler.buffer, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, layers);

	VkDeviceSize offset = offsetAlloc;

	for (auto i = 0U; i < mipLevels; i++) 
	{
		VkDeviceSize individualSize = VK::Utils::GetRawImageSizeFromFormat(format, width >> i, height >> i);

		VK::Utils::MultiCommands::CopyBufferToImage(rbo->cbBufferHandler.buffer, stagingBuffer, image, width >> i, height >> i, i, offset, { 0, 0, 0 }, 0, layers);

		offset += individualSize;
	}
	
	VK::Utils::MultiCommands::TransitionImageLayout(rbo->cbBufferHandler.buffer, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels, layers);

	return 0;
}

int VKDevice::ResetBufferAllocator(EntryHandle bufferIndex)
{
	BufferAlloc* deviceMem = GetBufferAlloc(bufferIndex);

	if (!deviceMem)
		return -1;

	deviceMem->alloc.Reset();

	return 0;
}

VKMemoryAllocatorDetails VKDevice::GetMemoryAllocDetailsForBuffer(EntryHandle bufferHandle)
{
	BufferAlloc* deviceMem = GetBufferAlloc(bufferHandle);

	if (!deviceMem)
		return {0, 0};

	return deviceMem->alloc.GetMemoryAllocDetails();
}

VKMemoryAllocatorDetails VKDevice::GetMemoryAllocDetailsForImageMemory(EntryHandle poolHandle)
{
	ImageMemoryPool* pool = GetImageMemoryPool(poolHandle);

	if (!pool)
		return {0, 0};

	return pool->alloc.GetMemoryAllocDetails();
}

int VKDevice::ResetQueryPool(EntryHandle poolHandle, uint32_t firstQuery, uint32_t queryCount)
{
	VkQueryPool queryPool = GetQueryPool(poolHandle);

	if (!queryPool)
		return -1;

	vkResetQueryPool(
		device,
		queryPool,
		firstQuery,
		queryCount);

	return 0;
}

int VKDevice::ReadbackResultsFromQueries(EntryHandle poolIndex, uint32_t firstQuery, uint32_t queryCount, void* dataOut, size_t sizeOfDataOut, VkDeviceSize dataStride, VkQueryResultFlags flags)
{
	VkQueryPool queryPool = GetQueryPool(poolIndex);

	if (!queryPool)
		return -1;

	VkResult vkRes = VK_SUCCESS;

	int retCode = 0;

	if ((vkRes = vkGetQueryPoolResults(device, queryPool,
		firstQuery, queryCount,
		sizeOfDataOut, dataOut,
		dataStride, flags)) != VK_SUCCESS)
	{
		AddDeviceErrorCode(MINOR_CODE_PACK(DEVICE_VK_TYPE_QUERY_POOL_FAILED) | DEVICE_VK_TYPE_TIMED_RESULT_FAILED, vkRes);
		retCode = -1;
	}

	return retCode;
}

/*---------------------------------------------------------*/

QueueManager::QueueManager(
	uint32_t* _cqs, uint32_t _cqss,
	int32_t _mqc, uint32_t _qfi,
	uint32_t _queueCapabilities, bool present,
	VKDevice* _d, void* data) :
	bitmap(0U),
	maxQueueCount(_mqc),
	queueFamilyIndex(_qfi),
	queueCapabilities(ConvertQueueProps(_queueCapabilities, present)),
	device(_d),
	queueSema(),
	submitSema()
{

	assert(maxQueueCount <= 16);

	poolIndices = reinterpret_cast<EntryHandle*>(data);

	queueCountCV = _mqc;

	QueueIndex index = QueueIndex{ _qfi };

	for (int32_t i = 0; i<_mqc; i++)
	{
		poolIndices[i] = _d->CreateCommandPool(index);
	}

	for (size_t i = 0; i < _cqss; i++)
	{
		bitmap.set(_cqs[i]);
	}
}

uint32_t QueueManager::GetQueue()
{
	std::unique_lock guard(queueSema);
	
	queueCV.wait(guard, [this]() { return queueCountCV > 0; });
	
	for (int32_t i = 0; i < maxQueueCount; i++)
	{
		if (!bitmap.test(i))
		{
			bitmap.set(i);
			queueCountCV--;
			return i;
		}
	}

	return ~0ui32;
}

void QueueManager::ReturnQueue(size_t queueNum)
{
	{
		std::scoped_lock guard(queueSema);
		bitmap.reset(queueNum);
		queueCountCV++;
	}
	queueCV.notify_one();
}

uint32_t QueueManager::ConvertQueueProps(uint32_t flags, bool present)
{
	uint32_t flag = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) * GRAPHICSQUEUE;
	flag |= ((flags & VK_QUEUE_COMPUTE_BIT) != 0) * COMPUTEQUEUE;
	flag |= ((flags & VK_QUEUE_TRANSFER_BIT) != 0) * TRANSFERQUEUE;
	flag |= (present * PRESENTQUEUE);
	return flag;
}

void QueueManager::DestroyManager()
{
	for (int32_t i = 0; i < maxQueueCount; i++)
	{
		device->DestroyCommandPool(poolIndices[i]);
	}
}