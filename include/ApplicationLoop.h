#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <queue>
#include <string>
#include <syncstream>


#include "AppTypes.h"
#include "Camera.h"
#include "Exporter.h"
#include "ProgramArgs.h"

#include "SMBFile.h"
#include "TextManager.h"
#include "TextureDictionary.h"
#include "ThreadManager.h"


class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args);
	~ApplicationLoop();

	void ExecuteCommands(const std::string& command, const std::vector<std::string>& args);

	void Execute();

	void InitializeRuntime();

	void CleanupRuntime();

	void ProcessCommands();

	void AddCommandTS(std::vector<std::string>& com);

	void SetRunning(bool set = false);

	void LoadObject(const std::string& file);

	void LoadThreadedWrapper(const std::string file);

	void LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string file);

	void FindWords(std::string words, std::vector<std::string>& out);

	void ScanSTDIN(std::stop_token stoken);

	void UpdateCameraMatrix();

	void WriteCameraMatrix(uint32_t frame);

	bool MoveCamera(double fps);

	void CreateGlobalStorageImage();

	void CreateTexturePools();

	int GetPoolIndexByFormat(ImageFormat format);

	void LoadSMBFile(SMBFile& file);

	void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file);

	void SetPositonOfMesh(int meshIndex, const Vector3f& pos);

	void SetPositionOfGeometry(int geomIndex, const Vector3f& pos);

	void UpdateThisThing();

	ProgramArgs& args;
	Semaphore queueSema;
	std::queue<std::vector<std::string>> commands;
	bool running, cleaned;
	WindowManager* mainWindow;
	
	Camera c;

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