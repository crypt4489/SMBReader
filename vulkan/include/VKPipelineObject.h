#pragma once

#include <array>
#include <vector>
#include <functional>

#include "vulkan/vulkan.h"

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKPipelineCache.h"

class VKPipelineObject
{
public:

	VKPipelineObject() = delete;
	VKPipelineObject(
		std::string name,
		size_t vertexBufferIndex_,
		size_t vertexBufferOffset_,
		size_t* vCount);

	~VKPipelineObject() = default;

	void Draw(VkCommandBuffer& cb, uint32_t frame, uint32_t setCount);

	void DrawIndirectOneBuffer(
		VkCommandBuffer& cb,
		VkBuffer& drawBuffer,
		uint32_t drawCount,
		uint32_t frame,
		uint32_t setCount,
		size_t indirectDrawBufferOffset);

	std::string GetPipelineType() const;

	void SetPerObjectData(void* data, size_t dataSize, uint32_t _dynamicOffset);

	void* perObjectShaderData;
	VkDeviceSize perObjectShaderDataSize;
	uint32_t dynamicOffset;

	std::string pipelineType;
	VkDeviceSize vertexBufferIndex, vertexBufferOffset;
	size_t *vertexCount = nullptr;
	
	
};

