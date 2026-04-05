#pragma once

#include <chrono>
#include <format>
#include <functional>
#include <queue>
#include <string>
#include <syncstream>


#include "AppTypes.h"
#include "Camera.h"
#include "SMBFile.h"
#include "ProgramArgs.h"
#include "WindowManager.h"
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

	void ExecuteCommands(const std::string& command, int argCount);

	void Execute();

	void InitializeRuntime();

	void CleanupRuntime();

	void ProcessCommands();

	void AddCommandTS(int wordCount);

	void SetRunning(bool set = false);

	void LoadObject(const std::string& file);

	void LoadThreadedWrapper(std::string& file);


	int FindWords(std::string words);

	void UpdateCameraMatrix();

	void WriteCameraMatrix(uint32_t frame);

	bool MoveCamera(double fps);

	void CreateTexturePools();

	EntryHandle GetPoolIndexByFormat(ImageFormat format);

	void LoadSMBFile(SMBFile& file);

	void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file);

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


	int CreateMaterial(
		int flags,
		int* texturesIDs,
		int textureCount,
		const Vector4f& diffuseColor,
		const Vector4f& specularColor,
		float shininess,
		const Vector4f& emissiveColor
	);

	int CreateSphereDebugStruct(const Sphere& sphere, uint32_t count, const Vector4f& scale, const Vector4f& color);
	int CreateSphereDebugStruct(const Vector4f& minExtent, const Vector4f& maxExtent, uint32_t count, const Vector4f& scale, const Vector4f& color);
	int CreateSphereDebugStruct(const AxisBox& box, uint32_t count, const Vector4f& scale, const Vector4f& color);

	int CreateSphereDebugStruct(const Vector3f& center, float r, uint32_t count, const Vector4f& scale, const Vector4f& color);

	int CreateAABBDebugStruct(const AxisBox& box, const Vector4f &scale, const Vector4f& color);
	int CreateAABBDebugStruct(const Vector4f& boxMin, const Vector4f& boxMax, const Vector4f& scale, const Vector4f& color);
	int CreateAABBDebugStruct(const Vector3f& center, const Vector4f& halfExtents, const Vector4f& scale, const Vector4f& color);

	int AddMaterialToDeviceMemory(int count, int* ids);
	int CreateRenderable(const Matrix4f& mat, int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount);

	void CreateCornerWall(float width, float height, float xDiv, float yDiv);

	int CreateGridRenderable(int meshIndex, int materialRangeIndex, const Matrix4f& world);

	void CreateSkyBox();

	void CreateMSAAPostFullScreen();

	void RecreateFrameGraphAttachments(uint32_t width, uint32_t height);

	void CreateMeshHandle(
		int meshIndex,
		void* vertexData, void* indexData,
		int vertexFlags, int vertexCount, int vertexStride,
		int indexStride, int indexCount,
		AxisBox& box, Sphere& sphere,
		int vertexAlloc, int indexAlloc
	);

	ProgramArgs& args;
	Semaphore queueSema;
	std::queue<int> wordCounts;
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