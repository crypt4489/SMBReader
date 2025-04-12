#pragma once

#include <vector>


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
	GenericObject(const SMBFile &file, RenderingBackend be) : vkPipelineObject(nullptr)
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
			
			vkPipelineObject = new VKPipelineObject(
				genericpipeline, 
				&vertexCount, 
				VK_NULL_HANDLE
			);
			
			auto rendInst = VKRenderer::gRenderInstance;
			uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;
			auto dlc = rendInst->GetDescriptorLayoutCache();
			auto dsc = rendInst->GetDescriptorSetCache();
			auto dl = dlc->GetLayout("oneimage");
			DescriptorSetBuilder dsb{};
			dsb.AllocDescriptorSets(rendInst->GetVulkanDevice(), rendInst->GetDescriptorPool(), dl, frames);
			dsb.AddPixelShaderImageDescription(rendInst->GetVulkanDevice(), textures[0].vkImpl->imageView, textures[0].vkImpl->sampler, 0, frames);
			dsc->AddDesciptorSet(genericpipeline, dsb.descriptorSets);
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

protected:
	VKPipelineObject* vkPipelineObject;
	size_t vertexCount;
	std::vector<AppTexture> textures;
};

