#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"


#include "SMBTexture.h"
#include "AppAllocator.h"


ApplicationLoop* loop;

enum VertexComponents
{
	POSITION = 1,
	TEXTURES1 = 2,
	TEXTURES2 = 4,
	TEXTURES3 = 8,
	NORMAL = 16,
	BONES2 = 32,
	COLOR = 64,
	COMPRESSED = 0x80000000,
};

std::array<std::string, 4> commandsStrings =
{
	"end",
	"load",
	"positionm",
	"positiong",
};

enum MaterialFlags
{
	ALBEDOMAPPED = 4,
	NORMALMAPPED = 8,
	VERTEXNORMAL = 16
};

struct Material
{
	int materialFlags;
	int unused1;
	int unused2;
	int unused3;
	Vector4f albedoColor;
	Vector4ui textureHandles; // y - normalMapCoordinates // x- albedo
};

struct Renderable
{
	int meshIndex;
	int lightCount; 
	int instanceCount;
	int pad1;
	int lightIndices[4];
	int materialStart;
	int materialCount;
	int blendLayersStart;
	int blendLayerCount;
	Matrix4f transform;
};

struct Handles
{
	int vertexFlags;
	int stride;
	int indexCount;
	int firstIndex;
	int vertexByteOffset;
	int pad1;
	int pad2;
	int pad3;
	AxisBox minMaxBox;
	Sphere sphere;
};

struct IndirectDrawData
{
	int commandBufferAlloc;
	int commandBufferCount;
	int commandBufferSize;
	int commandBufferCountAlloc;
	int indirectDrawDescriptor;
	int indirectDrawPipeline;
	int indirectCullDescriptor;
	int indirectCullPipeline;
	int indirectGlobalIDsAlloc;
	//EntryHandle indirectGlobalIDsView;
};

struct WorldSpaceGPUPartition
{
	int totalElementsCount; //total subdivisions
	int totalSumsNeeded;
	int deviceOffsetsAlloc;
	int deviceSumsAlloc;
	int deviceCountsAlloc;
	int prefixSumDescriptors;
	int prefixSumPipeline;
	int sumAfterDescriptors;
	int sumAfterPipeline;
	int sumAppliedToBinDescriptors;
	int sumAppliedToBinPipeline;
	
	int preWorldSpaceDivisionDescriptor; //for getting all the counts
	int preWorldSpaceDivisionPipeline;

	int postWorldSpaceDivisionDescriptor; //for assigning all the slots
	int postWorldSpaceDivisionPipeline;
	int worldSpaceDivisionAlloc; //where all the assignments go 
};

enum class LightType
{
	DIRECTIONAL = 0,
	POINT = 1,
	SPOT = 2,
};

struct LightSource
{
	Vector4f color; //w is intensity;
	Vector4f pos; //w is radius for point right
	Vector4f direction; //w is unused;
	Vector4f ancillary; //for spot, x, y are cosine theta cutoffs
};


LightSource mainDirectionalLight =
{
	Vector4f(229.0, 211.0, 191.0, .01),
	Vector4f(0.0,0.0,0.0,0.0),
	Vector4f(.75, -0.33, -.25, 1.0),
	Vector4f(0.001,0.0018,0.0012,0.0),
};


LightSource mainSpotLight =
{
	Vector4f(229.0, 211.0, 191.0, 100.5),
	Vector4f(-10.0,5.0, 4.0,15.0),
	Vector4f(0.0, 0.0, -1.0, 0.0),
	Vector4f(DegToRad(0.0),DegToRad(10.0),0.0,0.0),
};


static IndirectDrawData mainIndirectDrawData;

static IndirectDrawData debugIndirectDrawData;

static WorldSpaceGPUPartition worldSpaceAssignment;
static WorldSpaceGPUPartition lightAssignment;


static int globalIndexBuffer = 0;
static int globalIndexBufferSize = 1024 * KiB;
static int globalVertexBuffer = 0;
static int globalVertexBufferSize = 1024 * KiB;


static int globalLightBuffer = 0;
static int globalLightBufferSize = 1024 * KiB;
static int globalLightBufferCount = 0;



static int globalLightCount = 0;
static int globalLightSize = (1024 * KiB) / sizeof(LightSource);

static int globalLightTypesBuffer = 0;
static int globalLightTypesBufferSize = globalLightSize * sizeof(uint32_t);

static int normalDebugAlloc;

struct DebugDrawStruct
{
	Vector4f center; // fourth component is radius for sphere type
	Vector4f scale;
	Vector4f color;
	Vector4f halfExtents;
};

static int debugAllocBuffer = 0;
static int debugAllocBufferSize = 10 * KiB;

static int globalDebugMeshIDs = 0;
static int globalDebugTypes = 0;



static int globalBufferLocation;
static int globalBufferDescriptor;
static int globalTexturesDescriptor;

static int globalMeshLocation;
static int globalMeshSize = 24 * KiB;

static int globalRenderableLocation;
static int globalRenderableSize = 24 * KiB;

static int globalMaterialIndicesLocation;
static int globalMaterialIndicesSize = 4 * KiB;

static int globalMaterialsLocation; 
static int globalMaterialsSize = 4 * KiB;
static int globalMaterialsIndex = 0;

static char vertexAndIndicesMemory[16 * MiB];
static char meshObjectSpecificMemory[2 * MiB];
static char geometryObjectSpecificMemory[1 * MiB];
static char mainTextureCacheMemory[16 * MiB];


static SlabAllocator vertexAndIndicesAlloc(vertexAndIndicesMemory, sizeof(vertexAndIndicesMemory));
static SlabAllocator meshObjectSpecificAlloc(meshObjectSpecificMemory, sizeof(meshObjectSpecificMemory));
static SlabAllocator geometryObjectSpecificAlloc(geometryObjectSpecificMemory, sizeof(geometryObjectSpecificMemory));
static DeviceSlabAllocator meshDeviceSpecificAlloc(globalMeshSize);


static DeviceSlabAllocator indexBufferAlloc(globalIndexBufferSize);

static DeviceSlabAllocator vertexBufferAlloc(globalVertexBufferSize);

static TextureDictionary mainDictionary;

Vector4f wow = Vector4f(0.0f, 5.0f, 0.0f, 15.0f);

LightSource source3 = { .color = Vector4f(229, 211, 191, 130.0), .pos = wow, .direction= Vector4f(0.54,0.75,0.38,0.0), .ancillary= Vector4f(0.04,0.07,0.03,0.0) };

#define MAX_GEOMETRY 2048
#define MAX_MESHES 4096
#define MAX_MESH_TEXTURES 8192


std::array<Renderable, 150> renderablesObjects;
static int globalRenderableCount = 0;
static int globalMaterialRangeStart = 0;

struct Geometry
{
	int meshCount;
	int meshStart;
	int geometryInstanceLocalMemoryCount;
	int geometryInstanceLocalMemoryStart;
};

struct Mesh
{
	int vertexId;
	int verticesCount;
	int vertexSize;
	int indexId;
	int indicesCount;
	int indexSize;
	int texuresCount;
	int texturesStart;
	int drawableIndex;
	int meshInstanceLocalMemoryCount;
	int meshInstanceLocalMemoryStart;
	int meshInstanceDeviceMemoryCount;
	int meshInstanceDeviceMemoryStart;
	int meshDescriptor;
	int deviceIndices;
	int deviceVertices;
};

static ArrayAllocator<int, MAX_MESH_TEXTURES> meshTextureHandles{};
static ArrayAllocator<int, MAX_MESHES * 2> meshDeviceMemoryData{};
static ArrayAllocator<void*, MAX_MESHES> meshIndexData{};
static ArrayAllocator<void*, MAX_MESHES> meshVertexData{};
static ArrayAllocator<void*, MAX_MESHES * 2> meshObjectData{};
static ArrayAllocator<void*, MAX_MESHES> geometryObjectData{};
static ArrayAllocator<Mesh, MAX_MESHES> meshInstanceData{};
static ArrayAllocator<Geometry, MAX_GEOMETRY> geometryInstanceData{};

static void ProcessKeys(GenericKeyAction keyActions[KC_COUNT]);

struct UniformGrid
{
	
	Vector4f max;
	Vector4f min;
	int numberOfDivision;
};


