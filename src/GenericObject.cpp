#include "GenericObject.h"

#include "AppTypes.h"
#include "ApplicationLoop.h"
#include "RenderInstance.h"
#include "VertexTypes.h"
#include "VKDescriptorSetBuilder.h"



GenericObject::GenericObject(RenderingBackend be, size_t _oi, std::vector<int> &images) : objectIndex(_oi)
{

	textures = images;

	m = new Mesh();

	auto rendInst = VKRenderer::gRenderInstance;
	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

	int graphicDesc = rendInst->AllocateShaderResourceSet(0, 2, frames);

	objSpecificMemIndex[0] = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, alignof(glm::mat4));
	int objMorphFromVertexMemory = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));
	int objMorphToVertexMemory = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));

	objVertexMemoryIndex = rendInst->GetPageFromDeviceBuffer(m->vertexSize, alignof(glm::vec4));
	objIndexMemoryIndex = rendInst->GetPageFromDeviceBuffer(m->indexSize, alignof(uint32_t));

	rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, loop->instanceAlloc, DIRECT, 0);
	//rendInst->descriptorManager.BindSampledImageToShaderResource(graphicDesc, loop->mainDictionary.textureHandles[textures[0]], 1);

	int computeDesc = rendInst->AllocateShaderResourceSet(2, 0, frames);

	rendInst->descriptorManager.BindBufferToShaderResource(computeDesc, objMorphFromVertexMemory, DIRECT, 0);
	rendInst->descriptorManager.BindBufferToShaderResource(computeDesc, objMorphToVertexMemory, DIRECT, 1);
	rendInst->descriptorManager.BindBufferToShaderResource(computeDesc, objVertexMemoryIndex, DIRECT, 2);
	rendInst->descriptorManager.BindBarrier(computeDesc, 2, VERTEX_INPUT_BARRIER, READ_VERTEX_INPUT);
	rendInst->descriptorManager.UploadConstant(computeDesc, &interpolate, 0);

	rendInst->UpdateAllocation((void*)((uintptr_t)m->GetIndexData() + m->indexSize), objMorphToVertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

	rendInst->UpdateAllocation(m->GetVertexData(), objMorphFromVertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

	rendInst->UpdateAllocation(m->GetIndexData(), objIndexMemoryIndex, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);


	ShaderComputeLayout* layout = rendInst->GetComputeLayout(2);

	if (be == RenderingBackend::VULKAN)
	{

		GraphicsIntermediaryPipelineInfo create = {
			.drawType = 0,
			.vertexBufferIndex = objVertexMemoryIndex,
			.vertexCount = m->vertexCount,
			.indirectDrawBuffer{},
			.pipelinename = GENERIC,
			.descCount = 1,
			.descriptorsetid = &graphicDesc,
			.maxDynCap = 1,
			.indexBufferHandle = objIndexMemoryIndex,
			.indexCount = m->indexCount,
			.pushRangeCount = 0, 
			.instanceCount = 4096,
		};


		std::array compDynamicOffsets = { objMorphFromVertexMemory, objMorphToVertexMemory, objVertexMemoryIndex };

		ComputeIntermediaryPipelineInfo create2 = {
			.x = m->vertexCount / layout->x,
			.y = 1,
			.z = 1,
			.maxDynCap = 3,
			.pipelinename = MESH_INTERPOLATE,
			.descCount = 1,
			.descriptorsetid = &computeDesc,
			.barrierCount = 1,
			.pushRangeCount = 1
		};

		interpolate = 0.5f;

		uint32_t cOffsetPerSet = 3;

		computeHandle = rendInst->CreateComputeVulkanPipelineObject(&create2, compDynamicOffsets.data(), &cOffsetPerSet);

		std::array dynamicOffsets = { loop->instanceAlloc };

		uint32_t gOffsetPerSet = 1;

		pipelineIndex = rendInst->CreateGraphicsVulkanPipelineObject(&create, dynamicOffsets.data(), &gOffsetPerSet);
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
	//memoryCallback(&mat, sizeof(glm::mat4), objSpecificMemIndex[0]);
	//memoryCallback(&interpolate, sizeof(float), objSpecificMemIndex[1]);




}
