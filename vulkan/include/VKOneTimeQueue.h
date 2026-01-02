#pragma once
#include "VKTypes.h"
#include "IndexTypes.h"
#include "VKUtilities.h"

struct VKOneTimeQueue
{

	VKOneTimeQueue() = default;
	VKOneTimeQueue(VKOneTimeQueue&& other) = delete;

	VKOneTimeQueue(const VKOneTimeQueue& other) = delete;
	VKOneTimeQueue& operator=(const VKOneTimeQueue& other) = delete;

	VKOneTimeQueue& operator=(VKOneTimeQueue&& other) = delete;

	VKOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d);

	uint32_t AddObject(EntryHandle obj);

	void UpdateQueue();

	EntryHandle currentPipeline;

	uint32_t pipelineObjSize;
	std::atomic<uint32_t> pipelineObjCount;
	EntryHandle* objects;

	VKDevice* dev;
};


struct VKGraphicsOneTimeQueue : public VKOneTimeQueue
{ 


	VKGraphicsOneTimeQueue() = default;
	VKGraphicsOneTimeQueue(VKGraphicsOneTimeQueue&& other) = delete;

	VKGraphicsOneTimeQueue(const VKGraphicsOneTimeQueue& other) = delete;
	VKGraphicsOneTimeQueue& operator=(const VKGraphicsOneTimeQueue& other) = delete;

	VKGraphicsOneTimeQueue& operator=(VKGraphicsOneTimeQueue&& other) = delete;

	VKGraphicsOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d);

	void DrawScene(RecordingBufferObject* rbo, uint32_t frameNum);

};


struct VKComputeOneTimeQueue : public VKOneTimeQueue
{

	VKComputeOneTimeQueue() = default;
	VKComputeOneTimeQueue(VKComputeOneTimeQueue&& other) = delete;

	VKComputeOneTimeQueue(const VKComputeOneTimeQueue& other) = delete;
	VKComputeOneTimeQueue& operator=(const VKComputeOneTimeQueue& other) = delete;

	VKComputeOneTimeQueue& operator=(VKComputeOneTimeQueue&& other) = delete;

	VKComputeOneTimeQueue(DeviceOwnedAllocator* allocator, uint32_t pipelineCount, VKDevice* _d);

	void DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum);

};
