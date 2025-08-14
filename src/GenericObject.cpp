#include "GenericObject.h"

#include "AppTypes.h"
#include "RenderInstance.h"
#include "VertexTypes.h"

GenericObject::GenericObject(const SMBFile& file, RenderingBackend be, size_t _oi) : vkPipelineObject(nullptr), objectIndex(_oi)
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

		DescriptorSetBuilder dsb = rendInst->CreateDescriptorSet("genericobject", frames);
		OffsetIndex offset = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, 16);
		dsb.AddDynamicUniformBuffer(rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, 0);
		dsb.AddPixelShaderImageDescription(rendInst->GetImageView(textures[0].vkImpl->viewIndex), rendInst->GetSampler(textures[0].vkImpl->samplerIndex), 1, frames);
		dsb.AddDescriptorsToCache(genericpipeline);

		VKPipelineObjectCreateInfo create = {
			.drawType = 0,
			.vertexBufferIndex = ~0U,
			.vertexBufferOffset = ~0U,
			.vertexCount = static_cast<uint32_t>(vertexCount),
			.indirectDrawBuffer = ~0U,
			.indirectDrawOffset = ~0U,
			.pipelinename = genericpipeline,
			.descriptorsetname = genericpipeline
		};

		vkPipelineObject = new VKPipelineObject(create);

		vkPipelineObject->SetPerObjectData(offset);

		rendInst->CreateVulkanPipelineObject(vkPipelineObject);

		memoryOffset = std::move(offset);
	}
}

GenericObject::~GenericObject()
{
	if (vkPipelineObject) delete vkPipelineObject;
	textures.clear();
}

VKPipelineObject* GenericObject::GetPipelineObject() const
{
	return vkPipelineObject;
}

void GenericObject::SetMatrix(glm::mat4& f)
{
	mat = f;
}

void GenericObject::SetFunctionPointer(std::function<void(GenericObject*)> f)
{
	updateObject = [f, this]() {
		f(this);
		};
}

void GenericObject::CallUpdate()
{
	updateObject();
	auto rendInst = VKRenderer::gRenderInstance;
	rendInst->UpdateDynamicGlobalBuffer(&mat, sizeof(glm::mat4), memoryOffset, rendInst->currentFrame);
}
