#pragma once

#include "IndexTypes.h"
#include "RenderInstance.h"
#include "vulkan/vulkan.h"
#include <string>

class VKPipelineObject
{
public:

	VKPipelineObject() = delete;
	VKPipelineObject(
		std::string name,
		std::string descname,
		size_t vertexBufferIndex_,
		size_t vertexBufferOffset_,
		size_t* vCount,
		uint32_t renderTarget);

	~VKPipelineObject() = default;

	void Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet);

	void DrawIndirectOneBuffer(
		RecordingBufferObject& rbo,
		uint32_t bufferIndex,
		uint32_t drawCount,
		uint32_t frame,
		uint32_t firstSet,
		size_t indirectDrawBufferOffset);

	std::string GetPipelineType() const;

	void SetPerObjectData(void* data, size_t dataSize, OffsetIndex& _dynamicOffset);

	void* perObjectShaderData;
	VkDeviceSize perObjectShaderDataSize;
	OffsetIndex dynamicOffset;

	std::string pipelineType, descriptorSetName;
	VkDeviceSize vertexBufferIndex, vertexBufferOffset;
	size_t *vertexCount = nullptr;
	uint32_t mainRenderTarget;
	
	
};

