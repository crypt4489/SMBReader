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



enum class DebugDrawType
{
	DEBUGBOX = 1,
	DEBUGSPHERE = 2,
};

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

	void LoadThreadedWrapper(std::string& file);


	void FindWords(std::string words, std::vector<std::string>& out);

	void UpdateCameraMatrix();

	void WriteCameraMatrix(uint32_t frame);

	bool MoveCamera(double fps);

	void CreateTexturePools();

	EntryHandle GetPoolIndexByFormat(ImageFormat format);

	void LoadSMBFile(SMBFile& file);

	void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file);

	void SetPositonOfMesh(int meshIndex, const Vector3f& pos);

	void SetPositionOfGeometry(int geomIndex, const Vector3f& pos);

	void CreateCrateObject();

	int CreateMeshHandle(
		void* vertexData, void* indexData,
		int vertexFlags, int vertexCount, int vertexStride,
		int indexStride, int indexCount,
		AxisBox& box, Sphere& sphere,
		int vertexAlloc, int indexAlloc
		
	);

	int CreateMaterial(
		int flags,
		int* texturesIDs,
		int textureCount,
		const Vector4f& color
		);

	int CreateSphereDebugStruct(const Sphere& sphere, uint32_t count, const Vector4f& scale, const Vector4f& color);
	int CreateSphereDebugStruct(const Vector4f& minExtent, const Vector4f& maxExtent, uint32_t count, const Vector4f& scale, const Vector4f& color);
	int CreateSphereDebugStruct(const AxisBox& box, uint32_t count, const Vector4f& scale, const Vector4f& color);

	int CreateSphereDebugStruct(const Vector3f& center, float r, uint32_t count, const Vector4f& scale, const Vector4f& color);

	int CreateAABBDebugStruct(const AxisBox& box, const Vector4f &scale, const Vector4f& color);
	int CreateAABBDebugStruct(const Vector4f& boxMin, const Vector4f& boxMax, const Vector4f& scale, const Vector4f& color);
	int CreateAABBDebugStruct(const Vector3f& center, const Vector4f& halfExtents, const Vector4f& scale, const Vector4f& color);

	int AddMaterialToDeviceMemory(int count, int* ids);
	int CreateRenderable(Matrix4f& mat, int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount);

	

	ProgramArgs& args;
	Semaphore queueSema;
	std::queue<std::vector<std::string>> commands;
	bool running, cleaned;
	WindowManager* mainWindow = nullptr;
	
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