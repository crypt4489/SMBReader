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

	VKGraph(DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d);

	uint32_t AddObject(EntryHandle obj);

	void AddDynamicOffset(uint32_t offset);

	bool SetActive(uint32_t objIndex, bool active);

	EntryHandle currentPipeline;

	EntryHandle *descriptorId;
	uint32_t descriptorCount;

	uint32_t* dynamicOffsets;

	std::shared_mutex objectGuard;

	EntryHandle* objects;

	uint8_t* activeIndicators;

	VKDevice* dev;

	size_t dynamicOffsetCount, dynamicOffsetSize;
	size_t pipelineObjCount, pipelineObjSize;
};

class VKRenderGraph : public VKGraph
{
public:

	VKRenderGraph() = default;
	VKRenderGraph(VKRenderGraph&& other) = delete;
	
	VKRenderGraph(const VKRenderGraph& other) = delete;
	VKRenderGraph& operator=(const VKRenderGraph& other) = delete;

	VKRenderGraph& operator=(VKRenderGraph&& other) = delete;

	VKRenderGraph(EntryHandle _renderTargetIndex, DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d);

	void DrawScene(RecordingBufferObject* rbo, uint32_t frameNum);
	
	EntryHandle renderTargetIndex;

};


struct VKComputeGraph : public VKGraph
{

	VKComputeGraph() = default;
	VKComputeGraph(VKComputeGraph&& other) = delete;

	VKComputeGraph(const VKComputeGraph& other) = delete;
	VKComputeGraph& operator=(const VKComputeGraph& other) = delete;

	VKComputeGraph& operator=(VKComputeGraph&& other) = delete;

	VKComputeGraph(DeviceAllocator* allocator, size_t dCount, size_t descCount, size_t pCount, VKDevice* _d);

	void DispatchWork(RecordingBufferObject* rbo, uint32_t frameNum);

};