UniformGrid mainGrid = {
	
	.max = Vector4f(100.0f, 50.0f, 100.0f, 0.0),
	.min = Vector4f(-100.0f, -50.0f, -100.0f, 0.0),
	.numberOfDivision = 5,
};

static int skyboxPipeline = 0;

static void CreateUniformGrid()
{
	float div = (float)(mainGrid.numberOfDivision);

	int count = mainGrid.numberOfDivision;

	Vector4f extent = (mainGrid.max - mainGrid.min) / div;

	float xMove = extent.x;
	float yMove = extent.y;
	float zMove = extent.z;

	Vector4f maxHalf = mainGrid.max - (extent * 0.5);

	Vector4f half = extent/2.0;

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < count; j++)
		{
			for (int g = 0; g < count; g++)
			{
				Vector3f center = Vector3f(maxHalf.x - (i * xMove), maxHalf.y - (j * yMove), maxHalf.z - (g * zMove));

				loop->CreateAABBDebugStruct(center, half, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.0, 0.0, 0.0, 0.0));
			}
		}
	}
}

static SMBImageFormat ConvertAppImageFormatToSMBFormat(ImageFormat format)
{
	switch (format)
	{
	case X8L8U8V8:         return SMB_X8L8U8V8;
	case DXT1:             return SMB_DXT1;
	case DXT3:             return SMB_DXT3;
	case R8G8B8A8_UNORM:        return SMB_R8G8B8A8_UNORM;
	case B8G8R8A8_UNORM:        return SMB_B8G8R8A8_UNORM;

		// Formats that have no SMB equivalent:
	case B8G8R8A8:
	case D24UNORMS8STENCIL:
	case D32FLOATS8STENCIL:
	case D32FLOAT:
	case R8G8B8A8:
	case R8G8B8:
	case IMAGE_UNKNOWN:
	default:
		return SMB_IMAGEUNKNOWN;
	}
}

ImageFormat ConvertSMBImageToAppImage(SMBImageFormat fmt)
{
	switch (fmt)
	{
	case SMB_X8L8U8V8:     return X8L8U8V8;
	case SMB_DXT1:         return DXT1;
	case SMB_DXT3:         return DXT3;
	case SMB_R8G8B8A8_UNORM:    return R8G8B8A8_UNORM;
	case SMB_B8G8R8A8_UNORM:    return B8G8R8A8_UNORM;

	case SMB_IMAGEUNKNOWN:
	default:
		return IMAGE_UNKNOWN;
	}
}

static void ScanSTDIN(void*);
static bool stopThreadServer = false;

static OSThreadHandle threadHandle;

ApplicationLoop::ApplicationLoop(ProgramArgs& _args) :
	args(_args),
	queueSema(Semaphore()),
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

	if (mainWindow)
	{
		delete mainWindow;
	}
}

int ApplicationLoop::AddMaterialToDeviceMemory(int count, int* ids)
{
	int ret = globalMaterialRangeStart;

	globalMaterialRangeStart += count;

	GlobalRenderer::gRenderInstance->transferPool.Create(ids, sizeof(int) * count, globalMaterialIndicesLocation, sizeof(int) * ret, TransferType::CACHED);

	return ret;
}

int ApplicationLoop::CreateRenderable(Matrix4f& mat, int materialStart, int materialCount, int blendStart, int blendCount, int meshIndex, int instanceCount)
{
	Renderable* renderable = &renderablesObjects[globalRenderableCount];

	int ret = globalRenderableCount++;

	renderable->instanceCount = instanceCount;
	renderable->materialStart = materialStart;
	renderable->meshIndex = meshIndex;
	renderable->blendLayersStart = blendStart;
	renderable->blendLayerCount = blendCount;
	renderable->materialStart = materialStart;
	renderable->materialCount = materialCount;
	renderable->transform = mat;


	GlobalRenderer::gRenderInstance->transferPool.Create(renderable, sizeof(Renderable), globalRenderableLocation, sizeof(Renderable) * ret, TransferType::MEMORY);

	mainIndirectDrawData.commandBufferCount++;

	return ret;
}

int ApplicationLoop::CreateMaterial(
	int flags,
	int* texturesIDs,
	int textureCount,
	const Vector4f& color
)
{
	Material material{};

	material.albedoColor = color;

	material.materialFlags = flags;

	uint32_t* ptr = material.textureHandles.comp;

	for (int i = 0; i < textureCount; i++)
	{
		ptr[i] = texturesIDs[i];
	}

	GlobalRenderer::gRenderInstance->transferPool.Create(&material, sizeof(Material), globalMaterialsLocation, sizeof(Material) * globalMaterialsIndex, TransferType::CACHED);

	return globalMaterialsIndex++;
}

int ApplicationLoop::CreateMeshHandle(
	void* vertexData, void* indexData, 
	int vertexFlags, int vertexCount, int vertexStride, 
	int indexStride, int indexCount, 
	AxisBox& box, Sphere& sphere,
	int vertexAlloc, int indexAlloc
)
{
	Mesh* mesh = nullptr;

	uint32_t meshIndex;

	std::tie(meshIndex, mesh) = meshInstanceData.Allocate();

	mesh->indicesCount = indexCount;

	mesh->verticesCount = vertexCount;

	mesh->vertexSize = vertexStride;

	mesh->indexSize = indexStride;

	mesh->meshInstanceLocalMemoryCount = 1;


	Handles* handles = (Handles*)meshObjectSpecificAlloc.Allocate(sizeof(Handles));

	mesh->meshInstanceLocalMemoryStart = meshObjectData.Allocate(handles);

	mesh->vertexId = globalVertexBuffer;
	mesh->indexId = globalIndexBuffer;

	handles->vertexFlags = vertexFlags;
	handles->stride = vertexStride;

	handles->indexCount = mesh->indicesCount;
	handles->firstIndex = indexAlloc / mesh->indexSize;
	handles->vertexByteOffset = vertexAlloc;


	memcpy(&handles->minMaxBox, &box, sizeof(AxisBox));
	memcpy(&handles->sphere, &sphere, sizeof(Vector4f));

	int meshSpecificAlloc = meshDeviceSpecificAlloc.Allocate(sizeof(Handles), 1);

	mesh->meshInstanceDeviceMemoryCount = 1;
	mesh->meshInstanceDeviceMemoryStart = meshDeviceMemoryData.Allocate(meshSpecificAlloc);

	GlobalRenderer::gRenderInstance->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::MEMORY);

	return meshIndex;
}

void ApplicationLoop::SetPositonOfMesh(int meshIndex, const Vector3f& pos)
{
	auto rendInst = GlobalRenderer::gRenderInstance;

	Mesh* mesh = &meshInstanceData.dataArray[meshIndex];
	Handles* handles = (Handles*)meshObjectData.dataArray[mesh->meshInstanceLocalMemoryStart];


	int meshSpecificAlloc = meshDeviceMemoryData.dataArray[mesh->meshInstanceDeviceMemoryStart];

	//handles->m.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

	rendInst->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::CACHED);

}

void ApplicationLoop::SetPositionOfGeometry(int geomIndex, const Vector3f& pos)
{
	auto rendInst = GlobalRenderer::gRenderInstance;
	Geometry* geom = &geometryInstanceData.dataArray[geomIndex];
	
	int meshCount = geom->meshCount;
	int meshStart = geom->meshStart;

	for (int i = 0; i < meshCount; i++)
	{
		Mesh* mesh = &meshInstanceData.dataArray[meshStart + i];

		Handles* handles = (Handles*)meshObjectData.dataArray[mesh->meshInstanceLocalMemoryStart];

		int meshSpecificAlloc = meshDeviceMemoryData.dataArray[mesh->meshInstanceDeviceMemoryStart];

		//handles->m.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

		rendInst->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::CACHED);
	}

}

