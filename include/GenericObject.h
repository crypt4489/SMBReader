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
			
			vkPipelineObject = new VKPipelineObject(
				genericpipeline, 
				&vertexCount, 
				VK_NULL_HANDLE,
				0u
			);
			
			auto rendInst = VKRenderer::gRenderInstance;
			uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;
			auto dlc = rendInst->GetDescriptorLayoutCache();
			auto dsc = rendInst->GetDescriptorSetCache();
			auto dl = dlc->GetLayout("genericobject");
			DescriptorSetBuilder dsb{};
			VkBuffer gdb;
			size_t gdbBufferSize, gdbOffset;
			std::tie(gdb, std::ignore, gdbOffset, gdbBufferSize) = rendInst->GetDynamicBuffer();
			dsb.AllocDescriptorSets(rendInst->GetVulkanDevice(), rendInst->GetDescriptorPool(), dl, frames);
			dsb.AddDynamicUniformBuffer(rendInst->GetVulkanDevice(), gdb, gdbBufferSize, 0, frames, gdbOffset);
			dsb.AddPixelShaderImageDescription(rendInst->GetVulkanDevice(), textures[0].vkImpl->imageView, textures[0].vkImpl->sampler, 1, frames);
			dsc->AddDesciptorSet(genericpipeline, dsb.descriptorSets);
			vkPipelineObject->SetPerObjectData(&mat, sizeof(glm::mat4), static_cast<uint32_t>(objectIndex * sizeof(glm::mat4)));
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

