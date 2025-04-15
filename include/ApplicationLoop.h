#pragma once

#include <any>
#include <chrono>
#include <format>
#include <functional>
#include <optional>
#include <queue>
#include <string>
#include <syncstream>
#include <unordered_map>



#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include "AppTypes.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "GenericObject.h"
#include "RenderInstance.h"
#include "SMBFile.h"
#include "TextManager.h"
#include "ThreadManager.h"
#include "VKRenderGraph.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args) : 
		args(_args), 
		queueSema(Semaphore()), 
		objsSema(Semaphore()), 
		running(true),
		cleaned(false)
	{ 
		Execute(); 
	}
	~ApplicationLoop() { if (!cleaned) {   CleanupRuntime(); } delete mainWindow; }
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

			commandMap["load"]({ args.inputFile.string() });

			int i = 0, j = 1;

			LARGE_INTEGER startTime;
			LARGE_INTEGER currentTime;
			LARGE_INTEGER frequency;

			uint64_t frameCounter = 0;
			double FPS = 0.0f;

			auto fps = [&frameCounter, &currentTime, &startTime, &frequency, &FPS]()
				{
					double elapsed;
					QueryPerformanceCounter(&currentTime);

					elapsed = static_cast<double>((currentTime.QuadPart - startTime.QuadPart)) / frequency.QuadPart;

					if (elapsed >= 1.0) {
						FPS = static_cast<double>(frameCounter) / elapsed;
						frameCounter = 0;
						QueryPerformanceCounter(&startTime);
					}
				};

			QueryPerformanceFrequency(&frequency);
			QueryPerformanceCounter(&startTime);


			

			while (running)
			{
				std::string base = std::string("FPS : ");
				std::string newstring = base + std::to_string(FPS);
			    size_t stringLoc = base.size()-1;
				text1->UpdateText(newstring);
				TextManager::UpdateVertexBuffer(text1, stringLoc);

				//std::cout << newstring  << "\n";

				if (mainWindow->ShouldCloseWindow()) break;

				glm::mat4 proj = glm::perspective(glm::radians(45.0f), rend->GetSwapChainWidth() / (float)rend->GetSwapChainHeight(), 0.1f, 10000.0f);

				glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 55.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

				objsSema.Wait();

				auto index = rend->BeginFrame();
				if (index == 0xFFFFFFFF) break;
				VkCommandBuffer cb = rend->GetCurrentCommandBuffer();
				auto frameNum = rend->GetCurrentFrame();
				
				graph->DrawScene(cb, frameNum, vkpipes,view, proj);

				rend->SubmitFrame(index);

				objsSema.Notify();

				ProcessCommands();

				ThreadManager::ASyncThreadsDone();

				fps();

				frameCounter++;
			}
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

		graph = new VKRenderGraph(rend->GetRenderPass(), rend->GetMainRenderPassCache(), rend->GetDescriptorSetCache());

		TextManager::CreateFontTextManager("text.bmp", "text.dat");

		std::string name = "FPS : ";

		text1 = new Text(name, *TextManager::fonts, 0.0f, 0.05f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), name.size() + 25);

		//text2 = new Text(name, *TextManager::fonts, 0.5f, 0.75f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), name.size() + 15);

		TextManager::UploadToVertexBuffer(text1);

		graph->CreateUniformBuffers(rend, rend->MAX_FRAMES_IN_FLIGHT);

		graph->CreateRenderPassDescriptorSet(
			rend->GetVulkanDevice(),
			rend->GetDescriptorPool(),
			rend->GetDescriptorLayoutCache(),
			rend->MAX_FRAMES_IN_FLIGHT);

		//TextManager::UploadToVertexBuffer(text2);
	}

	void CleanupRuntime()
	{
		rend->WaitOnQueues();

		for (auto renderable : renderables)
		{
			delete renderable;
		}

		delete graph;

		renderables.clear();
		
		delete text1;

		delete text2;

		//graph->DestroyRenderGraph(rend->GetVulkanDevice());

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

		GenericObject* obj = new GenericObject(SMB, RenderingBackend::VULKAN, 0);

		glm::mat4 identity = glm::identity<glm::mat4>();

		//glm::mat4 identity = glm::zero<glm::mat4>();;

		obj->SetMatrix(identity);
		
		SemaphoreGuard lock(objsSema);

		renderables.push_back(obj);

		vkpipes.push_back(obj->GetPipelineObject());
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
	Semaphore queueSema, objsSema;
	std::queue<std::vector<std::any>> commands;
	std::unordered_map<std::string, std::function<void(std::vector<std::any>)>> commandMap;
	std::vector<GenericObject*> renderables;
	std::vector<VKPipelineObject*> vkpipes;
	bool running, cleaned;
	WindowManager* mainWindow;
	RenderInstance* rend;
	Text *text1, *text2;
	VKRenderGraph* graph;
};

