#pragma once

#include <any>
#include <functional>
#include <optional>
#include <queue>
#include <syncstream>
#include <unordered_map>
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include "AppTypes.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "TextManager.h"
#include "ThreadManager.h"
#include "VKRenderLoop.h"

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args) : vkLoop(nullptr), 
		args(_args), 
		queueSema(Semaphore()), 
		objsSema(Semaphore()), 
		running(true),
		cleaned(false)
	{ 
		Execute(); 
	}
	~ApplicationLoop() { if (!cleaned) CleanupRuntime();  delete vkLoop; delete mainWindow; }
private:

	void InitializeCommandMap()
	{
		commandMap["end"] = [this](std::optional<std::vector<std::any>> args)
			{
				SetRunning(false);
			};

		commandMap["load"] = [this](std::optional<std::vector<std::any>> args)
			{
				LoadThreadedWrapper(std::any_cast<std::string>(args->at(0)));
			};
	}

	void Execute()
	{
		
		if (args.justexport)
		{
			SMBFile mainSMB(args.inputFile);
			FileManager::SetFileCurrentDirectory(FileManager::ExtractFileNameFromPath(args.inputFile.string()));
			Exporter::ExportChunksFromFile(mainSMB);
			cleaned = true;
		}
		else
		{
			InitializeCommandMap();

			InitializeRuntime();

			//commandMap["load"]({ args.inputFile.string() });

			while (running)
			{
				if (mainWindow->ShouldCloseWindow()) break;

				objsSema.Wait();

				vkLoop->RenderLoop(std::ref(renderables));

				objsSema.Notify();

				ProcessCommands();

				ThreadManager::ASyncThreadsDone();
			}

			CleanupRuntime();
		}
	}

	void InitializeRuntime()
	{
		ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN),
				this, std::placeholders::_1));


		VKRenderer::gRenderInstance = new RenderInstance();

		rend = VKRenderer::gRenderInstance;

		mainWindow = new WindowManager();

		mainWindow->CreateWindowInstance();

		rend->CreateVulkanRenderer(mainWindow);

		TextManager::CreateFontTextManager("text.bmp", "text.dat");

		std::string name = "NEEDS TO BE BETTER";

		Text text(name, *TextManager::fonts, 0.25f, 0.25f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

		Text text2(name, *TextManager::fonts, 0.5f, 0.75f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

		TextManager::UpdateVertexBuffer(text);

		TextManager::UpdateVertexBuffer(text2);

		vkLoop = new VKRenderLoop(std::ref(*rend));
	}

	void CleanupRuntime()
	{
		rend->WaitOnQueues();

		for (auto renderable : renderables)
		{
			delete renderable;
		}

		renderables.clear();

		TextManager::DestroyTextManager();

		ThreadManager::DestroyThreadManager();

		rend->DestroyVulkanRenderer();

		delete VKRenderer::gRenderInstance;

		cleaned = true;
	}

	void ProcessCommands()
	{
		queueSema.Wait();
		if (!commands.size()) {
			queueSema.Notify();
			return;
		}
		std::vector<std::any> com = std::move(commands.front());
		commands.pop();
		queueSema.Notify();
		if (!com.size()) { std::cerr << "what are you doing\n"; return; }
		auto mapFunc = commandMap.find(std::any_cast<std::string>(com[0]));
		if (mapFunc == std::end(commandMap)) return;
		mapFunc->second({com.begin()+1, com.end()});
	}

	void AddCommandTS(std::vector<std::any>& com)
	{
		SemaphoreGuard lock(std::ref(queueSema));
		commands.push(std::move(com));
	}

	void SetRunning(bool set = false)
	{
		running = set;
	}

	void LoadObject(const std::string& file)
	{
		SMBFile SMB(file);

		GenericObject* obj = new GenericObject(SMB, RenderingBackend::VULKAN);

		SemaphoreGuard lock(objsSema);

		renderables.push_back(obj);
	}

	void LoadThreadedWrapper(const std::string file)
	{
		ThreadManager::LaunchASyncThread(std::bind(std::mem_fn(&ApplicationLoop::LoadObjectThreaded), this, std::placeholders::_1, file));
	}

	void LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string& file)
	{
		this->LoadObject(file);

		flag->store(true);
	}

	void FindWords(std::string words, std::vector<std::any> &out)
	{
		size_t size = words.length();
		int i = 0, j = 1;
		while (j < size) {
			if (words[j] == 0x20)
			{
				out.push_back(words.substr(i, (j - i)));
				j++;
				i = j;
			}
			j++;
		}
		out.push_back(words.substr(i, (j - i)));
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

			std::vector<std::any> comandargs{};

			FindWords(output, comandargs);

			this->AddCommandTS(comandargs);

			if (output == "end") break;

			std::osyncstream(std::cout) << "Hit enter and then write command > ";
		}
	}

	ProgramArgs& args;
	VKRenderLoop* vkLoop;
	Semaphore queueSema, objsSema;
	std::queue<std::vector<std::any>> commands;
	std::unordered_map<std::string, std::function<void(std::vector<std::any>)>> commandMap;
	std::vector<GenericObject*> renderables;
	bool running, cleaned;
	WindowManager* mainWindow;
	RenderInstance* rend;
};

