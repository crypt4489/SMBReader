#include "pch.h"

#include "VKPipelineObject.h"

#include "VKDevice.h"

VKPipelineObject::VKPipelineObject(
	VKGraphicsPipelineObjectCreateInfo* createinfo)
	:
	pipelineType(createinfo->pipelinename),
	descriptorSetId(createinfo->descriptorsetid),
	vertexCount(createinfo->vertexCount),
	vertexBufferOffset(createinfo->vertexBufferOffset),
	vertexBufferIndex(createinfo->vertexBufferIndex),
	indirectBufferIndex(createinfo->indirectDrawBuffer),
	indirectBufferOffset(createinfo->indirectDrawOffset),
	drawType(createinfo->drawType),
	objectData(createinfo->data),
	maxObjectCapacity(createinfo->maxDynCap),
	objectCount(0ui32),
	indexBufferHandle(createinfo->indexBufferHandle),
	indexBufferOffset(createinfo->indexBufferOffset),
	indexCount(createinfo->indexCount)
{

}


void VKPipelineObject::Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = static_cast<uint32_t>(vertexCount);

	if (EntryHandle() != descriptorSetId) {

		rbo.BindDescriptorSets(descriptorSetId, frame, 1, firstSet, objectCount, objectData);
	}

	if (vertexBufferIndex != ~0ui64)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	if (indexBufferOffset != ~0ui64)
	{
		rbo.BindIndexBuffer(indexBufferHandle, indexBufferOffset);
		rbo.BindingDrawIndexedCmd(indexCount, 1, 0, 0, 0);
	}
	else {
		rbo.BindingDrawCmd(0, drawSize);
	}
}

void VKPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject &rbo,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet)
{
	
	

	if (EntryHandle() != descriptorSetId) {

		rbo.BindDescriptorSets(descriptorSetId, frame, 1, firstSet, objectCount, objectData);
	}
		

	if (vertexBufferIndex != ~0ui64)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingIndirectDrawCmd(indirectBufferIndex, drawCount, indirectBufferOffset);
}


void VKPipelineObject::SetPerObjectData(uint32_t _dynamicOffset)
{
	objectData[objectCount++] = _dynamicOffset;
}