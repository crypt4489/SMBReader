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


#include "AppTypes.h"
#include "Camera.h"
#include "Exporter.h"
#include "ProgramArgs.h"
#include "GenericObject.h"
#include "SMBFile.h"
#include "TextManager.h"
#include "ThreadManager.h"

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args);
	~ApplicationLoop();

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

	void UpdateRenderables();

	void UpdateCameraMatrix();

	void WriteCameraMatrix(uint32_t frame);

	void MoveCamera(double fps);

	void CreateGlobalStorageImage();

	ProgramArgs& args;
	Semaphore queueSema, objsSema;
	std::queue<std::vector<std::any>> commands;
	std::unordered_map<std::string, std::function<void(std::vector<std::any>)>> commandMap;
	std::vector<GenericObject*> renderables;
	bool running, cleaned;
	WindowManager* mainWindow;
	Text* text1, * text2;
	size_t globalBufferLocation;
	std::function<void(void*, size_t, size_t)> gMemoryCallback;
	Camera c;
	EntryHandle storageBuffer;
	int instanceAlloc;
	std::array<glm::mat4, 64*64> instanceMatrices;

	enum DIRS {
		RIGHT = 0,
		LEFT = 1,
		FORWARD = 2,
		BACK = 3,
		ROTATEYRIGHT = 4,
		PITCHUP = 5,
		ROTATEYLEFT = 6,
		PITCHDOWN = 7,
		MAXDIRS
	};

	std::array<bool, MAXDIRS> camMovements;
};

extern ApplicationLoop* loop;