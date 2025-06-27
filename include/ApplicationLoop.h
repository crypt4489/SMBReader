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
	ApplicationLoop(ProgramArgs& _args);
	~ApplicationLoop();
private:

	void InitializeCommandMap();

	void Execute();

	void InitializeRuntime();

	void CleanupRuntime();

	void ProcessCommands();

	void AddCommandTS(std::vector<std::any>& com);

	void SetRunning(bool set = false);

	void LoadObject(const std::string& file);

	void LoadThreadedWrapper(const std::string file);

	void LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string& file);

	void FindWords(std::string words, std::vector<std::any>& out);

	void ScanSTDIN(std::stop_token stoken);

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

