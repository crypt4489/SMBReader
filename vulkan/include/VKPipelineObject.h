#pragma once

#include "IndexTypes.h"
#include "VKDevice.h"
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
	std::string pipelinename;
	std::string descriptorsetname;
};


class VKPipelineObject
{
public:

	VKPipelineObject() = delete;
	VKPipelineObject(VKPipelineObjectCreateInfo &createinfo);

	~VKPipelineObject() = default;

	void Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet);

	void DrawIndirectOneBuffer(
		RecordingBufferObject& rbo,
		uint32_t drawCount,
		uint32_t frame,
		uint32_t firstSet);

	void SetPerObjectData(uint32_t objectlocation);

	std::vector<uint32_t> objectData;

	std::string pipelineType, descriptorSetName;

	EntryHandle vertexBufferIndex, indirectBufferIndex;
	uint32_t drawType;

	std::size_t vertexBufferOffset, indirectBufferOffset;
	std::size_t vertexCount;
};

