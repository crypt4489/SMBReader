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
	vertexCount = 4;
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

	if (be == RenderingBackend::VULKAN)
	{
		std::string genericpipeline = "genericpipeline";

		auto rendInst = VKRenderer::gRenderInstance;
		uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

		DescriptorSetBuilder *dsb = rendInst->CreateDescriptorSet(rendInst->descriptorLayouts["genericobject"], frames);
		OffsetIndex offset = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, 16);
		dsb->AddDynamicUniformBuffer(rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, 0);
		dsb->AddPixelShaderImageDescription(rendInst->GetImageView(textures[0].vkImpl), rendInst->GetSampler(textures[0].vkImpl), 1, frames);
		EntryHandle descHandle = dsb->AddDescriptorsToCache();

		VKPipelineObjectCreateInfo create = {
			.drawType = 0,
			.vertexBufferIndex{},
			.vertexBufferOffset = ~0U,
			.vertexCount = static_cast<uint32_t>(vertexCount),
			.indirectDrawBuffer{},
			.indirectDrawOffset = ~0U,
			.pipelinename = rendInst->pipelinesIdentifier[genericpipeline],
			.descriptorsetid = descHandle,
			.maxDynCap = 1,
			.data = nullptr

		};

		auto ref = rendInst->vkInstance->GetLogicalDevice(rendInst->physicalIndex, rendInst->deviceIndex);

		pipelineIndex = ref->CreatePipelineObject(&create);

		VKPipelineObject* vkPipelineObject = ref->GetPipelineObject(pipelineIndex);

		vkPipelineObject->SetPerObjectData(offset);

		rendInst->CreateVulkanPipelineObject(vkPipelineObject);

		memoryOffset = std::move(offset);
	}
}

GenericObject::~GenericObject()
{
	textures.clear();
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