void ApplicationLoop::ExecuteCommands(const std::string& command, const std::vector<std::string>& args)
{

	if (command == "load")
	{
		//LoadThreadedWrapper(args.at(0));
	}
	else if (command == "end")
	{
		SetRunning(false);
	} 
	else if (command == "positionm")
	{
		if (args.size() != 4)
			return;
		int meshIndex = std::stoi(args.at(0));
		float x1 = std::stof(args.at(1));
		float y1 = std::stof(args.at(2));
		float z1 = std::stof(args.at(3));
		SetPositonOfMesh(meshIndex, Vector3f(x1, y1, z1));
	}
	else if (command == "positiong")
	{
		if (args.size() != 4)
			return;
		int geomIndex = std::stoi(args.at(0));
		float x1 = std::stof(args.at(1));
		float y1 = std::stof(args.at(2));
		float z1 = std::stof(args.at(3));
		SetPositionOfGeometry(geomIndex, Vector3f(x1, y1, z1));
	}
}


std::string name;

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

		InitializeRuntime();

		//ExecuteCommands("load", { args.inputFile.string() });

		name = args.inputFile.string();

		LoadObject(name);

		CreateUniformGrid();

		int i = 0, j = 1;

		LARGE_INTEGER startTime;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER frequency;

		uint64_t frameCounter = 0;
		double FPS = 60.0f;

		auto fps = [&frameCounter, &currentTime, &startTime, &frequency, &FPS, this]()
			{
				double elapsed;
				QueryPerformanceCounter(&currentTime);

				elapsed = static_cast<double>((currentTime.QuadPart - startTime.QuadPart)) / frequency.QuadPart;

				if (elapsed >= 1.0) {
					FPS = static_cast<double>(frameCounter) / elapsed;
					//std::cout << FPS << "\n";
					//printf("%f\n", FPS);
					mainWindow->SetWindowTitle(std::format("FPS: {:.2f}", FPS));
				

					frameCounter = 0;
					QueryPerformanceCounter(&startTime);
					return 1;
				}

				return 0;
			};

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startTime);

		int updatedCommand = 3, updatedDebugCommand = 3, updateLights = 3;

		int commandCountPrev = mainIndirectDrawData.commandBufferCount;

		int debugCommandCountPrev = debugIndirectDrawData.commandBufferCount;

		int lightCountPrev = globalLightCount;

		GlobalRenderer::gRenderInstance->EndFrame();

		float x = 230.0f;

		source3.pos.x = wow.x + cosf(DegToRad(-90.0f)) * 5.0f;
		source3.pos.y = 0.0f;
		source3.pos.z = wow.z + -sinf(DegToRad(-90.0f)) * 10.0f;

		GlobalRenderer::gRenderInstance->transferPool.Create(&source3, sizeof(LightSource), globalLightBuffer, sizeof(LightSource) * 2, TransferType::MEMORY);


		while (running)
		{
			mainWindow->PollEvents();

			if (mainWindow->ShouldCloseWindow()) break;

			ProcessKeys(mainWindow->windowData.info.actions);

			bool cameraMove = MoveCamera(FPS);

			if (lightCountPrev != globalLightCount || updateLights > 0)
			{
				if (!updateLights) updateLights = 3;

				lightCountPrev = globalLightCount;

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.preWorldSpaceDivisionPipeline, 0);

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.prefixSumPipeline, 0);

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.postWorldSpaceDivisionPipeline, 0);

				updateLights--;
			}

			if (cameraMove || commandCountPrev != mainIndirectDrawData.commandBufferCount || updatedCommand > 0)
			{
				if (!updatedCommand || cameraMove) {
					updatedCommand = 3;
					GlobalRenderer::gRenderInstance->transferCommandPool.Create(8, mainIndirectDrawData.commandBufferCountAlloc, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
				}

				updatedCommand--;
				 
				commandCountPrev = mainIndirectDrawData.commandBufferCount;
				
				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.preWorldSpaceDivisionPipeline, 0);

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.prefixSumPipeline, 0);

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.postWorldSpaceDivisionPipeline, 0);
				

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(mainIndirectDrawData.indirectCullPipeline, 0);

				
			}


		

			if (cameraMove || debugIndirectDrawData.commandBufferCount != debugCommandCountPrev || updatedDebugCommand > 0)
			{
				if (!updatedDebugCommand || cameraMove)
				{
					GlobalRenderer::gRenderInstance->transferCommandPool.Create(8, debugIndirectDrawData.commandBufferCountAlloc, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
					updatedDebugCommand = 3;
				}
				debugCommandCountPrev = debugIndirectDrawData.commandBufferCount;
				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(debugIndirectDrawData.indirectCullPipeline, 0);
				updatedDebugCommand--;
			}
			


			if (mainWindow->windowData.info.HandleResizeRequested())
			{
				GlobalRenderer::gRenderInstance->RecreateSwapChain();
				c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance->GetSwapChainWidth() / (float)GlobalRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, DegToRad(45.0f));
				UpdateCameraMatrix();
				continue;
			}

			auto index = GlobalRenderer::gRenderInstance->BeginFrame();

			if (index != ~0ui32) {

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(skyboxPipeline, 1);

				GlobalRenderer::gRenderInstance->AddPipelineToMainQueue(debugIndirectDrawData.indirectDrawPipeline, 1);

				GlobalRenderer::gRenderInstance->DrawScene(index);

				GlobalRenderer::gRenderInstance->SubmitFrame(index);
			}
			
			GlobalRenderer::gRenderInstance->EndFrame();	

			ProcessCommands();

			ThreadManager::ASyncThreadsDone();

			/*
			std::array<Renderable, 10> arr{};
			std::array<Handles, 10> arr3{};

			std::array<int, 10> arr2{};

			std::array<VkDrawIndexedIndirectCommand, 10> arr4;

			GlobalRenderer::gRenderInstance->ReadData(globalMaterialIndicesLocation, arr2.data(), sizeof(int) * 10, 0);
		//	GlobalRenderer::gRenderInstance->ReadData(globalRenderableLocation, arr.data(), sizeof(Renderable) * 10, 0);
		//	GlobalRenderer::gRenderInstance->ReadData(globalMeshLocation, arr3.data(), sizeof(Handles) * 10, 0);
		//	GlobalRenderer::gRenderInstance->ReadData(mainIndirectDrawData.commandBufferAlloc, arr4.data(), sizeof(VkDrawIndexedIndirectCommand) * 10, 0);

		*/

			fps();

			frameCounter++;
			
		}
	}
}

#define MAX_IMAGE_DIM 4096

void ApplicationLoop::CreateTexturePools()
{
	std::array<ImageFormat, 4> formats = {
		ImageFormat::DXT1,
		ImageFormat::DXT3,
		ImageFormat::R8G8B8A8,
		ImageFormat::B8G8R8A8_UNORM
	};

	auto rendInst = GlobalRenderer::gRenderInstance;

	for (int i = 0; i < 4; i++)
	{
		mainDictionary.texturePoolsFormat[i] = formats[i];
		mainDictionary.texturePoolsSize[i] = 128 * MiB;
		mainDictionary.texturePoolsAllocatedSize[i] = 0;
		mainDictionary.texturePoolHandle[i] = rendInst->CreateImagePool(
			mainDictionary.texturePoolsSize[i],
			formats[i], MAX_IMAGE_DIM, MAX_IMAGE_DIM, false
		);
	}
}

int instanceAlloc;
std::array<Matrix4f, 64 * 64> instanceMatrices;

static EntryHandle storageBuffer;

bool ApplicationLoop::MoveCamera(double fps)
{
	bool moved = false;

	float stepfactor = 15.0f / static_cast<float>(fps);

	double angleFactor = 15.0 / fps;

	if (camMovements[RIGHT])
	{
		c.LTM.MoveRight(stepfactor);
		moved = true;
	}

	if (camMovements[LEFT])
	{
		c.LTM.MoveRight(-stepfactor);
		moved = true;
	}

	if (camMovements[FORWARD])
	{
		c.LTM.MoveForward(-stepfactor);
		moved = true;
	}

	if (camMovements[BACK])
	{
		c.LTM.MoveForward(stepfactor);
		moved = true;
	}

	if (camMovements[PITCHDOWN])
	{
		c.LTM.PitchLTM(-angleFactor);
		moved = true;
	}

	if (camMovements[PITCHUP])
	{
		c.LTM.PitchLTM(angleFactor);
		moved = true;
	}

	if (camMovements[ROTATEYLEFT])
	{
		c.LTM.RotateAroundUp(angleFactor);
		moved = true;
	}

	if (camMovements[ROTATEYRIGHT])
	{
		c.LTM.RotateAroundUp(-angleFactor);
		moved = true;
	}

	if (moved) UpdateCameraMatrix();

	return moved;
}

