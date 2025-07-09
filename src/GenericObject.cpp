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
		dsb.AddDynamicUniformBuffer(rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, offset);
		dsb.AddPixelShaderImageDescription(rendInst->GetImageView(textures[0].vkImpl->viewIndex), rendInst->GetSampler(textures[0].vkImpl->samplerIndex), 1, frames);
		dsb.AddDescriptorsToCache(genericpipeline);

		vkPipelineObject = new VKPipelineObject(
			genericpipeline,
			~0U,
			~0U,
			&vertexCount,
			rendInst->mainRenderTarget
		);

		vkPipelineObject->SetPerObjectData(&mat, sizeof(glm::mat4), offset);
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
}
