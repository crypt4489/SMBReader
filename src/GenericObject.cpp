#include "GenericObject.h"

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VertexTypes.h"
#include "VKDescriptorSetBuilder.h"

GenericObject::GenericObject(const SMBFile& file, RenderingBackend be, size_t _oi) : objectIndex(_oi)
{
	for (const auto& chunk : file.chunks)
	{
		switch (chunk.chunkType)
		{
		case GEO:
			break;
		case TEXTURE:
			textures.emplace_back(file, chunk);
			break;
		case GR2:
			break;
		case Joints:
			break;
		default:
			std::cerr << "Unprocessed chunkType\n";
			break;
		}
	}

	m = new Mesh();

	auto rendInst = VKRenderer::gRenderInstance;
	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

	int graphicDesc = rendInst->AllocateDescriptorSet(0, 1, 1, frames);

	objSpecificMemIndex[0] = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, alignof(glm::mat4));
	int objMorphFromVertexMemory = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));
	int objMorphToVertexMemory = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));

	objVertexMemoryIndex = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));
	objIndexMemoryIndex = rendInst->GetPageFromDeviceBuffer(m->indexSize, alignof(uint32_t));

	rendInst->BindBufferToDescriptor(graphicDesc, objSpecificMemIndex[0], false, 0);
	rendInst->BindImageToDescriptor(graphicDesc, textures[0].vkImpl, 1);

	int computeDesc = rendInst->AllocateDescriptorSet(2, 0, 3, frames);

	rendInst->BindBufferToDescriptor(computeDesc, objMorphFromVertexMemory, true, 0);
	rendInst->BindBufferToDescriptor(computeDesc, objMorphToVertexMemory, true, 1);
	rendInst->BindBufferToDescriptor(computeDesc, objVertexMemoryIndex, true, 2);

	rendInst->UpdateAllocation((void*)((uintptr_t)m->GetIndexData() + m->indexSize), objMorphToVertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

	rendInst->UpdateAllocation(m->GetVertexData(), objMorphFromVertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

	rendInst->UpdateAllocation(m->GetIndexData(), objIndexMemoryIndex, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);


	if (be == RenderingBackend::VULKAN)
	{

		GraphicsIntermediaryPipelineInfo create = {
			.drawType = 0,
			.vertexBufferIndex = objVertexMemoryIndex,
			.vertexCount = m->vertexCount,
			.indirectDrawBuffer{},
			.pipelinename = GENERIC,
			.descriptorsetid = graphicDesc,
			.maxDynCap = 1,
			.indexBufferHandle = objIndexMemoryIndex,
			.indexCount = m->indexCount,
			.pushRangeCount = 0
		};


		std::array compDynamicOffsets = { objMorphFromVertexMemory, objMorphToVertexMemory, objVertexMemoryIndex };

		ComputeIntermediaryPipelineInfo create2 = {
			.x = m->vertexCount / 8,
			.y = 1,
			.z = 1,
			.maxDynCap = 3,
			.pipelinename = MESH_INTERPOLATE,
			.descriptorsetid = computeDesc,
			.barrierCount = 1,
			.pushRangeCount = 1
		};

		interpolate = 0.5f;

		std::array<std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>, 1> pushArgs = {
			std::tuple<void*, uint32_t, uint32_t, VkShaderStageFlags>{&interpolate, 4, 0, VK_SHADER_STAGE_COMPUTE_BIT}
		};

		ResourceGraphNode computeGraphNode(1, 0);

		computeGraphNode.AddBufferBarrier(COMPUTE_STAGE, VERTEX_INPUT_STAGE, WRITE_SHADER_RESOURCE, READ_VERTEX_INPUT, EntryHandle(objVertexMemoryIndex), m->vertexSize);

		EntryHandle handle = rendInst->CreateComputeVulkanPipelineObject(&create2, compDynamicOffsets.data(), pushArgs.data(), &computeGraphNode);

		std::array dynamicOffsets = { objSpecificMemIndex[0] };

		pipelineIndex = rendInst->CreateGraphicsVulkanPipelineObject(&create, dynamicOffsets.data(), nullptr, nullptr);
	}
}

GenericObject::~GenericObject()
{
	textures.clear();
	delete m;
}


void GenericObject::CallUpdate()
{
	updateObject(this);
	memoryCallback(&mat, sizeof(glm::mat4), objSpecificMemIndex[0]);
	//memoryCallback(&interpolate, sizeof(float), objSpecificMemIndex[1]);




}
