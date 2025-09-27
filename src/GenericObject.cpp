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

		objSpecificMemIndex = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, alignof(glm::mat4));
		objVertexMemoryIndex = rendInst->GetPageFromUniformBuffer(m->vertexSize, alignof(glm::vec4));
		objIndexMemoryIndex = rendInst->GetPageFromUniformBuffer(m->indexSize, alignof(uint32_t));

		IntermediaryPipelineInfo create = {
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

		rendInst->UpdateAllocation(m->GetVertexData(), objVertexMemoryIndex, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

		rendInst->UpdateAllocation(m->GetIndexData(), objIndexMemoryIndex, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

		std::array dynamicOffsets = { objSpecificMemIndex };

		pipelineIndex = rendInst->CreateVulkanPipelineObject(&create, dynamicOffsets.data());
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
	memoryCallback(&mat, sizeof(glm::mat4), objSpecificMemIndex);
}
