#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
static void Rotate(GenericObject* obj)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	obj->mat = glm::rotate(glm::mat4(1.0f), time * glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}

static ApplicationLoop* loop;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

ApplicationLoop::ApplicationLoop(ProgramArgs& _args) :
	args(_args),
	queueSema(Semaphore()),
	objsSema(Semaphore()),
	running(true),
	cleaned(false)
{
	loop = this;
	Execute();
}
ApplicationLoop::~ApplicationLoop() { 
	if (!cleaned) 
	{ 
		CleanupRuntime(); 
	} 
	delete mainWindow; 
}

void ApplicationLoop::InitializeCommandMap()
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

void ApplicationLoop::Execute()
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
					std::cout << FPS << std::endl;
					frameCounter = 0;
					QueryPerformanceCounter(&startTime);
				}
			};

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startTime);




		while (running)
		{
			//std::string base = std::string("FPS : ");
			//std::string newstring = base + std::to_string(FPS);
			//size_t stringLoc = base.size() - 1;
			//text1->UpdateText(newstring);
			//TextManager::UpdateVertexBuffer(text1, stringLoc);

		

			if (mainWindow->ShouldCloseWindow()) break;

			glfwPollEvents();

			UpdateRenderables();

			auto index = VKRenderer::gRenderInstance->BeginFrame();

			if (index != ~0ui32) {
				VKRenderer::gRenderInstance->SubmitFrame(index);
			}

			ProcessCommands();

			ThreadManager::ASyncThreadsDone();

			fps();

			frameCounter++;
		}
	}
}

void ApplicationLoop::UpdateRenderables()
{
	SemaphoreGuard guard(objsSema);

	for (auto& ref : renderables)
	{
		ref->CallUpdate();
	}
}

void ApplicationLoop::UpdateCameraMatrix(const glm::vec3& movestatement)
{
	c.MoveCamera(movestatement);

	c.UpdateCamera();

	WriteCameraMatrix(RenderInstance::MAX_FRAMES_IN_FLIGHT);
}

void ApplicationLoop::WriteCameraMatrix(uint32_t frame)
{
	if (frame >= RenderInstance::MAX_FRAMES_IN_FLIGHT)
	{
		VKRenderer::gRenderInstance->UpdateDynamicGlobalBufferForAllFrames(&c.View, sizeof(glm::mat4) * 2, globalBufferLocation);
	}
	else
	{
		VKRenderer::gRenderInstance->UpdateDynamicGlobalBufferCurrent(&c.View, sizeof(glm::mat4) * 2, globalBufferLocation);
	}
}


void ApplicationLoop::InitializeRuntime()
{
	ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN),
				this, std::placeholders::_1));


	VKRenderer::gRenderInstance = new RenderInstance();

	mainWindow = new WindowManager();

	mainWindow->CreateWindowInstance();

	glfwSetKeyCallback(mainWindow->GetWindow(), key_callback);

	VKRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow);

	gMemoryCallback = [this](void* _d, size_t _si, size_t _off)
		{
			VKRenderer::gRenderInstance->UpdateDynamicGlobalBufferCurrent(_d, _si, _off);
		};

	//TextManager::CreateFontTextManager("text.bmp", "text.dat");

	//std::string name = "FPS : ";

	//text1 = new Text(name, *TextManager::fonts, 0.0f, 0.05f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), name.size() + 25);

	//TextManager::UploadToVertexBuffer(text1);


	globalBufferLocation = VKRenderer::gRenderInstance->CreateRenderGraph(sizeof(glm::mat4) * 2 * VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT, 16);

	c.CamLookAt(glm::vec3(0.0f, 0.0f, 55.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(VKRenderer::gRenderInstance->GetSwapChainWidth() / (float)VKRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, glm::radians(45.0f));


	WriteCameraMatrix(VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

}


static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
		loop->UpdateCameraMatrix(glm::vec3(1, 0, 0));
	if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
		loop->UpdateCameraMatrix(glm::vec3(-1, 0, 0));
	if (key == GLFW_KEY_UP && action == GLFW_PRESS)
		loop->UpdateCameraMatrix(glm::vec3(0, 0, 1));
	if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
		loop->UpdateCameraMatrix(glm::vec3(0, 0, -1));
}

void ApplicationLoop::CleanupRuntime()
{
	VKRenderer::gRenderInstance->WaitOnRender();

	for (auto renderable : renderables)
	{
		delete renderable;
	}

	renderables.clear();

	delete text1;

	delete text2;

	TextManager::DestroyTextManager();

	ThreadManager::DestroyThreadManager();

	delete VKRenderer::gRenderInstance;

	cleaned = true;
}

void ApplicationLoop::ProcessCommands()
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
	mapFunc->second({ com.begin() + 1, com.end() });
}



void ApplicationLoop::AddCommandTS(std::vector<std::any>& com)
{
	SemaphoreGuard lock(std::ref(queueSema));
	commands.push(std::move(com));
}

void ApplicationLoop::SetRunning(bool set)
{
	running = set;
}

void ApplicationLoop::LoadObject(const std::string& file)
{
	SMBFile SMB(file);

	GenericObject* obj = new GenericObject(SMB, RenderingBackend::VULKAN, 0);

	glm::mat4 identity = glm::identity<glm::mat4>();

	obj->SetMatrix(identity);

	obj->SetPerObjectCallback(Rotate);

	obj->SetPerObjectMemoryCallback(gMemoryCallback);

	SemaphoreGuard lock(objsSema);

	renderables.push_back(obj);

	for (int i = 0; i < RenderInstance::MAX_FRAMES_IN_FLIGHT; i++)
		VKRenderer::gRenderInstance->InvalidateRecordBuffer(i);
}

void ApplicationLoop::LoadThreadedWrapper(const std::string file)
{
	ThreadManager::LaunchASyncThread(std::bind(std::mem_fn(&ApplicationLoop::LoadObjectThreaded), this, std::placeholders::_1, file));
}

void ApplicationLoop::LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string& file)
{

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	this->LoadObject(file);

	flag->store(true);
}

void ApplicationLoop::FindWords(std::string words, std::vector<std::any>& out)
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

void ApplicationLoop::ScanSTDIN(std::stop_token stoken)
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

		std::string output(inputBuffer, numberOfBytesRead - 2);

		std::vector<std::any> comandargs{};

		FindWords(output, comandargs);

		this->AddCommandTS(comandargs);

		if (output == "end") break;

		std::osyncstream(std::cout) << "Hit enter and then write command > ";
	}
}