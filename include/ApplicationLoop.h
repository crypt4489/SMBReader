#pragma once

#include <functional>
#include <unordered_map>
#include <queue>
#include <syncstream>

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include "AppTypes.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "ThreadManager.h"
#include "VKRenderLoop.h"

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args) : vkLoop(nullptr), 
		args(_args), 
		queueSema(Semaphore()), 
		objsSema(Semaphore()), 
		running(true) 
	{ 
		commandMap["end"] = std::bind(std::mem_fn(&ApplicationLoop::SetRunning), this, false);

		Execute(); 
	}
	~ApplicationLoop() { delete vkLoop; delete mainWindow; }
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
			
			ThreadManager::LaunchBackgroundThread(
				std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN), 
					this, std::placeholders::_1));


			auto rend = ::VK::Renderer::gRenderInstance = new RenderInstance();

			mainWindow = new WindowManager();

			mainWindow->CreateWindowInstance();
			
			rend->CreateVulkanRenderer(mainWindow);
			
			vkLoop = new VKRenderLoop(std::ref(*rend));
			
			renderables.push_back(new GenericObject(mainSMB, RenderingBackend::VULKAN));

			while (running)
			{
				if (mainWindow->ShouldCloseWindow()) break;

				vkLoop->RenderLoop(std::ref(renderables));

				ProcessCommands();
			}

			rend->WaitOnQueues();

			for (auto renderable : renderables)
			{
				delete renderable;
			}


			rend->DestroyVulkanRenderer();

			delete ::VK::Renderer::gRenderInstance;

			ThreadManager::DestroyThreadManager();
		}
	}

	void ProcessCommands()
	{
		queueSema.Wait();
		if (!commands.size()) {
			queueSema.Notify();
			return;
		}
		std::string com = std::move(commands.front());
		commands.pop();
		queueSema.Notify();
		auto mapFunc = commandMap.find(com);
		if (mapFunc == std::end(commandMap)) return;
		mapFunc->second();
	}

	void AddCommandTS(std::string& com)
	{
		SemaphoreGuard lock(std::ref(queueSema));
		commands.push(com);
	}

	void SetRunning(bool set = false)
	{
		running = set;
	}

	void ScanSTDIN(std::stop_token stoken)
	{
		HANDLE stdInHandle = GetStdHandle(STD_INPUT_HANDLE);

		if (stdInHandle == INVALID_HANDLE_VALUE)
		{
			std::osyncstream(std::cerr) << "Cannot open handle to STD INPUT\n";
			return;
		}

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

			std::string output(inputBuffer, numberOfBytesRead-2);

			this->AddCommandTS(output);

			if (output == "end") break;

			std::osyncstream(std::cout) << "Hit enter and then write command > ";
		}
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
	Semaphore queueSema, objsSema;
	std::queue<std::string> commands;
	std::unordered_map<std::string, std::function<void()>> commandMap;
	std::vector<GenericObject*> renderables;
	bool running;
	WindowManager *mainWindow;
};

