#pragma once

#include <functional>
#include <syncstream>

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

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
		HANDLE stdInHandle = GetStdHandle(STD_INPUT_HANDLE);

		if (stdInHandle == INVALID_HANDLE_VALUE)
		{
			std::osyncstream(std::cerr) << "Cannot open handle to STD INPUT\n";
			return;
		}

		DWORD fdMode;

		GetConsoleMode(stdInHandle, &fdMode);

		fdMode &= ~(ENABLE_MOUSE_INPUT);
		fdMode |= ENABLE_PROCESSED_INPUT;

		SetConsoleMode(stdInHandle, fdMode);

		DWORD numberOfBytesRead;
		DWORD events;
		char inputBuffer[1024];
		while (!stoken.stop_requested())
		{
			events = WaitForSingleObject(stdInHandle, 0);
			std::osyncstream(std::cerr) << "Just Polling" << events << "\n";
			if (events == WAIT_OBJECT_0)
			{
				BOOL read = ReadFile(stdInHandle, inputBuffer, 1024, &numberOfBytesRead, NULL);
				
				if (!read)
				{
					std::osyncstream(std::cerr) << "Broken stream (somehow)\n";
					break;
				}

				if (numberOfBytesRead <= 2)
					continue;

				std::osyncstream(std::cerr) << "Number of bytes read : " << numberOfBytesRead << "\n";
				std::string output(inputBuffer, numberOfBytesRead);
				std::osyncstream(std::cerr) << output;
			} 
			else if (events == WAIT_TIMEOUT)
			{
				std::osyncstream(std::cerr) << "Just Waiting\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			}
			
		}
		std::osyncstream(std::cout) << "Just Waiting here\n";
		CloseHandle(stdInHandle);
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
};

