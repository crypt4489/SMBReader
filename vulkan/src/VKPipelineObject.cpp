#include "VKPipelineObject.h"

#include <array>
#include <vector>
#include <functional>

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKPipelineCache.h"

VKPipelineObject::VKPipelineObject(
	VKPipelineObjectCreateInfo& createinfo)
	:
	pipelineType(createinfo.pipelinename),
	descriptorSetName(createinfo.descriptorsetname),
	vertexCount(createinfo.vertexCount),
	vertexBufferOffset(createinfo.vertexBufferOffset),
	vertexBufferIndex(createinfo.vertexBufferIndex),
	indirectBufferIndex(createinfo.indirectDrawBuffer),
	indirectBufferOffset(createinfo.indirectDrawOffset),
	drawType(createinfo.drawType)
{

}


void VKPipelineObject::Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = vertexCount;

	if (!descriptorSetName.empty()) {

		std::vector<uint32_t> dynamicOffsets;

		for (auto& c : objectData)
		{
			dynamicOffsets.push_back(c);
		}

		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, 1, dynamicOffsets.data());
	}

	if (vertexBufferIndex != ~0ui32)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingDrawCmd(0, 4);
}

void VKPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject &rbo,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet)
{
	
	

	if (!descriptorSetName.empty()) {
		std::vector<uint32_t> dynamicOffsets;

		for (auto& c : objectData)
		{
			dynamicOffsets.push_back(static_cast<uint32_t>(c));
		}

		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
	}
		

	if (vertexBufferIndex != ~0ui32)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingIndirectDrawCmd(indirectBufferIndex, drawCount, indirectBufferOffset);
}


void VKPipelineObject::SetPerObjectData(uint32_t _dynamicOffset)
{
	objectData.emplace_back(_dynamicOffset);
}