void ApplicationLoop::UpdateCameraMatrix()
{
	c.UpdateCamera();

	WriteCameraMatrix(RenderInstance::MAX_FRAMES_IN_FLIGHT);
}

void ApplicationLoop::WriteCameraMatrix(uint32_t frame)
{
	//GlobalRenderer::gRenderInstance->UpdateAllocation(&c.View, globalBufferLocation, (sizeof(Matrix4f) * 3) + sizeof(Frustrum), 0, 0, frame);
	GlobalRenderer::gRenderInstance->transferPool.Create(&c.View, (sizeof(Matrix4f) * 3) + sizeof(Frustrum), globalBufferLocation, 0, TransferType::MEMORY);
}

int ApplicationLoop::GetPoolIndexByFormat(ImageFormat format)
{
	int ret = 0;
	switch (format)
	{
	case ImageFormat::DXT1:
		ret = 0;
		break;
	case ImageFormat::DXT3:
		ret = 1;
		break;
	case ImageFormat::R8G8B8A8:
		ret = 2;
		break;
	case ImageFormat::B8G8R8A8_UNORM:
		ret = 3;
		break;
	}
	return ret;
}




std::atomic<float> geoX = 0.0f;

void ReadImage(std::string* name, int count);



static Vector3s pack6comp(float* vector, AxisBox& box)
{
	float x = vector[0];
	float y = vector[1];
	float z = vector[2];

	float x1 = ((((x - box.min.x) / (box.max.x - box.min.x)) * 2.0) - 1.0);
	float y1 = ((((y - box.min.y) / (box.max.y - box.min.y)) * 2.0) - 1.0);
	float z1 = ((((z - box.min.z) / (box.max.z - box.min.z)) * 2.0) - 1.0);

	int16_t xs = x1 * 32767;
	int16_t ys = y1 * 32767;
	int16_t zs = z1 * 32767;
   
	return Vector3s(xs, ys, zs);
}

static Vector3f convertnormal(int32_t normal)
{

	float denom = pow(2.0, 31.0);

	float x = float(normal << 21) * 1.0 / denom;
	float y = float((int)((normal << 10) & 0xfffff800)) * 1.0 / denom;
	float z = float((int)(normal & 0xffc00000)) * 1.0 / (denom - 1.0);

	return Vector3f(x, y, z);
}

static uint32_t compressnormal(Vector3f normal)
{
	uint32_t reg = 0;

	float denom = pow(2.0, 31.0);

	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	int16_t xs = (1023 * x);
	int16_t ys = (1023 * y);
	int16_t zs = (511 * z);

	reg = ((zs & 0x3ff) << 22) | ((ys & 0x7ff) << 11) | ((xs & 0x7ff));

	return reg;
}

void ApplicationLoop::CreateCrateObject()
{
	static uint16_t BoxIndices[52] = {
		2,  1,  0, 1, 
		1,  2,  3, 3, 4,
		4,  5,  6, 6,
		7,  6,  5, 5, 8,
		8,  9,  10, 10,
		11, 10, 9, 9, 14,
	   14, 13, 12, 13,
	   13, 14, 15, 15, 18,
	   18, 17, 16, 17,
	   17, 18, 19, 19, 20,
	   20, 21, 22, 22,
	   23, 22, 21
	};

	static Vector4f BoxVerts[24] =
	{
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0)
	};


	static Vector4f BoxColors[24] =
	{
		Vector4f(1,0,0,1),
		Vector4f(1,0,0,1),
		Vector4f(1,0,0,1),
		Vector4f(1,0,0,1),

	
		Vector4f(0,1,0,1),
		Vector4f(0,1,0,1),
		Vector4f(0,1,0,1),
		Vector4f(0,1,0,1),

		
		Vector4f(0,0,1,1),
		Vector4f(0,0,1,1),
		Vector4f(0,0,1,1),
		Vector4f(0,0,1,1),

		
		Vector4f(1,1,0,1),
		Vector4f(1,1,0,1),
		Vector4f(1,1,0,1),
		Vector4f(1,1,0,1),

		
		Vector4f(1,0,1,1),
		Vector4f(1,0,1,1),
		Vector4f(1,0,1,1),
		Vector4f(1,0,1,1),

		
		Vector4f(0,1,1,1),
		Vector4f(0,1,1,1),
		Vector4f(0,1,1,1),
		Vector4f(0,1,1,1),
	};

	Vector3f totalNormals[24] = {


		Vector3f(1.0, 0.0, 0.0),
		Vector3f(1.0, 0.0, 0.0),
		Vector3f(1.0, 0.0, 0.0),
		Vector3f(1.0, 0.0, 0.0),
		Vector3f(-1.0, 0.0, 0.0),
		Vector3f(-1.0, 0.0, 0.0),
		Vector3f(-1.0, 0.0, 0.0),
		Vector3f(-1.0, 0.0, 0.0),
		Vector3f(0.0, 1.0, 0.0),
		Vector3f(0.0, 1.0, 0.0),
		Vector3f(0.0, 1.0, 0.0),
		Vector3f(0.0, 1.0, 0.0),
		Vector3f(0.0, -1.0, 0.0),
		Vector3f(0.0, -1.0, 0.0),
		Vector3f(0.0, -1.0, 0.0),
		Vector3f(0.0, -1.0, 0.0),
		
		Vector3f(0.0, 0.0, 1.0),
		Vector3f(0.0, 0.0, 1.0),
		Vector3f(0.0, 0.0, 1.0),
		Vector3f(0.0, 0.0, 1.0),
		Vector3f(0.0, 0.0, -1.0),
		Vector3f(0.0, 0.0, -1.0),
		Vector3f(0.0, 0.0, -1.0),
		Vector3f(0.0, 0.0, -1.0),
	};

	
#pragma pack(push, 1)
	struct ColoredVertex
	{
		int32_t normalCoord;
		Vector4f color;
		Vector3s pos;
		
	};
#pragma pack(pop)
	ColoredVertex compVerts[24];

	AxisBox BOX =
	{
		.min = Vector4f(-1.0, -1.0, -1.0, 0.0),
		.max = Vector4f(1.0, 1.0, 1.0, 0.0),
	};

	for (int i = 0; i < 24; i++)
	{
		compVerts[i].pos = pack6comp(BoxVerts[i].comp, BOX);
		compVerts[i].color = BoxColors[i];
		compVerts[i].normalCoord = compressnormal(totalNormals[i]);
	}

	auto rendInst = GlobalRenderer::gRenderInstance;

	Matrix4f crateMatrix = Identity4f();

	crateMatrix.translate = Vector4f(-10.0, 5.0, 0.0f, 1.0f);

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, 1.5f);

	int vertexAlloc = vertexBufferAlloc.Allocate(sizeof(ColoredVertex) * 24, 16);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	std::array<int, 1> materialIDs = { CreateMaterial(VERTEXNORMAL, nullptr, 0, Vector4f(1.0, 1.0, 1.0, 1.0)) };

	int materialRangeStart = AddMaterialToDeviceMemory(1, materialIDs.data());
	int materialRangeCount = 1;

	int meshIndex = CreateMeshHandle(compVerts, BoxIndices, COMPRESSED | NORMAL | POSITION | COLOR, 24, sizeof(ColoredVertex), 2, 52, BOX, sphere, vertexAlloc, indexAlloc);

	int renderableIndex = CreateRenderable(crateMatrix, materialRangeStart, materialRangeCount, 0, 0, meshIndex, 1);

	rendInst->deviceMemoryUpdater.Create(compVerts, sizeof(compVerts), globalVertexBuffer, vertexAlloc, 1, TransferType::CACHED);
	rendInst->deviceMemoryUpdater.Create(BoxIndices, sizeof(BoxIndices), globalIndexBuffer, indexAlloc, 1, TransferType::MEMORY);

	
}

