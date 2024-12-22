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
			vkPipelineObject = new VKPipelineObject();
			vkPipelineObject->AddShader("typicaltextured.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			vkPipelineObject->AddShader("typicaltextured.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			vkPipelineObject->AddPixelShaderImageDescription(textures[0].vkImpl->imageView, textures[0].vkImpl->sampler, 0);
			vkPipelineObject->CreatePipelineObject();
		}
	}

	~GenericObject()
	{
		if (vkPipelineObject) delete vkPipelineObject;
		textures.clear();
	}

	void Draw(VkCommandBuffer cb, uint32_t currentFrame)
	{
		vkPipelineObject->Draw(cb, 4, currentFrame);
	}

protected:
	VKPipelineObject* vkPipelineObject;

	std::vector<AppTexture> textures;
};

