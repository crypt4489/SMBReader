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
	EntryHandle descriptorId;
	EntryHandle pipelineId;
	uint32_t maxDynCap;
	uint32_t* data;
	uint32_t barrierCount;
	uint32_t pushRangeCount;
};

struct VKGraphicsPipelineObjectCreateInfo
{
	uint32_t drawType;
	EntryHandle vertexBufferIndex;
	uint32_t vertexBufferOffset;
	uint32_t vertexCount;
	EntryHandle indirectDrawBuffer;
	uint32_t indirectDrawOffset;
	EntryHandle pipelinename;
	EntryHandle descriptorsetid;
	uint32_t maxDynCap;
	uint32_t* data;
	EntryHandle indexBufferHandle;
	uint32_t indexBufferOffset;
	uint32_t indexCount;
	uint32_t pushRangeCount;
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

struct VkBarrierInfo
{
	uint32_t type;
	VkPipelineStageFlags srcStageMask;
	VkPipelineStageFlags dstStageMask;
	VkDependencyFlags dependencyFlags;
	EntryHandle barrierIndex;
	struct VkBarrierInfo* next;
	struct VkBarrierInfo* child;
};

struct VKPipelineObject
{
	uint32_t type;
	uint32_t* objectData;
	uint32_t maxObjectCapacity, objectCount;

	EntryHandle pipelineID;
	EntryHandle descriptorSetId;

	uint32_t pushConstantCount;
	PushConstantArguments* pushArgs;

	VkBarrierInfo* infos;
	uint32_t memBarrierCapacity;
	uint32_t memBarrierCounter;

	VKPipelineObject() = delete;
	VKPipelineObject(EntryHandle _pid, EntryHandle _dsid, uint32_t* data, uint32_t moc, PipelineObjectType _type, uint32_t pcrCount, uint32_t memBarrierCount);

	~VKPipelineObject() = default;

	void SetPerObjectData(uint32_t objectlocation);

	void AddPushConstant(void* _data, uint32_t size, uint32_t offset, uint32_t bindLocation, VkShaderStageFlags flags);

	void AddBufferMemoryBarrier(
		VKDevice* d, EntryHandle bufferIndex,
		size_t size, size_t offset,
		VkAccessFlags srcPoint, VkAccessFlags dstPoint,
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage
	);
};


struct VKGraphicsPipelineObject : public VKPipelineObject
{
	VKGraphicsPipelineObject() = delete;
	VKGraphicsPipelineObject(VKGraphicsPipelineObjectCreateInfo *createinfo);

	~VKGraphicsPipelineObject() = default;

	void Draw(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

	void DrawIndirectOneBuffer(
		RecordingBufferObject* rbo,
		uint32_t drawCount,
		uint32_t frame,
		uint32_t firstSet);

	EntryHandle vertexBufferIndex, indirectBufferIndex, indexBufferHandle;
	uint32_t drawType;

	std::size_t vertexBufferOffset, indirectBufferOffset, indexBufferOffset;
	std::size_t vertexCount, indexCount;
};



struct VKComputePipelineObject : public VKPipelineObject
{
	uint32_t x, y, z;

	VKComputePipelineObject() = delete;
	~VKComputePipelineObject() = default;
	VKComputePipelineObject(VKComputePipelineObjectCreateInfo* info);

	void Dispatch(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

};