void ApplicationLoop::SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file)
{
	auto rendInst = GlobalRenderer::gRenderInstance;

	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

	int count = geoDef->numRenderables;

	float xLoc = UpdateAtomic(geoX, 50.0f, 0.0f);

	

	Geometry* geom = nullptr;

	std::tie(std::ignore, geom) = geometryInstanceData.Allocate();

	geom->meshStart = meshInstanceData.allocatorPtr;

	geom->geometryInstanceLocalMemoryCount = 1;

	Matrix4f *geomSpecificData = (Matrix4f*)geometryObjectSpecificAlloc.Allocate(sizeof(Matrix4f));

	geom->geometryInstanceLocalMemoryStart = geometryObjectData.Allocate(geomSpecificData);

	*geomSpecificData = Identity4f();
		
	//*geomSpecificData = *geomSpecificData * 5.0f;
		
	(geomSpecificData)->translate = Vector4f(xLoc, 0.f, 0.f, 1.0f);

	Matrix4f rotation = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0f), DegToRad(90.0f));

	//*geomSpecificData = *geomSpecificData * rotation;

	for (int i = 0; i < count; i++)
	{

		SMBVertexTypes type = geoDef->vertexTypes[i];

		int vertexCount = geoDef->verticesCount[i];

		int indexCount = geoDef->indicesCount[i];

		size_t vertexSize = 0;
		
		void* vertexData = nullptr;

		bool decompressed = false;

		switch (type)
		{
		case PosPack6_CNorm_C16Tex1_Bone2:
			if (decompressed)
				vertexSize = sizeof(Vertex_PosPack6_CNorm_C16Tex1_Bone2);
			else
				vertexSize = sizeof(CVertex_PosPack6_CNorm_C16Tex1_Bone2);
			break;
		case PosPack6_C16Tex2_Bone2:
			if (decompressed)
				vertexSize = sizeof(Vertex_PosPack6_C16Tex2_Bone2);
			else
				vertexSize = sizeof(CVertex_PosPack6_C16Tex2_Bone2);
			break;
		case PosPack6_C16Tex1_Bone2:
			if (decompressed)
				vertexSize = sizeof(Vertex_PosPack6_C16Tex1_Bone2);
			else
				vertexSize = sizeof(CVertex_PosPack6_C16Tex1_Bone2);
			break;
		}

		vertexData = (void*)vertexAndIndicesAlloc.Allocate(vertexSize * vertexCount);

		SMBCopyVertexData(geoDef, i, file, vertexData, decompressed);

		int vertexFlags = POSITION | TEXTURES1;

		switch (type)
		{
		case PosPack6_CNorm_C16Tex1_Bone2:
		{
			vertexFlags = POSITION | TEXTURES1 | NORMAL | BONES2;
			break;
		}
		case PosPack6_C16Tex2_Bone2:
		{
			vertexFlags = POSITION | TEXTURES1 | TEXTURES2 | BONES2;
			break;
		}
		case PosPack6_C16Tex1_Bone2:
		{
			vertexFlags = POSITION | TEXTURES1 | BONES2;
			break;
		}
		}

		if (!decompressed)
		{
			vertexFlags |= COMPRESSED;
		}



		int vertexAlloc = vertexBufferAlloc.Allocate(vertexSize * vertexCount, 16);

		uint16_t* indices = (uint16_t*)vertexAndIndicesAlloc.Allocate(sizeof(uint16_t) * indexCount);

		SMBCopyIndices(geoDef, i, file, indices);

		int indexAlloc = indexBufferAlloc.Allocate(sizeof(uint16_t) * indexCount, 1);

		
		rendInst->deviceMemoryUpdater.Create(indices, sizeof(uint16_t) * indexCount, globalIndexBuffer, indexAlloc, 1, TransferType::MEMORY);

		rendInst->deviceMemoryUpdater.Create(vertexData, vertexSize * vertexCount, globalVertexBuffer, vertexAlloc, 1, TransferType::MEMORY);
		

		int base = geoDef->materialStart[i];

		int textureStart = meshTextureHandles.AllocateN(geoDef->materialsCount[i]);
		int textureCount = geoDef->materialsCount[i];

		for (int j = 0; j < textureCount; j++)
		{
			meshTextureHandles.Update(textureStart + i, geoDef->materialsId[base + i]);
		}

		std::array<int, 1> materialHandles = { CreateMaterial(ALBEDOMAPPED | ((textureCount > 1) ? NORMALMAPPED : VERTEXNORMAL), &geoDef->materialsId[base], textureCount, Vector4f(1.0, 1.0, 1.0, 1.0))};

		//geoDef->spheres[i].sphere.w += .75f;

		int materialRangeStart = AddMaterialToDeviceMemory(1, materialHandles.data());
		int materialRangeCount = 1;

		int meshIndex = CreateMeshHandle(vertexData, indices, 
			vertexFlags, vertexCount, vertexSize, 2, 
			indexCount,
			geoDef->axialBox, geoDef->spheres[i], 
			vertexAlloc, indexAlloc
		);

		

		int renderableIndex = CreateRenderable(*geomSpecificData, materialRangeStart, materialRangeCount, 0, 0, meshIndex, 1);

	}

	CreateSphereDebugStruct(geoDef->axialBox, 24, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(0.0, 1.0, 0.0, 0.0));

	CreateAABBDebugStruct(geoDef->axialBox, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.5, 0.5, 1.0, 0.0));

	//mainIndirectDrawData.commandBufferCount += count;
}


int ApplicationLoop::CreateSphereDebugStruct(const Sphere& sphere, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	return CreateSphereDebugStruct(Vector3f(sphere.sphere.x, sphere.sphere.y, sphere.sphere.z), sphere.sphere.w, count, scale, color);
}

int ApplicationLoop::CreateSphereDebugStruct(const AxisBox& box, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	return CreateSphereDebugStruct(box.min, box.max, count, scale, color);
}

int ApplicationLoop::CreateSphereDebugStruct(const Vector4f& minExtent, const Vector4f& maxExtent, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	Vector4f center = ((maxExtent - minExtent) * 0.5 + (minExtent));

	Vector4f half = ((maxExtent - minExtent) * 0.5);
	Vector3f half3 = Vector3f(half.x, half.y, half.z);
	float r = Length(half3);

	return CreateSphereDebugStruct(Vector3f(center.x, center.y, center.z), r, count, scale, color);
}

