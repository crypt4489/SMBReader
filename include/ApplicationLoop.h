#pragma once

#include "AppTexture.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "VertexTypes.h"
#include "VKPipelineObject.h"
#include "VKRenderLoop.h"

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args) : vkLoop(new VKRenderLoop()), args(_args) { Execute(); }
	~ApplicationLoop() { delete vkLoop; }
private:

	void Execute()
	{
		SMBFile mainSMB{};
		if (args.justexport)
		{
			FileManager::SetFileCurrentDirectory(FileManager::ExtractFileNameFromPath(args.inputFile.string()));
			mainSMB.LoadFile(args.inputFile);
			Exporter::ExportChunksFromFile(mainSMB);
		}
		else
		{
			mainSMB.LoadFile(args.inputFile);

			int j = 0;
			auto& ref = mainSMB.chunks;
			for (int i = 0; i < ref.size(); i++)
			{
				if (ref[i].chunkType == TEXTURE)
				{
					j = i;
					break;
				}
			}

			auto rend = ::VK::Renderer::gRenderInstance = new RenderInstance();
			rend->CreateVulkanRenderer();


			VKPipelineObject* pipe = new VKPipelineObject();
			AppTexture* tex = new AppTexture(mainSMB, ref[j]);


			pipe->AddShader("typicaltextured.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			pipe->AddShader("typicaltextured.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			pipe->AddPixelShaderImageDescription(tex->vkImpl->imageView, tex->vkImpl->sampler, 0);
			pipe->CreatePipelineObject();

			vkLoop->AddPipeline(pipe);
			vkLoop->RenderLoop(rend);

			delete tex;
			delete pipe;

			rend->DestroyVulkanRenderer();

			delete ::VK::Renderer::gRenderInstance;
		}
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
};

