#pragma once

#include <functional>
#include <syncstream>

#include "AppTexture.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "ThreadManager.h"
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
		SMBFile mainSMB(args.inputFile);
		if (args.justexport)
		{
			FileManager::SetFileCurrentDirectory(FileManager::ExtractFileNameFromPath(args.inputFile.string()));
			Exporter::ExportChunksFromFile(mainSMB);
		}
		else
		{
			auto daemon = std::mem_fn(&ApplicationLoop::ScanSTDIN);
			ThreadManager::LaunchBackgroundThread(std::bind(daemon, this, std::placeholders::_1));

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

			ThreadManager::DestroyThreadManager();
		}
	}

	void ScanSTDIN(std::stop_token stoken)
	{
		std::string in;
		std::getline(std::cin, in);
		while (!stoken.stop_requested())
		{
			std::getline(std::cin, in);
			std::osyncstream(std::cout) << "You wrote this : " << in << "!\n";
			
			//std::this_thread::sleep_for(std::chrono::seconds(1));
			std::osyncstream(std::cout) << "come on" << stoken.stop_requested() << "\n";
		}
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
};

