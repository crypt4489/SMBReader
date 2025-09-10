#include "VKPipelineObject.h"

#include <array>
#include <vector>
#include <functional>

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VKDevice.h"
#include "VKPipelineCache.h"

VKPipelineObject::VKPipelineObject(
	VKPipelineObjectCreateInfo* createinfo)
	:
	pipelineType(createinfo->pipelinename),
	descriptorSetName(createinfo->descriptorsetname),
	vertexCount(createinfo->vertexCount),
	vertexBufferOffset(createinfo->vertexBufferOffset),
	vertexBufferIndex(createinfo->vertexBufferIndex),
	indirectBufferIndex(createinfo->indirectDrawBuffer),
	indirectBufferOffset(createinfo->indirectDrawOffset),
	drawType(createinfo->drawType),
	objectData(createinfo->data),
	maxObjectCapacity(createinfo->maxDynCap),
	objectCount(0ui32)
{

}


void VKPipelineObject::Draw(RecordingBufferObject& rbo, uint32_t frame, uint32_t firstSet)
{
	uint32_t drawSize = static_cast<uint32_t>(vertexCount);

	if (!descriptorSetName.empty()) {

		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, objectCount, objectData);
	}

	if (vertexBufferIndex != ~0ui64)
	{
		rbo.BindVertexBuffer(vertexBufferIndex, 0, 1, &vertexBufferOffset);
	}

	rbo.BindingDrawCmd(0, drawSize);
}

void VKPipelineObject::DrawIndirectOneBuffer(
	RecordingBufferObject &rbo,
	uint32_t drawCount,
	uint32_t frame,
	uint32_t firstSet)
{
	
	

	if (!descriptorSetName.empty()) {

		rbo.BindDescriptorSets(descriptorSetName, frame, 1, firstSet, objectCount, objectData);
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