#pragma once

#include "IndexTypes.h"
#include "VKTypes.h"
#include "vulkan/vulkan.h"
#include <string>
#include <vector>

struct VKComputePipelineObjectCreateInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t descCount;
	EntryHandle* descriptorId;
	uint32_t* dynamicPerSet;
	EntryHandle pipelineId;
	uint32_t maxDynCap;
	uint32_t barrierCount;
	uint32_t pushRangeCount;
};

struct VKGraphicsPipelineObjectCreateInfo
{
	EntryHandle vertexBufferIndex;
	uint32_t vertexBufferOffset;
	uint32_t vertexCount;
	EntryHandle pipelinename;
	uint32_t descCount;
	EntryHandle* descriptorsetid;
	uint32_t* dynamicPerSet;
	uint32_t maxDynCap;
	EntryHandle indexBufferHandle;
	uint32_t indexBufferOffset;
	uint32_t indexCount;
	uint32_t pushRangeCount;
	uint32_t instanceCount;
	uint32_t indexSize;
};

struct VKIndirectPipelineObjectCreateInfo
{
	EntryHandle vertexBufferIndex;
	uint32_t vertexBufferOffset;
	uint32_t vertexCount;
	EntryHandle pipelinename;
	uint32_t descCount;
	EntryHandle* descriptorsetid;
	uint32_t* dynamicPerSet;
	uint32_t maxDynCap;
	EntryHandle indexBufferHandle;
	uint32_t indexBufferOffset;
	uint32_t pushRangeCount;
	uint32_t indexSize;
	uint32_t indirectDrawCount;
	EntryHandle indirectBufferHandle;
	uint32_t indirectBufferOffset;
};

struct PushConstantArguments
{
	VkShaderStageFlags stage;
	uint32_t offset;
	uint32_t size;
	void* data;
};

enum PipelineObjectType
{
	GRAPHICSPO = 0,
	COMPUTEPO = 1
};

enum VKBarrierType
{
	MEMBARRIER = 0,
	BUFFBARRIER = 1,
	IMAGEBARRIER = 2
};

enum VKBarrierLocation
{
	BEFORE = 0,
	AFTER = 1
};

struct VkBarrierInfo
{
	uint16_t type;
	uint16_t location;
	VkPipelineStageFlags srcStageMask;
	VkPipelineStageFlags dstStageMask;
	VkDependencyFlags dependencyFlags;
	EntryHandle barrierIndex;
	struct VkBarrierInfo* next;
	struct VkBarrierInfo* child;
};

struct VKPipelineObject
{
	uint32_t* objectData;
	uint32_t maxObjectCapacity, objectCount;

	EntryHandle pipelineID;
	EntryHandle* descriptorSetId;
	uint32_t descriptorCount;
	uint32_t* dynamicPerSet;

	uint32_t pushConstantCount;
	PushConstantArguments* pushArgs;

	VkBarrierInfo* infos;
	uint32_t memBarrierCapacity;
	uint32_t memBarrierCounter;

	VKPipelineObject() = delete;
	VKPipelineObject(DeviceOwnedAllocator* alloc, EntryHandle _pid, EntryHandle* _dsid, uint32_t* dynamicPerSet, uint32_t descCount, uint32_t moc, uint32_t pcrCount, uint32_t memBarrierCount);

	~VKPipelineObject() = default;

	void SetPerObjectData(uint32_t objectlocation);

	void AddPushConstant(void* _data, uint32_t size, uint32_t offset, uint32_t bindLocation, VkShaderStageFlags flags);

	void AddBufferMemoryBarrier(
		EntryHandle index, VKBarrierLocation location,
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
	);

	void AddImageMemoryBarrier(
		EntryHandle index, VKBarrierLocation location, VkShaderStageFlags srcStage, VkShaderStageFlags dstStage
	);

	VkBarrierInfo* GetNextBarrierInfo(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

	void CreatePipelineBarriers(RecordingBufferObject* rbo, VKBarrierLocation location);
	
	void AddInfoBarrier(VkBarrierInfo* info, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
		EntryHandle barrierIndex, VKBarrierLocation location, uint16_t barrierType);
};


struct VKGraphicsPipelineObject : public VKPipelineObject
{
	VKGraphicsPipelineObject() = delete;
	VKGraphicsPipelineObject(VKGraphicsPipelineObjectCreateInfo *createinfo, DeviceOwnedAllocator* alloc);

	~VKGraphicsPipelineObject() = default;

	void Draw(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

	EntryHandle vertexBufferIndex, indexBufferHandle;

	std::size_t vertexBufferOffset, indexBufferOffset;
	std::size_t vertexCount, indexCount;
	uint32_t instanceCount;
	VkIndexType indexType;
};


struct VKIndirectPipelineObject : public VKPipelineObject
{
	VKIndirectPipelineObject() = delete;
	VKIndirectPipelineObject(VKIndirectPipelineObjectCreateInfo* createinfo, DeviceOwnedAllocator* alloc);

	~VKIndirectPipelineObject() = default;

	void Draw(
		RecordingBufferObject* rbo,
		uint32_t frame,
		uint32_t firstSet
	);

	EntryHandle vertexBufferHandle, indirectBufferHandle, indexBufferHandle;

	std::size_t vertexBufferOffset, indirectBufferOffset, indexBufferOffset;
	
	uint32_t drawCount;
	VkIndexType indexType;

};



struct VKComputePipelineObject : public VKPipelineObject
{
	uint32_t x, y, z;

	VKComputePipelineObject() = delete;
	~VKComputePipelineObject() = default;
	VKComputePipelineObject(VKComputePipelineObjectCreateInfo* info, DeviceOwnedAllocator* alloc);

	void Dispatch(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

};

