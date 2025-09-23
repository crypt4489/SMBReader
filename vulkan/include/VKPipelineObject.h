#pragma once

#include "IndexTypes.h"
#include "VKTypes.h"
#include "vulkan/vulkan.h"
#include <string>
#include <vector>

struct VKPipelineObjectCreateInfo
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
};


class VKPipelineObject
{
public:

	VKPipelineObject() = delete;
	VKPipelineObject(VKPipelineObjectCreateInfo *createinfo);

	~VKPipelineObject() = default;

	void Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet);

	void DrawIndirectOneBuffer(
		RecordingBufferObject& rbo,
		uint32_t drawCount,
		uint32_t frame,
		uint32_t firstSet);

	void SetPerObjectData(uint32_t objectlocation);

	uint32_t *objectData;
	uint32_t maxObjectCapacity, objectCount;

	EntryHandle pipelineType;
	EntryHandle descriptorSetId;

	EntryHandle vertexBufferIndex, indirectBufferIndex, indexBufferHandle;
	uint32_t drawType;

	std::size_t vertexBufferOffset, indirectBufferOffset, indexBufferOffset;
	std::size_t vertexCount, indexCount;
};

