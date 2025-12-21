#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"
#include <shared_mutex>


struct VKGraph {

	VKGraph() = default;
	VKGraph(VKGraph&& other) = delete;
	  
	VKGraph(const VKGraph& other) = delete;
	VKGraph& operator=(const VKGraph& other) = delete;
	
	VKGraph& operator=(VKGraph&& other) = delete;

	VKGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d);

	uint32_t AddObject(EntryHandle obj);

	void AddDynamicOffset(uint32_t offset);

	bool SetActive(uint32_t objIndex, bool active);

	//std::shared_mutex objectGuard;

	EntryHandle* currentPipeline;
	EntryHandle* descriptorId;
	uint32_t* dynamicsPerSet;
	uint32_t* dynamicOffsets;
	EntryHandle* objects;
	uint8_t* activeIndicators;
	VKDevice* dev;

	uint32_t descriptorCount;	
	uint32_t maxFramesInFlight;

	uint32_t dynamicOffsetCount, dynamicOffsetSize;
	uint32_t pipelineObjCount, pipelineObjSize;
};

struct VKRenderGraph : public VKGraph
{


	VKRenderGraph() = default;
	VKRenderGraph(VKRenderGraph&& other) = delete;
	
	VKRenderGraph(const VKRenderGraph& other) = delete;
	VKRenderGraph& operator=(const VKRenderGraph& other) = delete;

	VKRenderGraph& operator=(VKRenderGraph&& other) = delete;

	VKRenderGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d);

	void DrawScene(RecordingBufferObject* rbo, uint32_t frameNum);

};


struct VKComputeGraph : public VKGraph
{

	VKComputeGraph() = default;
	VKComputeGraph(VKComputeGraph&& other) = delete;

	VKComputeGraph(const VKComputeGraph& other) = delete;
	VKComputeGraph& operator=(const VKComputeGraph& other) = delete;

	VKComputeGraph& operator=(VKComputeGraph&& other) = delete;

	VKComputeGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, uint32_t maxConcurrentAccesses, VKDevice* _d);

	void DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum);

};

