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

	if (be == RenderingBackend::VULKAN)
	{
		std::string genericpipeline = "genericpipeline";

		auto rendInst = VKRenderer::gRenderInstance;
		uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

		DescriptorSetBuilder *dsb = rendInst->CreateDescriptorSet(rendInst->descriptorLayouts["genericobject"], frames);
		dsb->AddDynamicUniformBuffer(rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, 0);
		dsb->AddPixelShaderImageDescription(rendInst->GetImageView(textures[0].vkImpl), rendInst->GetSampler(textures[0].vkImpl), 1, frames);
		EntryHandle descHandle = dsb->AddDescriptorsToCache();


		struct ubo
		{
			glm::mat4 mat; //need 64 bytes padding, will switch to pipeline push constant
		};

		objSpecificMemIndex[0] = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, alignof(glm::mat4));
		size_t objMorphFromVertexMemory = rendInst->GetPageFromUniformBuffer(m->vertexSize, alignof(glm::vec4));
		size_t objMorphToVertexMemory = rendInst->GetPageFromUniformBuffer(m->vertexSize, alignof(glm::vec4));
		objSpecificMemIndex[1] = rendInst->GetPageFromUniformBuffer(sizeof(ubo), alignof(ubo));
		objVertexMemoryIndex = rendInst->GetPageFromUniformBuffer(m->vertexSize, alignof(glm::vec4));
		objIndexMemoryIndex = rendInst->GetPageFromUniformBuffer(m->indexSize, alignof(uint32_t));

		DescriptorSetBuilder* cdsb = rendInst->CreateDescriptorSet(rendInst->descriptorLayouts["compute"], frames);
		cdsb->AddDynamicUniformBufferDirect(rendInst->GetDynamicUniformBuffer(), sizeof(ubo), 0, frames, 0);
		cdsb->AddDynamicStorageBufferDirect(rendInst->GetDynamicUniformBuffer(), m->vertexSize, 1, frames, 0);
		cdsb->AddDynamicStorageBufferDirect(rendInst->GetDynamicUniformBuffer(), m->vertexSize, 2, frames, 0);
		cdsb->AddDynamicStorageBufferDirect(rendInst->GetDynamicUniformBuffer(), m->vertexSize, 3, frames, 0);

		EntryHandle computeID = cdsb->AddDescriptorsToCache();

		GraphicsIntermediaryPipelineInfo create = {
			.drawType = 0,
			.vertexBufferIndex = objVertexMemoryIndex,
			.vertexCount = m->vertexCount,
			.indirectDrawBuffer{},
			.pipelinename = rendInst->pipelinesIdentifier[genericpipeline],
			.descriptorsetid = descHandle,
			.maxDynCap = 1,
			.indexBufferHandle = objIndexMemoryIndex,
			.indexCount = m->indexCount,
		};

		rendInst->UpdateAllocation((void*)((uintptr_t)m->GetIndexData() + m->indexSize), objMorphToVertexMemory, m->vertexSize, ABSOLUTE_ALLOCATION_OFFSET);

		


		
		//rendInst->UpdateAllocation(m->GetVertexData(), objVertexMemoryIndex, m->vertexSize, ABSOLUTE_ALLOCATION_OFFSET);

		rendInst->UpdateAllocation(m->GetVertexData(), objMorphFromVertexMemory, m->vertexSize, ABSOLUTE_ALLOCATION_OFFSET);

		rendInst->UpdateAllocation(m->GetIndexData(), objIndexMemoryIndex, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);


		std::array compDynamicOffsets = { objSpecificMemIndex[1], objMorphFromVertexMemory, objMorphToVertexMemory, objVertexMemoryIndex };

		ComputeIntermediaryPipelineInfo create2 = {
			.x = m->vertexCount / 8,
			.y = 1,
			.z = 1,
			.maxDynCap = 4,
			.pipelinename = rendInst->pipelinesIdentifier["compute"],
			.descriptorsetid = computeID,
			.barrierCount = 1
		};

		EntryHandle handle = rendInst->CreateComputeVulkanPipelineObject(&create2, compDynamicOffsets.data());

		rendInst->CreateBufferMemBarrier(handle, objVertexMemoryIndex, m->vertexSize);

		std::array dynamicOffsets = { objSpecificMemIndex[0] };

		pipelineIndex = rendInst->CreateGraphicsVulkanPipelineObject(&create, dynamicOffsets.data());
	}
}

GenericObject::~GenericObject()
{
	textures.clear();
	delete m;
}

void GenericObject::SetMatrix(glm::mat4& f)
{
	mat = f;
}

void GenericObject::SetPerObjectCallback(std::function<void(GenericObject*)> f)
{
	updateObject = f;
}

void GenericObject::SetPerObjectMemoryCallback(std::function<void(void*, size_t, size_t)> ptr)
{
	memoryCallback = ptr;
}

void GenericObject::CallUpdate()
{
	updateObject(this);
	memoryCallback(&mat, sizeof(glm::mat4), objSpecificMemIndex[0]);
	memoryCallback(&interpolate, sizeof(float), objSpecificMemIndex[1]);




}