int ApplicationLoop::CreateSphereDebugStruct(const Vector3f& center, float r, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	DebugDrawStruct drawStruct;
	DebugDrawType type = DebugDrawType::DEBUGSPHERE;


	drawStruct.center = Vector4f(center.x, center.y, center.z, 1.0f);

	drawStruct.scale = scale;
	drawStruct.color = color;

	drawStruct.halfExtents = Vector4f(r, std::bit_cast<float, uint32_t>(count), 1.0, 1.0);


	GlobalRenderer::gRenderInstance->transferPool.Create(&drawStruct, sizeof(DebugDrawStruct), debugAllocBuffer, sizeof(DebugDrawStruct) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&type, sizeof(DebugDrawType), globalDebugTypes, sizeof(DebugDrawType) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	return debugIndirectDrawData.commandBufferCount++;
}

int ApplicationLoop::CreateAABBDebugStruct(const AxisBox& box, const Vector4f& scale, const Vector4f& color)
{
	Vector4f half = ((box.max - box.min) * 0.5);

	Vector4f center = ((box.max - box.min) * 0.5 + (box.min));

	return CreateAABBDebugStruct(Vector3f(center.x, center.y, center.z), half, scale, color);
}

int ApplicationLoop::CreateAABBDebugStruct(const Vector4f& boxMin, const Vector4f& boxMax, const Vector4f& scale, const Vector4f& color)
{
	Vector4f half = ((boxMax - boxMin) * 0.5);;

	Vector4f center = ((boxMax - boxMin) * 0.5 + (boxMin));
	
	return CreateAABBDebugStruct(Vector3f(center.x, center.y, center.z), half, scale, color);
}

int ApplicationLoop::CreateAABBDebugStruct(const Vector3f& center, const Vector4f& halfExtents, const Vector4f& scale, const Vector4f& color)
{
	DebugDrawType type = DebugDrawType::DEBUGBOX;
	DebugDrawStruct drawStruct;
	
	drawStruct.center = Vector4f(center.x , center.y, center.z, 1.0f);
	drawStruct.scale = scale;
	drawStruct.color = color;
	drawStruct.halfExtents = halfExtents;

	GlobalRenderer::gRenderInstance->transferPool.Create(&drawStruct, sizeof(DebugDrawStruct), debugAllocBuffer, sizeof(DebugDrawStruct) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	GlobalRenderer::gRenderInstance->transferPool.Create(&type, sizeof(DebugDrawType), globalDebugTypes, sizeof(DebugDrawType) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	return debugIndirectDrawData.commandBufferCount++;
}



void ApplicationLoop::LoadSMBFile(SMBFile &file)
{

	int totalTextureCount = 0, totalMeshCount = 0;

	std::vector<SMBTexture> textures;

	auto& chunk = file.chunks;

	std::array<SMBGeoChunk, 10> geoDefs{};

	for (size_t i = 0; i<chunk.size(); i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{
			
			SMBGeoChunk* geoDef = &geoDefs[totalMeshCount++];

			OSFileHandle* handle = FileManager::GetFile(file.id);


			size_t seekpos = chunk[i].offsetInHeader;

			OSSeekFile(handle, seekpos, BEGIN);

			std::vector<char> geomHeader(chunk[i].headerSize);

			OSReadFile(handle, chunk[i].headerSize, geomHeader.data());

		    ProcessGeometryClass(geomHeader.data(), totalTextureCount, geoDef, chunk[i].contigOffset + file.fileOffset, chunk[i].fileOffset + file.numContiguousBytes + file.fileOffset);
	
			break;
		}
		case TEXTURE:
		{
			SMBTexture texture(file, chunk[i]);

			
			textures.emplace_back( texture );
			totalTextureCount++;
			break;
		}
		case GR2:
			break;
		case Joints:
			break;
		default:
			std::cerr << "Unprocessed chunkType\n";
			break;
		}
	}

	static char imageMemory[16 * MiB];
	static uint32_t imageSizes[400];
	int cpointer = 0, ispointer = 0;

	int index = mainDictionary.AllocateNTextureHandles(totalTextureCount);

	for (int ii = 0; ii < totalTextureCount; ii++)
	{
		SMBTexture& texture = textures[ii];

		ImageFormat format = ConvertSMBImageToAppImage(texture.type);

		mainDictionary.UpdateTextureData(
			ii + index,
			(char*)texture.data,
			texture.cumulativeSize,
			format,
			texture.width,
			texture.height,
			texture.miplevels);

		memcpy(imageMemory + cpointer, texture.data, texture.cumulativeSize);
		
		memcpy(imageSizes + ispointer, texture.imageSizes, texture.miplevels * sizeof(uint32_t));
		

		mainDictionary.textureHandles[ii + index] =
			GlobalRenderer::gRenderInstance->CreateImageHandle(
				texture.cumulativeSize,
				texture.width,
				texture.height,
				texture.miplevels,
				format,
				GetPoolIndexByFormat(format));

		GlobalRenderer::gRenderInstance->imageMemoryUpdateManager.Create(
			imageMemory + cpointer, 
			mainDictionary.textureHandles[ii + index], 
			imageSizes + ispointer, 
			texture.cumulativeSize, 
			texture.width, 
			texture.height, 
			texture.miplevels,
			1,
			format
		);

		ispointer += texture.miplevels;
		cpointer += texture.cumulativeSize;
	}

	ResourceArrayUpdate update; 

	update.count = totalTextureCount;
	update.dstBegin = index;
	update.handles = mainDictionary.textureHandles.data() + index;

	GlobalRenderer::gRenderInstance->descriptorUpdatePool.Create(globalTexturesDescriptor, 0, ShaderResourceType::IMAGE2D, &update);


	ResourceArrayUpdate update2;

	update2.count = 1;
	update2.dstBegin = 0;
	update2.handles = &GlobalRenderer::gRenderInstance->samplerIndex;

	GlobalRenderer::gRenderInstance->descriptorUpdatePool.Create(globalTexturesDescriptor, 1, ShaderResourceType::SAMPLERSTATE, &update2);
	
	for (int i = 0; i < totalMeshCount; i++)
	{
		SMBGeoChunk* geoDef = &geoDefs[i];

		int base = 0;

		for (int jj = 0; jj < geoDef->numRenderables; jj++)
		{
			int count = geoDef->materialsCount[jj];
			for (int hh = 0; hh < count; hh++)
			{
				uint32_t id = geoDef->materialsId[hh + base];
				int gg = 0;
				for (; gg < totalTextureCount; gg++)
				{
					if (id == textures[gg].id)
					{
						geoDef->materialsId[hh + base] = index + gg;
						break;
					}
				}

				if (gg == totalTextureCount)
				{
					geoDef->materialsId[hh + base] = -1;
				}

			}

			geoDef->materialStart[jj] = base;

			base += count;
		}

		SMBGeometricalObject(geoDef, file);
	}
	

	
}

void CreateDebugCommandBuffers(int count)
{
	debugIndirectDrawData.commandBufferCount = 0;
	debugIndirectDrawData.commandBufferSize = count;


	debugIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(VkDrawIndirectCommand) * debugIndirectDrawData.commandBufferSize, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 1);

	debugIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 0);

	debugIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 1);


	debugIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(6, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.commandBufferAlloc, 0, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.indirectGlobalIDsAlloc, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, globalDebugTypes, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, globalBufferLocation, 3, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugAllocBuffer, 4, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.commandBufferCountAlloc, 5, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCount, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 5, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);


	std::array debugCullDescriptors = { debugIndirectDrawData.indirectCullDescriptor };

	ShaderComputeLayout* debugCullDescriptorLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(6);

	ComputeIntermediaryPipelineInfo debugCullPipelineCreate = {
			.x = 4096 / debugCullDescriptorLayout->x,
			.y = 1,
			.z = 1,
			.pipelinename = 6,
			.descCount = 1,
			.descriptorsetid = debugCullDescriptors.data()
	};


	debugIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&debugCullPipelineCreate);

	debugIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(5, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectDrawDescriptor, debugAllocBuffer, 0, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, debugIndirectDrawData.indirectGlobalIDsAlloc, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, globalDebugTypes, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	std::array<int, 2> indirectDebugDrawDescriptors = {
		globalBufferDescriptor,
		debugIndirectDrawData.indirectDrawDescriptor,
	};

	GraphicsIntermediaryPipelineInfo indirectDebugDrawPipelineCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = 5,
		.descCount = 2,
		.descriptorsetid = indirectDebugDrawDescriptors.data(),
		.indexBufferHandle = ~0,
		.indexSize = 0,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = debugIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = debugIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = debugIndirectDrawData.commandBufferCountAlloc
	};


	debugIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&indirectDebugDrawPipelineCreate, false);
}

