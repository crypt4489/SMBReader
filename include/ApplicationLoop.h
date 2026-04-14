#pragma once

#include <array>
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

class ApplicationLoop
{
public:
	ApplicationLoop(ProgramArgs& _args);
	~ApplicationLoop();

	void ExecuteCommands(const StringView& command, int argCount);

	void Execute();

	void InitializeRuntime();

	void CleanupRuntime();

	void ProcessCommands();

	void AddCommandTS(int wordCount);

	void SetRunning(bool set = false);

	void LoadObject(const StringView& file);

	void LoadThreadedWrapper(StringView& file);

	int FindWords(const char* words, int charCount);

	void UpdateCameraMatrix();

	void WriteCameraMatrix(uint32_t frame);

	bool MoveCamera(double fps);

	void CreateTexturePools();

	EntryHandle GetPoolIndexByFormat(ImageFormat format);

	void LoadSMBFile(SMBFile& file);

	void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file, int* textureHandles, int textureBase);

	void SetPositionOfGeometry(int geomIndex, const Vector3f& pos);

	void CreateCrateObject();

	int CreateMeshHandle(
		void* vertexData, void* indexData,
		int vertexFlags, int vertexCount, int vertexStride,
		int indexStride, int indexCount,
		Sphere& sphere,
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
	int CreateRenderable(const Matrix4f& mat, int geomIndex, int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount);

	void CreateCornerWall(float width, float height, float xDiv, float yDiv);

	void CreateSkyBox();

	void CreateMSAAPostFullScreen();

	void RecreateFrameGraphAttachments(uint32_t width, uint32_t height);

	void CreateMeshHandle(
		int meshIndex,
		void* vertexData, void* indexData,
		int vertexFlags, int vertexCount, int vertexStride,
		int indexStride, int indexCount,
		Sphere& sphere,
		int vertexAlloc, int indexAlloc
	);

	int CreateGPUGeometryDetails(const AxisBox& minMaxBox);
	int CreateGPUGeometryRenderable(const Matrix4f& matrix, int geomDesc, int renderableStart, int renderableCount);

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
