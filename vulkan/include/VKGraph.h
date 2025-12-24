#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"


struct VKGraph {

	VKGraph() = default;
	VKGraph(VKGraph&& other) = delete;
	  
	VKGraph(const VKGraph& other) = delete;
	VKGraph& operator=(const VKGraph& other) = delete;
	
	VKGraph& operator=(VKGraph&& other) = delete;

	VKGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d);

	uint32_t AddObject(EntryHandle obj);

	void AddDynamicOffset(uint32_t offset);

	bool SetActive(uint32_t objIndex, bool active);

	void UpdateLists();

	EntryHandle currentPipeline;
	
	uint32_t descriptorCount;
	EntryHandle* descriptorId;
	uint32_t* dynamicsPerSet;

	uint32_t dynamicOffsetSize, dynamicOffsetCount;
	uint32_t* dynamicOffsets;

	uint32_t pipelineObjSize;
	std::atomic<uint32_t> pipelineObjCount;
	EntryHandle* objects;
	EntryHandle* objectsCopies;
	uint8_t* activeIndicators;
	uint8_t* activeIndicatorsCopies;

	VKDevice* dev;
};

struct VKRenderGraph : public VKGraph
{


	VKRenderGraph() = default;
	VKRenderGraph(VKRenderGraph&& other) = delete;
	
	VKRenderGraph(const VKRenderGraph& other) = delete;
	VKRenderGraph& operator=(const VKRenderGraph& other) = delete;

	VKRenderGraph& operator=(VKRenderGraph&& other) = delete;

	VKRenderGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d);

	void DrawScene(RecordingBufferObject* rbo, uint32_t frameNum);

};


struct VKComputeGraph : public VKGraph
{

	VKComputeGraph() = default;
	VKComputeGraph(VKComputeGraph&& other) = delete;

	VKComputeGraph(const VKComputeGraph& other) = delete;
	VKComputeGraph& operator=(const VKComputeGraph& other) = delete;

	VKComputeGraph& operator=(VKComputeGraph&& other) = delete;

	VKComputeGraph(DeviceOwnedAllocator* allocator, uint32_t dynamicCount, uint32_t descriptorCount, uint32_t pipelineCount, VKDevice* _d);

	void DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum);

};