void CreateGenericMeshCommandBuffers(int count)
{
	mainIndirectDrawData.commandBufferSize = count;
	mainIndirectDrawData.commandBufferCount = 0;

	mainIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(VkDrawIndexedIndirectCommand) * count, alignof(VkDrawIndexedIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 1);

	mainIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 1);

	mainIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(4, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	mainIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * mainIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 1);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.commandBufferAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, globalMeshLocation, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, globalBufferLocation, 2, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 3, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.commandBufferCountAlloc, 4, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, globalRenderableLocation, 5, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 5, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 4, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);



	mainIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(0, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalMeshLocation, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalVertexBuffer, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalLightBuffer, 3, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, globalLightTypesBuffer, 4, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalMaterialsLocation, 5, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalRenderableLocation, 6, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, globalMaterialIndicesLocation, 7, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);


	std::array<int, 1> indirectDrawDescriptors = {
		mainIndirectDrawData.indirectDrawDescriptor,
	};

	GraphicsIntermediaryPipelineInfo indirectDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = GENERIC,
		.descCount = 1,
		.descriptorsetid = indirectDrawDescriptors.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexSize = 2,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = mainIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = mainIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = mainIndirectDrawData.commandBufferCountAlloc
	};


	mainIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&indirectDrawCreate, true);

	int cullLightDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(4, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.worldSpaceDivisionAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.deviceOffsetsAlloc, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.deviceCountsAlloc, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(cullLightDescriptor, globalLightBuffer, 3, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, globalLightTypesBuffer, 4, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);


	ShaderComputeLayout* layout = GlobalRenderer::gRenderInstance->GetComputeLayout(4);

	std::array computeDescriptors = { mainIndirectDrawData.indirectCullDescriptor, cullLightDescriptor };

	ComputeIntermediaryPipelineInfo mainCullComputeSetup = {
			.x = 4096 / layout->x,
			.y = 1,
			.z = 1,
			.pipelinename = 4,
			.descCount = 2,
			.descriptorsetid = computeDescriptors.data()
	};

	mainIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&mainCullComputeSetup);

	/*

	normalDebugAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(VkDrawIndirectCommand) * 10, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	int normalDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(13, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(normalDrawDescriptor, globalMeshLocation, 0, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(normalDrawDescriptor, globalVertexBuffer, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(normalDrawDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	std::array<int, 1> indirectDrawDescriptors2 = {
		normalDrawDescriptor,

	};
	GraphicsIntermediaryPipelineInfo normalDrawCreate = {
	.drawType = 0,
	.vertexBufferHandle = ~0,
	.vertexCount = 0,
	.pipelinename = 13,
	.descCount = 1,
	.descriptorsetid = indirectDrawDescriptors2.data(),
	.indexBufferHandle = ~0,
	.indexSize = 0,
	.indexOffset = 0,
	.vertexOffset = 0,
	.indirectAllocation = normalDebugAlloc,
	.indirectDrawCount = 6,
	.indirectCountAllocation = ~0
	};

	//int what = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&normalDrawCreate, true);
	*/


	int outlineDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(15, 2, 3);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, globalMeshLocation, 0, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, globalVertexBuffer, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(outlineDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, globalRenderableLocation, 3, 0);

	static Vector4f red = { 0.0, 0.0, 0.0, 1.0 };

	static float outlineLength = 1.025;

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(outlineDescriptor, &red, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(outlineDescriptor, &outlineLength, 1);

	std::array<int, 2> indirectOutline = { outlineDescriptor };

	GraphicsIntermediaryPipelineInfo outlineDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = 15,
		.descCount = 1,
		.descriptorsetid = indirectOutline.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexSize = 2,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = mainIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = mainIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = mainIndirectDrawData.commandBufferCountAlloc
	};

	int outlinePipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&outlineDrawCreate, true);


}

