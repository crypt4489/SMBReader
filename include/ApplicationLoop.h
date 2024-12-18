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

		//fdMode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
		fdMode |= ENABLE_PROCESSED_INPUT;

		SetConsoleMode(stdInHandle, fdMode);

		DWORD numberOfBytesRead;
		DWORD events;
		INPUT_RECORD record;

		char inputBuffer[1024];

		std::this_thread::sleep_for(std::chrono::seconds(2));

		std::osyncstream(std::cout) << "Ready for commands... \n" << "Hit enter and then write command > ";

		while (!stoken.stop_requested())
		{

			DWORD ret = WaitForSingleObject(stdInHandle, 500);

			if (ret == WAIT_TIMEOUT) continue;

			BOOL success = ReadConsoleInput(stdInHandle, &record, 1, &events);

			if (!success) {
				std::osyncstream(std::cerr) << "Cannot get ReadConsoleInput\n"; 
				break;
			}

			switch (record.EventType) {
			case KEY_EVENT:
				// Handle KEY_EVENT
				if (record.Event.KeyEvent.bKeyDown) {
					if (record.Event.KeyEvent.uChar.AsciiChar == VK_RETURN)
					{
						break;
					}
				}
			default:
				continue;
			}

			success = ReadFile(stdInHandle, inputBuffer, 1024, &numberOfBytesRead, NULL);

			if (!success)
			{
				std::osyncstream(std::cerr) << "Cannot get ReadFile from stdinput\n";
				break;
			}

			if (numberOfBytesRead <= 2)
				continue;

			std::string output(inputBuffer, numberOfBytesRead);
			std::osyncstream(std::cout) << "Hit enter and then write command > ";
		}
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
};

