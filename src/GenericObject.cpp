#include "GenericObject.h"

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VertexTypes.h"
#include "VKInstance.h"
#include "VKDevice.h"
#include "VKDescriptorSetBuilder.h"
#include "VKPipelineObject.h"

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
		size_t offset = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, 16);
		dsb->AddDynamicUniformBuffer(rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, 0);
		dsb->AddPixelShaderImageDescription(rendInst->GetImageView(textures[0].vkImpl), rendInst->GetSampler(textures[0].vkImpl), 1, frames);
		EntryHandle descHandle = dsb->AddDescriptorsToCache();

		uint32_t vertexOff = static_cast<uint32_t>(rendInst->GetPageFromUniformBuffer(m->vertexSize, alignof(glm::vec4)));

		uint32_t indexOff = static_cast<uint32_t>(rendInst->GetPageFromUniformBuffer(m->indexSize, alignof(uint32_t)));

		VKPipelineObjectCreateInfo create = {
			.drawType = 0,
			.vertexBufferIndex = rendInst->GetMainBufferIndex(),
			.vertexBufferOffset = vertexOff,
			.vertexCount = m->vertexCount,
			.indirectDrawBuffer{},
			.indirectDrawOffset = ~0U,
			.pipelinename = rendInst->pipelinesIdentifier[genericpipeline],
			.descriptorsetid = descHandle,
			.maxDynCap = 1,
			.data = nullptr,
			.indexBufferHandle = rendInst->GetMainBufferIndex(),
			.indexBufferOffset = indexOff,
			.indexCount = m->indexCount,
		};


		rendInst->UpdateDynamicGlobalBufferAbsolute(m->GetVertexData(), m->vertexSize, vertexOff);

		rendInst->UpdateDynamicGlobalBufferAbsolute(m->GetIndexData(), m->indexSize, indexOff);

		auto ref = rendInst->vkInstance->GetLogicalDevice(rendInst->physicalIndex, rendInst->deviceIndex);

		pipelineIndex = ref->CreatePipelineObject(&create);

		VKPipelineObject* vkPipelineObject = ref->GetPipelineObject(pipelineIndex);

		vkPipelineObject->SetPerObjectData((uint32_t)offset);

		rendInst->CreateVulkanPipelineObject(vkPipelineObject);

		memoryOffset = offset;
		vertexBufferMemory = vertexOff;
		indexBufferMemory = indexOff;
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
	memoryCallback(&mat, sizeof(glm::mat4), memoryOffset);
}