void CreateMeshWorldAssignment(int count)
{
	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(7);

	worldSpaceAssignment.totalElementsCount = count;
	worldSpaceAssignment.totalSumsNeeded = (int)floor(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	worldSpaceAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	worldSpaceAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	worldSpaceAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	if (worldSpaceAssignment.totalSumsNeeded )
	{
		worldSpaceAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);
		
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceSumsAlloc, 2, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else 
	{
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, -1, 2, 0);
	}


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceCountsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceOffsetsAlloc, 1, 0);
	

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

	std::array prefixSumDescriptor = { worldSpaceAssignment.prefixSumDescriptors };

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = 7,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};



	worldSpaceAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);
	
	if (worldSpaceAssignment.totalSumsNeeded)
	{

		worldSpaceAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 0, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 2, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { worldSpaceAssignment.sumAfterDescriptors, };

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = 7,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		worldSpaceAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);

		worldSpaceAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(8, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, worldSpaceAssignment.deviceOffsetsAlloc, 0, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, worldSpaceAssignment.deviceSumsAlloc, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



		std::array incrementSumsDescriptor = { worldSpaceAssignment.sumAppliedToBinDescriptors, };

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = 8,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};

		worldSpaceAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(10);

	uint32_t assignmentGroupCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)assignmentLayout->x);

	worldSpaceAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 0);

	worldSpaceAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(9, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, worldSpaceAssignment.deviceCountsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, globalMeshLocation, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, globalRenderableLocation, 2, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { worldSpaceAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = 9,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	worldSpaceAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);


	worldSpaceAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(10, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, worldSpaceAssignment.deviceOffsetsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, globalMeshLocation, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, worldSpaceAssignment.worldSpaceDivisionAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, globalRenderableLocation, 3, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { worldSpaceAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = 10,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	worldSpaceAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);
}

void CreateLightAssignments(int count)
{
	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(7);

	lightAssignment.totalElementsCount = count;
	lightAssignment.totalSumsNeeded = (int)floor(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	lightAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	lightAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 1);

	lightAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceCountsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceOffsetsAlloc, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.prefixSumDescriptors, &lightAssignment.totalElementsCount, 0);

	if (lightAssignment.totalSumsNeeded)
	{
		lightAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * lightAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 1);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceSumsAlloc, 2, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else
	{
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, -1, 2, 0);
	}

	std::array prefixSumDescriptor = { lightAssignment.prefixSumDescriptors };

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = 7,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};

	lightAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);

	if (lightAssignment.totalSumsNeeded)
	{

		lightAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 0, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 2, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAfterDescriptors, &lightAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { lightAssignment.sumAfterDescriptors, };

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = 7,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		lightAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);



		lightAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(8, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, lightAssignment.deviceOffsetsAlloc, 0, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, lightAssignment.deviceSumsAlloc, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



		std::array incrementSumsDescriptor = { lightAssignment.sumAppliedToBinDescriptors, };

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = 8,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};


		lightAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(12);

	uint32_t assignmentGroupCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)assignmentLayout->x);


	lightAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 1);

	lightAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(11, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, lightAssignment.deviceCountsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, globalLightBuffer, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.preWorldSpaceDivisionDescriptor, globalLightTypesBuffer, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightCount, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { lightAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 11,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	lightAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);


	lightAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(12, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, lightAssignment.deviceOffsetsAlloc, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, globalLightBuffer, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.postWorldSpaceDivisionDescriptor, globalLightTypesBuffer, 3, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.postWorldSpaceDivisionDescriptor, lightAssignment.worldSpaceDivisionAlloc, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { lightAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 12,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	lightAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);
}


void ApplicationLoop::InitializeRuntime()
{

	queueSema.Create();

	threadHandle = ThreadManager::LaunchOSBackgroundThread(ScanSTDIN, &stopThreadServer);

	mainDictionary.textureCache = (uintptr_t)mainTextureCacheMemory;
	
	mainDictionary.textureSize = sizeof(mainTextureCacheMemory);

	mainWindow = new WindowManager();

	mainWindow->CreateMainWindow();

	GlobalRenderer::gRenderInstance = new RenderInstance();

	GlobalRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow);

	CreateTexturePools();

	globalBufferLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer((sizeof(Matrix4f) * 3) + sizeof(Frustrum), alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);
	globalIndexBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalIndexBufferSize, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, 2);
	globalVertexBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalVertexBufferSize, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, 1);
	globalBufferDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(0, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(0, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	globalMeshLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalMeshSize, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);
	globalMaterialsLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalMaterialsSize, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	globalLightBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalLightBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);
	globalLightTypesBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalLightTypesBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 0);

	debugAllocBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(debugAllocBufferSize, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);
	globalDebugTypes = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(uint32_t) * 128, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 0);

	globalMaterialIndicesLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalMaterialIndicesSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, 0);
	globalRenderableLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(globalRenderableSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalBufferDescriptor, globalBufferLocation, 0, 0);

	std::array arr = { globalBufferDescriptor, globalTexturesDescriptor };

	GlobalRenderer::gRenderInstance->CreateRenderTargetData(arr.data(), 2);

	static uint16_t BoxIndices[36] = {
		2,  1,  0,
		1,  2,  3,
		4,  5,  6,
		7,  6,  5,
	    8,  9,  10,
		11, 10, 9,
	   14, 13, 12,
	   13, 14, 15,
	   18, 17, 16,
	   17, 18, 19,
	   20, 21, 22,
	   23, 22, 21
	};

	static Vector4f BoxVerts[24] =
	{
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0),
		Vector4f(-1.0,  1.0,  1.0, 1.0),
		Vector4f(1.0,  1.0,  1.0, 1.0),
		Vector4f(-1.0, -1.0,  1.0, 1.0),
		Vector4f(1.0, -1.0,  1.0, 1.0),
		Vector4f(-1.0,  1.0, -1.0, 1.0),
		Vector4f(1.0,  1.0, -1.0, 1.0),
		Vector4f(-1.0, -1.0, -1.0, 1.0),
		Vector4f(1.0, -1.0, -1.0, 1.0)
	};

	int vertexAlloc = vertexBufferAlloc.Allocate(sizeof(BoxVerts), 64);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	GlobalRenderer::gRenderInstance->deviceMemoryUpdater.Create(BoxVerts, sizeof(BoxVerts), globalVertexBuffer, vertexAlloc, 1, TransferType::MEMORY);
	GlobalRenderer::gRenderInstance->deviceMemoryUpdater.Create(BoxIndices, sizeof(BoxIndices), globalIndexBuffer, indexAlloc, 1, TransferType::MEMORY);

	std::string names[6] = {
	"face4.bmp",
	"face1.bmp",
	"face5.bmp",
	"face2.bmp",
	"face6.bmp",
	"face3.bmp",
	
	
	
	};

	ReadImage(names, 6);

	static Matrix4f matrix = Identity4f();

	matrix.translate = Vector4f(-30.0, 0.0, 0.0, 1.0f);

	int skyboxDesc = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(14, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	int globalPipelineBufferWhat = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(14, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalPipelineBufferWhat, globalBufferLocation, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindSampledImageToShaderResource(skyboxDesc, &mainDictionary.textureHandles[10], 1, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(skyboxDesc, &matrix, 0);

	std::array<int, 2> skyboxDescs = { globalPipelineBufferWhat, skyboxDesc };

	GraphicsIntermediaryPipelineInfo skyboxInfo = {
		.drawType = 0,
		.vertexBufferHandle = globalVertexBuffer,
		.vertexCount = 24,
		.pipelinename = 14,
		.descCount = 2,
		.descriptorsetid = skyboxDescs.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexCount = 36,
		.instanceCount = 1,
		.indexSize = 2,
		.indexOffset = (uint32_t)indexAlloc,
		
		.vertexOffset = (uint32_t)vertexAlloc,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	skyboxPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&skyboxInfo, false);

	CreateDebugCommandBuffers(256);
	CreateLightAssignments(128);
	CreateMeshWorldAssignment(256);
	CreateGenericMeshCommandBuffers(256);


	LightSource source1 = { .color = Vector4f(1.0f, 0.0, 0.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, -80.0f, 9.0f) };
	LightSource source2 = { .color = Vector4f(1.0f, 1.0, 1.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, -40.0f, 9.0f) };
	LightSource source4 = { .color = Vector4f(1.0f, 1.0f, 0.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, 40.0f, 9.0f) };

	GlobalRenderer::gRenderInstance->transferPool.Create(&mainSpotLight, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);

	GlobalRenderer::gRenderInstance->transferPool.Create(&source2, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&source3, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::MEMORY);
	GlobalRenderer::gRenderInstance->transferPool.Create(&source4, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&mainDirectionalLight, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&source1, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);


	LightType ltype = LightType::POINT;

	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 5, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 1, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 2, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 3, TransferType::CACHED);

	ltype = LightType::DIRECTIONAL;

	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 4, TransferType::CACHED);

	ltype = LightType::SPOT;



	GlobalRenderer::gRenderInstance->transferPool.Create(&ltype, sizeof(uint32_t), globalLightTypesBuffer, sizeof(uint32_t) * 0, TransferType::CACHED);

	c.CamLookAt(Vector3f(0.0f, 0.0f, 15.0f), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance->GetSwapChainWidth() / (float)GlobalRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, DegToRad(45.0f));

	WriteCameraMatrix(GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	


	CreateCrateObject();

	
	
	
}

#include "TextureIO.h"

static int imageCounter = 10;

void ReadImage(std::string *name, int count)
{

	static char imagemem[64 * MiB];

	size_t offset = 0;

	TextureDetails details;

	for (int i = 0; i < 6; i++)
	{
		std::vector<char> data;
		FileManager::ReadFileInFull(name[i], data);

		

		ParseBMP(data, &details);

		memcpy(imagemem + offset, details.data, details.dataSize);

		offset += details.dataSize;

		delete[] details.data;
	}

	int textureStart = mainDictionary.AllocateNTextureHandles(1);


	mainDictionary.textureHandles[10] =
		GlobalRenderer::gRenderInstance->CreateCubeImageHandle(
			offset,
			details.width,
			details.height,
			details.miplevels,
			details.type,
			loop->GetPoolIndexByFormat(details.type));

	GlobalRenderer::gRenderInstance->imageMemoryUpdateManager.Create(
		imagemem,
		mainDictionary.textureHandles[10],
		nullptr,
		offset,
		details.width,
		details.height,
		1,
		6,
		details.type
	);
}


void ProcessKeys(GenericKeyAction keyActions[KC_COUNT])
{

	loop->camMovements[ApplicationLoop::RIGHT] = (keyActions[KC_D].state == HELD || keyActions[KC_D].state == PRESSED);

	loop->camMovements[ApplicationLoop::LEFT] = (keyActions[KC_A].state == HELD || keyActions[KC_A].state == PRESSED);

	loop->camMovements[ApplicationLoop::FORWARD] = (keyActions[KC_W].state == HELD || keyActions[KC_W].state == PRESSED);
	
	loop->camMovements[ApplicationLoop::BACK] = (keyActions[KC_S].state == HELD || keyActions[KC_S].state == PRESSED);

	loop->camMovements[ApplicationLoop::PITCHUP] = (keyActions[KC_UP].state == HELD || keyActions[KC_UP].state == PRESSED);
	
	loop->camMovements[ApplicationLoop::PITCHDOWN] = (keyActions[KC_DOWN].state == HELD || keyActions[KC_DOWN].state == PRESSED);
	
	loop->camMovements[ApplicationLoop::ROTATEYRIGHT] = (keyActions[KC_RIGHT].state == HELD || keyActions[KC_RIGHT].state == PRESSED);
	
	loop->camMovements[ApplicationLoop::ROTATEYLEFT] = (keyActions[KC_LEFT].state == HELD || keyActions[KC_LEFT].state == PRESSED);


	if (keyActions[KC_TWO].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance->IncreaseMSAA();
	}

	if (keyActions[KC_ONE].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance->DecreaseMSAA();
	}
}

void ApplicationLoop::CleanupRuntime()
{

	stopThreadServer = true;

	OSCloseThread(&threadHandle);

	GlobalRenderer::gRenderInstance->WaitOnRender();

	ThreadManager::DestroyThreadManager();

	delete GlobalRenderer::gRenderInstance;

	cleaned = true;
}

void ApplicationLoop::ProcessCommands()
{
	queueSema.Wait();
	if (!commands.size()) {
		queueSema.Notify();
		return;
	}
	std::vector<std::string> com = std::move(commands.front());
	commands.pop();
	queueSema.Notify();
	if (!com.size()) { std::cerr << "what are you doing\n"; return; }
	auto mapFunc = std::find(commandsStrings.begin(), commandsStrings.end(), com[0]);
	if (mapFunc == std::end(commandsStrings)) return;
	if (com.size() > 1)
		ExecuteCommands(*mapFunc, { com.begin()+1, com.end()});
	else 
		ExecuteCommands(*mapFunc, { });
}



void ApplicationLoop::AddCommandTS(std::vector<std::string>& com)
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

	LoadSMBFile(SMB);

}

void LoadObjectThreaded(void* data);

void ApplicationLoop::LoadThreadedWrapper(std::string& file)
{
	ThreadManager::LaunchOSASyncThread(LoadObjectThreaded, &file);
}

void LoadObjectThreaded(void* data)
{
	std::string* file = (std::string*)data;

	std::string out = *file;

	if (out[0] == 0x22)
	{
		out = out.substr(1, out.length() - 2);
	}

	loop->LoadObject(out);
}

void ApplicationLoop::FindWords(std::string words, std::vector<std::string>& out)
{
	size_t size = words.length();
	int i = 0, j = 1;

	while (j < size) {

		if (words[j] == 0x20)
		{
			out.push_back(words.substr(i, (j - i)));
			i = j+1;
		}
		else if (words[j] == 0x22)
		{
			while (j++ < size && words[j] != 0x22);
		}
		j++;
	}
	out.push_back(words.substr(i, (j - i)));
}

void ScanSTDIN(void* data)
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

	volatile bool* stopToken = (volatile bool*)data;

	while (!*stopToken)
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

		std::vector<std::string> comandargs{};

		loop->FindWords(output, comandargs);

		loop->AddCommandTS(comandargs);

		if (output == "end") break;

		std::osyncstream(std::cout) << "Hit enter and then write command > ";
	}


	return;
}