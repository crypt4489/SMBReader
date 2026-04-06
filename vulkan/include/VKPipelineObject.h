#pragma once

#include "IndexTypes.h"
#include "VKTypes.h"
#include "vulkan/vulkan.h"

enum PipelineModMember
{
	// VKPipelineObject
	PIPELINE_MOD_MEMBER_TYPE,
	PIPELINE_MOD_MEMBER_OBJECTDATA,
	PIPELINE_MOD_MEMBER_MAXOBJECTCAPACITY,
	PIPELINE_MOD_MEMBER_OBJECTCOUNT,
	PIPELINE_MOD_MEMBER_PIPELINEID,
	PIPELINE_MOD_MEMBER_DESCRIPTORSETID,
	PIPELINE_MOD_MEMBER_DESCRIPTORCOUNT,
	PIPELINE_MOD_MEMBER_DYNAMICPERSET,
	PIPELINE_MOD_MEMBER_PUSHCONSTANTCOUNT,
	PIPELINE_MOD_MEMBER_PUSHARGS,
	PIPELINE_MOD_MEMBER_INFOS,
	PIPELINE_MOD_MEMBER_MEMBARRIERCAPACITY,
	PIPELINE_MOD_MEMBER_MEMBARRIERCOUNTER,

	// VKGraphicsPipelineObject
	PIPELINE_MOD_MEMBER_VERTEXBUFFERINDEX,
	PIPELINE_MOD_MEMBER_INDEXBUFFERHANDLE,
	PIPELINE_MOD_MEMBER_INDIRECTBUFFERHANDLE,
	PIPELINE_MOD_MEMBER_INDIRECTCOUNTBUFFERHANDLE,
	PIPELINE_MOD_MEMBER_VERTEXBUFFEROFFSET,
	PIPELINE_MOD_MEMBER_INDEXBUFFEROFFSET,
	PIPELINE_MOD_MEMBER_INDIRECTBUFFEROFFSET,
	PIPELINE_MOD_MEMBER_INDIRECTCOUNTBUFFEROFFSET,
	PIPELINE_MOD_MEMBER_VERTEXCOUNT,
	PIPELINE_MOD_MEMBER_INDEXCOUNT,
	PIPELINE_MOD_MEMBER_INSTANCECOUNT,
	PIPELINE_MOD_MEMBER_INDEXTYPE,
	PIPELINE_MOD_MEMBER_INDIRECTCOMMANDSTRIDE,
	PIPELINE_MOD_MEMBER_INDIRECTCOUNTSTRIDE,
	PIPELINE_MOD_MEMBER_INDIRECTDRAWCOUNT,

	// VKComputePipelineObject
	PIPELINE_MOD_MEMBER_X,
	PIPELINE_MOD_MEMBER_Y,
	PIPELINE_MOD_MEMBER_Z
};

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
	uint32_t indirectDrawCount;
	EntryHandle indirectBufferHandle;
	uint32_t indirectBufferOffset;
	uint32_t indirectBufferFrames;
	EntryHandle indirectCountBufferHandle;
	uint32_t indirectCountBufferOffset;
	uint32_t indirectCountBufferStride;
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
	GRAPHICSPIPELINETYPE = 0,
	COMPUTEPIPELINETYPE = 1,
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
	uint32_t perFrameOffset;
};

struct VKPipelineObject
{
	PipelineObjectType type;
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
	VKPipelineObject(DeviceOwnedAllocator* alloc, EntryHandle _pid, EntryHandle* _dsid, uint32_t* dynamicPerSet, uint32_t descCount, uint32_t moc, uint32_t pcrCount, uint32_t memBarrierCount, PipelineObjectType type);

	~VKPipelineObject() = default;

	void SetPerObjectData(uint32_t _dynamicOffset);

	void SetPerObjectData(uint32_t* offsets, uint32_t count);

	void AddPushConstant(void* _data, uint32_t size, uint32_t offset, uint32_t bindLocation, VkShaderStageFlags flags);

	void AddBufferMemoryBarrier(
		EntryHandle index, VKBarrierLocation location,
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, uint32_t perFrameOffset
	);

	void AddImageMemoryBarrier(
		EntryHandle index, VKBarrierLocation location, VkShaderStageFlags srcStage, VkShaderStageFlags dstStage
	);

	VkBarrierInfo* GetNextBarrierInfo(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

	void CreatePipelineBarriers(RecordingBufferObject* rbo, VKBarrierLocation location, uint32_t frame);
	
	void AddInfoBarrier(VkBarrierInfo* info, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, 
		EntryHandle barrierIndex, VKBarrierLocation location, uint16_t barrierType);

	void ChangePipelineMember(
		PipelineModMember member,
		void* value
	);
};


struct VKGraphicsPipelineObject : public VKPipelineObject
{
	VKGraphicsPipelineObject() = delete;
	VKGraphicsPipelineObject(VKGraphicsPipelineObjectCreateInfo *createinfo, DeviceOwnedAllocator* alloc);

	~VKGraphicsPipelineObject() = default;

	void Draw(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

	EntryHandle vertexBufferIndex, indexBufferHandle, indirectBufferHandle, indirectCountBufferHandle;

	size_t vertexBufferOffset, indexBufferOffset, indirectBufferOffset, indirectCountBufferOffset;
	size_t vertexCount, indexCount;
	uint32_t instanceCount;
	VkIndexType indexType;
	uint32_t indirectCommandStride;
	uint32_t indirectCountStride;
	uint32_t indirectDrawCount;

	void ChangePipelineMember(
		PipelineModMember member,
		void* value
	);
};



struct VKComputePipelineObject : public VKPipelineObject
{
	uint32_t x, y, z;

	VKComputePipelineObject() = delete;
	~VKComputePipelineObject() = default;
	VKComputePipelineObject(VKComputePipelineObjectCreateInfo* info, DeviceOwnedAllocator* alloc);

	void Dispatch(RecordingBufferObject* rbo, uint32_t frame, uint32_t firstSet);

	void ChangePipelineMember(
		PipelineModMember member,
		void* value
	);
};

