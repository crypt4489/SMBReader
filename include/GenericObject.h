#pragma once

#include <vector>
#include <functional>

#include "AppTexture.h"
#include "AppTypes.h"
#include "VKDescriptorSetCache.h"
#include "GenericObject.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "VertexTypes.h"
#include "VKPipelineObject.h"


class GenericObject
{
public:
	GenericObject(const SMBFile &file, RenderingBackend be, size_t _oi) : vkPipelineObject(nullptr), objectIndex(_oi)
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
			auto dlc = rendInst->GetDescriptorLayoutCache();
			auto dsc = rendInst->GetDescriptorSetCache();
			auto dl = dlc->GetLayout("genericobject");
			DescriptorSetBuilder dsb{};
			uint32_t offset = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, 16);
			dsb.AllocDescriptorSets(rendInst->GetVulkanDevice(), rendInst->GetDescriptorPool(), dl, frames);
			dsb.AddDynamicUniformBuffer(rendInst->GetVulkanDevice(), rendInst->GetDynamicUniformBuffer(), sizeof(glm::mat4), 0, frames, offset);
			dsb.AddPixelShaderImageDescription(rendInst->GetVulkanDevice(), rendInst->GetImageView(textures[0].vkImpl->viewIndex), rendInst->GetSampler(textures[0].vkImpl->samplerIndex), 1, frames);
			dsc->AddDesciptorSet(genericpipeline, dsb.descriptorSets);
			
			vkPipelineObject = new VKPipelineObject(
				genericpipeline,
				~0U,
				~0U,
				&vertexCount
			);
			
			vkPipelineObject->SetPerObjectData(&mat, sizeof(glm::mat4), offset);
		}
	}

	~GenericObject()
	{
		if (vkPipelineObject) delete vkPipelineObject;
		textures.clear();
	}

	VKPipelineObject* GetPipelineObject() const
	{
		return vkPipelineObject;
	}

	void SetMatrix(glm::mat4& f)
	{
		mat = f;
	}

	void SetFunctionPointer(std::function<void(GenericObject*)> f)
	{
		updateObject = [f, this]() {
			f(this);
		};
	}

	void CallUpdate()
	{
		updateObject();
	}

protected:
	VKPipelineObject* vkPipelineObject;
	size_t vertexCount;
	std::vector<AppTexture> textures;
	size_t objectIndex;
	glm::mat4 mat;
	std::function<void()> updateObject;
};

