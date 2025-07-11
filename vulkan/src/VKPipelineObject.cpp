#include "VKPipelineObject.h"

#include <array>
#include <vector>
#include <functional>

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKPipelineCache.h"

VKPipelineObject::VKPipelineObject(
	std::string name,
	std::string descname,
	size_t vertexBufferIndex_,
	size_t vertexBufferOffset_,
	size_t* vCount,
	uint32_t renderTarget)
	:
	pipelineType(name),
	descriptorSetName(descname),
	vertexCount(vCount),
	vertexBufferOffset(vertexBufferOffset_),
	vertexBufferIndex(vertexBufferIndex_),
	mainRenderTarget(renderTarget)
{

}

void VKPipelineObject::Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = static_cast<uint32_t>(*vertexCount);

	if (!descriptorSetName.empty()) {
		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, 0, nullptr);
	}

	if (vertexBufferIndex != ~0ui32)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingDrawCmd(0, drawSize);
}

void VKPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject &rbo,
	uint32_t bufferIndex,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet,
	size_t indirectDrawBufferOffset)
{
	
	

	if (!descriptorSetName.empty()) {
		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, 0, nullptr);
	}
		

	if (vertexBufferIndex != ~0ui32)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingIndirectDrawCmd(bufferIndex, drawCount, indirectDrawBufferOffset);
}

std::string VKPipelineObject::GetPipelineType() const
{
	return pipelineType;
}

void VKPipelineObject::SetPerObjectData(void* data, size_t dataSize, OffsetIndex& _dynamicOffset)
{
	perObjectShaderData = data;
	perObjectShaderDataSize = dataSize;
	dynamicOffset = std::move(_dynamicOffset);
}