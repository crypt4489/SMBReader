#pragma once

#include <vector>


#include "AppTexture.h"
#include "AppTypes.h"
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
			vkPipelineObject = new VKPipelineObject(
				"2dimage", 
				&vertexCount, 
				VK_NULL_HANDLE,
				VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT
			);
			vkPipelineObject->AddPixelShaderImageDescription(textures[0].vkImpl->imageView, textures[0].vkImpl->sampler, 0);
			vkPipelineObject->CreatePipelineObject(VKRenderer::gRenderInstance->GetVulkanDevice());
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

