#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"


#include "SMBTexture.h"
#include "AppAllocator.h"


ApplicationLoop* loop;



std::array<std::string, 4> commandsStrings =
{
	"end",
	"load",
	"positionm",
	"positiong",
};

struct Handles
{
	int vertexFlags;
	int numHandles;
	int stride;
	int indexCount;
	int instanceCount;
	int firstIndex;
	int vertexByteOffset;
	int lightCount;
	int handles[4];
	int lightIndex[4];
	Matrix4f m;
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
	//EntryHandle worldSpaceDivisonView;
	//EntryHandle deviceOffsetsView;
	//EntryHandle deviceCountsView;
}; 

struct LightSource
{
	Vector4f color; //w is theta
	Vector4f pos; //w is radius for point right
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

std::array<int, 125> tempArr;

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

LightSource source3 = { .color = Vector4f(229, 211, 191, 1.0), .pos = wow };

#define MAX_GEOMETRY 2048
#define MAX_MESHES 4096
#define MAX_MESH_TEXTURES 8192


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

static void CreateUniformGrid()
{

	DebugDrawStruct drawStruct;
	

	drawStruct.scale = Vector4f(1.0, 1.0, 1.0, 1.0);
	drawStruct.color = Vector4f(1.0, 0.0, 0.0, 0.0);

	float div = (float)(mainGrid.numberOfDivision);

	int count = 5;

	Vector4f extent = (mainGrid.max - mainGrid.min) / div;

	float xMove = extent.x;
	float yMove = extent.y;
	float zMove = extent.z;

	Vector4f min = mainGrid.max - (extent * 0.5);

	Vector4f half = extent/2.0;


	drawStruct.halfExtents = half;

	std::vector<int> tags(count*count*count);

	std::fill(tags.begin(), tags.end(), 1);

	VKRenderer::gRenderInstance->transferPool.Create(tags.data(), sizeof(uint32_t) * tags.size(), globalDebugTypes, sizeof(uint32_t) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < count; j++)
		{
			for (int g = 0; g < count; g++)
			{
				drawStruct.center = Vector4f(min.x - (i * xMove), min.y - (j * yMove), min.z - (g * zMove), 1.0f);

				VKRenderer::gRenderInstance->transferPool.Create(&drawStruct, sizeof(DebugDrawStruct), debugAllocBuffer, sizeof(DebugDrawStruct) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);
				
				debugIndirectDrawData.commandBufferCount++;
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


void ApplicationLoop::SetPositonOfMesh(int meshIndex, const Vector3f& pos)
{
	auto rendInst = VKRenderer::gRenderInstance;

	Mesh* mesh = &meshInstanceData.dataArray[meshIndex];
	Handles* handles = (Handles*)meshObjectData.dataArray[mesh->meshInstanceLocalMemoryStart];


	int meshSpecificAlloc = meshDeviceMemoryData.dataArray[mesh->meshInstanceDeviceMemoryStart];

	handles->m.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

	rendInst->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::CACHED);

}

void ApplicationLoop::SetPositionOfGeometry(int geomIndex, const Vector3f& pos)
{
	auto rendInst = VKRenderer::gRenderInstance;
	Geometry* geom = &geometryInstanceData.dataArray[geomIndex];
	
	int meshCount = geom->meshCount;
	int meshStart = geom->meshStart;

	for (int i = 0; i < meshCount; i++)
	{
		Mesh* mesh = &meshInstanceData.dataArray[meshStart + i];

		Handles* handles = (Handles*)meshObjectData.dataArray[mesh->meshInstanceLocalMemoryStart];

		int meshSpecificAlloc = meshDeviceMemoryData.dataArray[mesh->meshInstanceDeviceMemoryStart];

		handles->m.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

		rendInst->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::CACHED);
	}

}

void ApplicationLoop::ExecuteCommands(const std::string& command, const std::vector<std::string>& args)
{

	if (command == "load")
	{
		LoadThreadedWrapper(args.at(0));
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

		LoadObject(args.inputFile.string());

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

		VKRenderer::gRenderInstance->EndFrame();

		static int what2 = 0;

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

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.preWorldSpaceDivisionPipeline, 0);

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.prefixSumPipeline, 0);

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(lightAssignment.postWorldSpaceDivisionPipeline, 0);

				updateLights--;
			}

			if (cameraMove || commandCountPrev != mainIndirectDrawData.commandBufferCount || updatedCommand > 0)
			{
				if (!updatedCommand || cameraMove) {
					updatedCommand = 3;
					VKRenderer::gRenderInstance->transferCommandPool.Create(8, mainIndirectDrawData.commandBufferCountAlloc, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
					//VKRenderer::gRenderInstance->transferCommandPool.Create(4, mainIndirectDrawData.commandBufferCountAlloc, 4, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE);
				}

				updatedCommand--;
				 
				commandCountPrev = mainIndirectDrawData.commandBufferCount;
				
				VKRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.preWorldSpaceDivisionPipeline, 0);

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.prefixSumPipeline, 0);

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(worldSpaceAssignment.postWorldSpaceDivisionPipeline, 0);
				

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(mainIndirectDrawData.indirectCullPipeline, 0);

				
			}


		

			if (cameraMove || debugIndirectDrawData.commandBufferCount != debugCommandCountPrev || updatedDebugCommand > 0)
			{
				if (!updatedDebugCommand || cameraMove)
				{
					VKRenderer::gRenderInstance->transferCommandPool.Create(8, debugIndirectDrawData.commandBufferCountAlloc, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
					//VKRenderer::gRenderInstance->transferCommandPool.Create(4, debugIndirectDrawData.commandBufferCountAlloc, 4, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE);
					updatedDebugCommand = 3;
				}
				debugCommandCountPrev = debugIndirectDrawData.commandBufferCount;
				VKRenderer::gRenderInstance->AddPipelineToMainQueue(debugIndirectDrawData.indirectCullPipeline, 0);
				updatedDebugCommand--;
			}
			


			if (mainWindow->windowData.info.HandleResizeRequested())
			{
				VKRenderer::gRenderInstance->RecreateSwapChain();
				c.CreateProjectionMatrix(VKRenderer::gRenderInstance->GetSwapChainWidth() / (float)VKRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, DegToRad(45.0f));
				UpdateCameraMatrix();
				continue;
			}

			//UpdateThisThing();

			auto index = VKRenderer::gRenderInstance->BeginFrame();

			if (index != ~0ui32) {

				

				VKRenderer::gRenderInstance->AddPipelineToMainQueue(debugIndirectDrawData.indirectDrawPipeline, 1);

				VKRenderer::gRenderInstance->DrawScene(index);

				VKRenderer::gRenderInstance->SubmitFrame(index);
			}
			
			VKRenderer::gRenderInstance->EndFrame();	

			ProcessCommands();

			ThreadManager::ASyncThreadsDone();

			int what = fps();

			static float x = 230.0f;
			
			source3.pos.x = wow.x + cosf(DegToRad(-90.0f)) * 5.0f;
			source3.pos.y = 0.0f;
			source3.pos.z = wow.z + -sinf(DegToRad(-90.0f)) * 10.0f;

			VKRenderer::gRenderInstance->transferPool.Create(&source3, sizeof(LightSource), globalLightBuffer, sizeof(LightSource) * 2, TransferType::MEMORY);

			x += 0.01f;

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

	auto rendInst = VKRenderer::gRenderInstance;

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
	//VKRenderer::gRenderInstance->UpdateAllocation(&c.View, globalBufferLocation, (sizeof(Matrix4f) * 3) + sizeof(Frustrum), 0, 0, frame);
	VKRenderer::gRenderInstance->transferPool.Create(&c.View, (sizeof(Matrix4f) * 3) + sizeof(Frustrum), globalBufferLocation, 0, TransferType::MEMORY);
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


enum VertexComponents
{
	POSITION = 1,
	TEXTURES1 = 2,
	TEXTURES2 = 4,
	TEXTURES3 = 8,
	NORMAL = 16,
	BONES2 = 32,
	COMPRESSED = 0x80000000,
};

std::atomic<float> geoX = 0.0f;

void ApplicationLoop::SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file)
{
	auto rendInst = VKRenderer::gRenderInstance;

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

		Mesh* mesh = nullptr;

		uint32_t meshIndex;

		std::tie(meshIndex, mesh) = meshInstanceData.Allocate();

		geom->meshCount++;

		int indexCount = geoDef->indicesCount[i];

		int vertexCount = geoDef->verticesCount[i];

		mesh->indicesCount = indexCount;

		mesh->verticesCount = vertexCount;

		int vertexSize = 0;

		void* vertexData;

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



		mesh->vertexSize = vertexSize;



		vertexData = (void*)vertexAndIndicesAlloc.Allocate(vertexSize * vertexCount);

		mesh->vertexId = meshVertexData.Allocate(vertexData);

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



		mesh->indexSize = 2;

		uint16_t* indices = (uint16_t*)vertexAndIndicesAlloc.Allocate(sizeof(uint16_t) * indexCount);

		mesh->indexId = meshIndexData.Allocate(indices);

		SMBCopyIndices(geoDef, i, file, indices);

		int indexAlloc = indexBufferAlloc.Allocate(sizeof(uint16_t) * indexCount, 1);

		rendInst->UpdateAllocation(indices, globalIndexBuffer, sizeof(uint16_t) * indexCount, indexAlloc, 0, 1);

		rendInst->UpdateAllocation(vertexData, globalVertexBuffer, vertexSize * vertexCount, vertexAlloc, 0, 1);

		mesh->meshInstanceLocalMemoryCount = 1;

		Handles* handles = (Handles*)meshObjectSpecificAlloc.Allocate(sizeof(Handles));

		mesh->meshInstanceLocalMemoryStart = meshObjectData.Allocate(handles);

		handles->numHandles = geoDef->materialsCount[i];

		int base = geoDef->materialStart[i];

		mesh->texturesStart = meshTextureHandles.AllocateN(handles->numHandles);
		mesh->texuresCount = handles->numHandles;

		mesh->vertexId = globalVertexBuffer;
		mesh->indexId = globalIndexBuffer;

		handles->vertexFlags = vertexFlags;
		handles->stride = vertexSize;

		handles->indexCount = mesh->indicesCount;
		handles->instanceCount = 1;
		handles->firstIndex = indexAlloc / mesh->indexSize;
		handles->vertexByteOffset = vertexAlloc;

		for (int ii = 0; ii < handles->numHandles; ii++)
		{
			handles->handles[ii] = geoDef->materialsId[base + ii];
			if (handles->handles[ii] == -1) handles->handles[ii] = 0;
			meshTextureHandles.Update(mesh->texturesStart + ii, handles->handles[ii]);
		}

		memcpy(&handles->minMaxBox, &geoDef->axialBox, sizeof(AxisBox));
		memcpy(&handles->m, geomSpecificData, sizeof(Matrix4f));
		memcpy(&handles->sphere, &geoDef->spheres[i], sizeof(Vector4f));

		int meshSpecificAlloc = meshDeviceSpecificAlloc.Allocate(sizeof(Handles), 1);

		mesh->meshInstanceDeviceMemoryCount = 1;
		mesh->meshInstanceDeviceMemoryStart = meshDeviceMemoryData.Allocate(meshSpecificAlloc);

		rendInst->transferPool.Create(handles, sizeof(Handles), globalMeshLocation, meshSpecificAlloc, TransferType::MEMORY);

	
		
		VkDrawIndirectCommand command;

		command.firstInstance = 0;
		command.firstVertex = 0;
		command.instanceCount = 1;
		command.vertexCount = vertexCount * 2;

		rendInst->transferPool.Create(&command, sizeof(VkDrawIndirectCommand), normalDebugAlloc, sizeof(VkDrawIndirectCommand) * i, TransferType::CACHED);


		


	}


	

	int tag = 2;

	VKRenderer::gRenderInstance->transferPool.Create(&tag, sizeof(uint32_t), globalDebugTypes, sizeof(uint32_t) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);


	DebugDrawStruct drawStruct;
	drawStruct.center = ((*geomSpecificData * geoDef->axialBox.max - *geomSpecificData * geoDef->axialBox.min) * 0.5 + (*geomSpecificData * geoDef->axialBox.min));

	drawStruct.scale = Vector4f(1.0, 1.0, 1.0, 1.0);
	drawStruct.color = Vector4f(1.0, 0.0, 1.0, 0.0);


	Vector4f half = ((geoDef->axialBox.max - geoDef->axialBox.min) * 0.5);
	Vector3f half3 = Vector3f(half.x, half.y, half.z);

	float r = Length(half3);

	drawStruct.halfExtents = Vector4f(r, std::bit_cast<float, uint32_t>(24), 1.0, 1.0);


	VKRenderer::gRenderInstance->transferPool.Create(&drawStruct, sizeof(DebugDrawStruct), debugAllocBuffer, sizeof(DebugDrawStruct) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	

	debugIndirectDrawData.commandBufferCount++;


	tag = 1;

	VKRenderer::gRenderInstance->transferPool.Create(&tag, sizeof(uint32_t), globalDebugTypes, sizeof(uint32_t) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	drawStruct.scale = Vector4f(1.0, 1.0, 1.0, 1.0);
	drawStruct.color = Vector4f(0.5, 0.5, 1.0, 0.0);


	drawStruct.halfExtents = (*geomSpecificData * geoDef->axialBox.max - *geomSpecificData * geoDef->axialBox.min) * 0.5;

	VKRenderer::gRenderInstance->transferPool.Create(&drawStruct, sizeof(DebugDrawStruct), debugAllocBuffer, sizeof(DebugDrawStruct) * debugIndirectDrawData.commandBufferCount, TransferType::CACHED);

	debugIndirectDrawData.commandBufferCount++;

	mainIndirectDrawData.commandBufferCount += count;
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

		mainDictionary.textureHandles[ii + index] =
			VKRenderer::gRenderInstance->CreateImage(
				(char*)texture.data,
				texture.imageSizes,
				texture.cumulativeSize,
				texture.width,
				texture.height,
				texture.miplevels,
				format,
				GetPoolIndexByFormat(format));
	}

	BindlessSamplerUpdate update; 

	update.samplercount = totalTextureCount;
	update.begdestinationslot = index;
	update.handles = mainDictionary.textureHandles.data() + index;

	VKRenderer::gRenderInstance->descriptorUpdatePool.Create(globalTexturesDescriptor, 0, ShaderResourceType::SAMPLERBINDLESS, &update);
	
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


void ApplicationLoop::InitializeRuntime()
{

	queueSema.Create();

	ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN),
				this, std::placeholders::_1));

	mainDictionary.textureCache = (uintptr_t)mainTextureCacheMemory;
	
	mainDictionary.textureSize = sizeof(mainTextureCacheMemory);

	mainWindow = new WindowManager();

	mainWindow->CreateMainWindow();

	VKRenderer::gRenderInstance = new RenderInstance();

	VKRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow);

	CreateTexturePools();

	globalBufferLocation = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer((sizeof(Matrix4f) * 3) + sizeof(Frustrum), alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);
	globalIndexBuffer = VKRenderer::gRenderInstance->GetAllocFromDeviceBuffer(globalIndexBufferSize, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT);
	globalVertexBuffer = VKRenderer::gRenderInstance->GetAllocFromDeviceStorageBuffer(globalVertexBufferSize, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT);
	globalBufferDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	globalMeshLocation = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(globalMeshSize, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalBufferDescriptor, globalBufferLocation, 0, 0);

	std::array arr = { globalBufferDescriptor, globalTexturesDescriptor };

	VKRenderer::gRenderInstance->CreateRenderTargetData(arr.data(), 2);

	mainIndirectDrawData.commandBufferSize = 64;
	mainIndirectDrawData.commandBufferCount = 0;

	mainIndirectDrawData.commandBufferAlloc = VKRenderer::gRenderInstance->GetAllocFromDeviceBuffer(sizeof(VkDrawIndexedIndirectCommand) * 64, alignof(VkDrawIndexedIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);
	
	mainIndirectDrawData.commandBufferCountAlloc = VKRenderer::gRenderInstance->GetAllocFromDeviceStorageBuffer(sizeof(uint32_t) * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	mainIndirectDrawData.indirectCullDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(4, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	mainIndirectDrawData.indirectGlobalIDsAlloc = VKRenderer::gRenderInstance->GetAllocFromDeviceStorageBuffer(sizeof(uint32_t) * mainIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);


	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.commandBufferAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, globalMeshLocation, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, globalBufferLocation, 2, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 3, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, mainIndirectDrawData.commandBufferCountAlloc, 4, 0);
	
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainGrid, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);
	
	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 4, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	


	

	debugAllocBuffer = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(debugAllocBufferSize, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	
	
	debugIndirectDrawData.commandBufferCount = 0;
	debugIndirectDrawData.commandBufferSize = 128;



	debugIndirectDrawData.commandBufferAlloc = VKRenderer::gRenderInstance->GetAllocFromDeviceBuffer(sizeof(VkDrawIndirectCommand) * debugIndirectDrawData.commandBufferSize, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	debugIndirectDrawData.indirectGlobalIDsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	globalDebugTypes = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	
	
	debugIndirectDrawData.commandBufferCountAlloc = VKRenderer::gRenderInstance->GetAllocFromDeviceStorageBuffer(sizeof(uint32_t) * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);


	debugIndirectDrawData.indirectCullDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(6, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.commandBufferAlloc, 0, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.indirectGlobalIDsAlloc, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, globalDebugTypes, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, globalBufferLocation, 3, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugAllocBuffer, 4, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, debugIndirectDrawData.commandBufferCountAlloc, 5, 0);

	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCount, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 5, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);


	std::array debugCullDescriptors = { debugIndirectDrawData.indirectCullDescriptor };

	ShaderComputeLayout* debugCullDescriptorLayout = VKRenderer::gRenderInstance->GetComputeLayout(6);

	ComputeIntermediaryPipelineInfo debugCullPipelineCreate = {
			.x = 4096 / debugCullDescriptorLayout->x,
			.y = 1,
			.z = 1,
			.pipelinename = 6,
			.descCount = 1,
			.descriptorsetid = debugCullDescriptors.data()
	};


	debugIndirectDrawData.indirectCullPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&debugCullPipelineCreate);

	debugIndirectDrawData.indirectDrawDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(5, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectDrawDescriptor, debugAllocBuffer, 0, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, debugIndirectDrawData.indirectGlobalIDsAlloc, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, globalDebugTypes, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

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


	debugIndirectDrawData.indirectDrawPipeline =  VKRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&indirectDebugDrawPipelineCreate, false);




	worldSpaceAssignment.totalElementsCount = 125;
	worldSpaceAssignment.totalSumsNeeded = (int)ceil(worldSpaceAssignment.totalElementsCount / 2048.0f);


	 worldSpaceAssignment.prefixSumDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	 worldSpaceAssignment.deviceOffsetsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	 worldSpaceAssignment.deviceCountsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);
	 
	 worldSpaceAssignment.deviceSumsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);
	


	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceCountsAlloc, 0, 0);
	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceOffsetsAlloc, 1, 0);
	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, worldSpaceAssignment.deviceSumsAlloc, 2, 0);

	 VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	 VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	 VKRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.totalElementsCount, 0);



	 std::array prefixSumDescriptor = { worldSpaceAssignment.prefixSumDescriptors };

	 ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			 .x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
			 .y = 1,
			 .z = 1,
			 .pipelinename = 7,
			 .descCount = 1,
			 .descriptorsetid = prefixSumDescriptor.data()
	 };

	 

	 worldSpaceAssignment.prefixSumPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);
	 

	 worldSpaceAssignment.sumAfterDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 0, 0);
	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 1, 0);
	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, worldSpaceAssignment.deviceSumsAlloc, 2, 0);
	 VKRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.totalSumsNeeded, 0);

	 VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	 VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	
	 std::array prefixSumOverflowDescriptor = { worldSpaceAssignment.sumAfterDescriptors, };

	 ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
			 .x = (uint32_t)ceil(worldSpaceAssignment.totalSumsNeeded/2048.0f),
			 .y = 1,
			 .z = 1,
			 .pipelinename = 7,
			 .descCount = 1,
			 .descriptorsetid = prefixSumOverflowDescriptor.data()
	 };

	 worldSpaceAssignment.sumAfterPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);
	


	 worldSpaceAssignment.sumAppliedToBinDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(8, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, worldSpaceAssignment.deviceOffsetsAlloc, 0, 0);
	 VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, worldSpaceAssignment.deviceSumsAlloc, 1, 0);
	 VKRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

	 VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



	 std::array incrementSumsDescriptor = { worldSpaceAssignment.sumAppliedToBinDescriptors, };

	 ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
			 .x = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / 2048.0f),
			 .y = 1,
			 .z = 1,
			 .pipelinename = 8,
			 .descCount = 1,
			 .descriptorsetid = incrementSumsDescriptor.data()
	 };

	 worldSpaceAssignment.sumAppliedToBinPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);


	worldSpaceAssignment.worldSpaceDivisionAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * worldSpaceAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	worldSpaceAssignment.preWorldSpaceDivisionDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(9, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, worldSpaceAssignment.deviceCountsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, globalMeshLocation, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { worldSpaceAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 9,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	worldSpaceAssignment.preWorldSpaceDivisionPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);


	worldSpaceAssignment.postWorldSpaceDivisionDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(10, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, worldSpaceAssignment.deviceOffsetsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, globalMeshLocation, 1, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, worldSpaceAssignment.worldSpaceDivisionAlloc, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	//VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, worldSpaceAssignment.worldSpaceDivisionAlloc, 2, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { worldSpaceAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 10,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	worldSpaceAssignment.postWorldSpaceDivisionPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);

	globalLightBuffer = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(globalLightBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	globalLightTypesBuffer = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(globalLightTypesBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	lightAssignment.totalElementsCount = 125;
	lightAssignment.totalSumsNeeded = (int)ceil(lightAssignment.totalElementsCount / 2048.0f);


	lightAssignment.prefixSumDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	lightAssignment.deviceOffsetsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	lightAssignment.deviceCountsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	lightAssignment.deviceSumsAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * lightAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);



	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceCountsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceOffsetsAlloc, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, lightAssignment.deviceSumsAlloc, 2, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.prefixSumDescriptors, &lightAssignment.totalElementsCount, 0);



	std::array prefixSumDescriptor2 = { lightAssignment.prefixSumDescriptors };

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix2 = {
			.x = (uint32_t)lightAssignment.totalSumsNeeded,
			.y = 1,
			.z = 1,
			.pipelinename = 7,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor2.data()
	};



	lightAssignment.prefixSumPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix2);





	/*
	lightAssignment.sumAfterDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(7, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, lightAssignment.deviceSumsAlloc, 2, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAfterDescriptors, &lightAssignment.totalSumsNeeded, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array prefixSumOverflowDescriptor = { lightAssignment.sumAfterDescriptors, };

	ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
			.x = (uint32_t)ceil(lightAssignment.totalSumsNeeded / 2048.0f),
			.y = 1,
			.z = 1,
			.pipelinename = 7,
			.descCount = 1,
			.descriptorsetid = prefixSumOverflowDescriptor.data()
	};

	lightAssignment.sumAfterPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);



	lightAssignment.sumAppliedToBinDescriptors = VKRenderer::gRenderInstance->AllocateShaderResourceSet(8, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, lightAssignment.deviceOffsetsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, lightAssignment.deviceSumsAlloc, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.totalElementsCount, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



	std::array incrementSumsDescriptor = { lightAssignment.sumAppliedToBinDescriptors, };

	ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
			.x = (uint32_t)ceil(lightAssignment.totalElementsCount / 2048.0f),
			.y = 1,
			.z = 1,
			.pipelinename = 8,
			.descCount = 1,
			.descriptorsetid = incrementSumsDescriptor.data()
	};

	

	lightAssignment.sumAppliedToBinPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);
	*/

	lightAssignment.worldSpaceDivisionAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(uint32_t) * lightAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT);

	lightAssignment.preWorldSpaceDivisionDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(11, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, lightAssignment.deviceCountsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, globalLightBuffer, 1, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor2 = { lightAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline2 = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 11,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor2.data()
	};

	lightAssignment.preWorldSpaceDivisionPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline2);


	lightAssignment.postWorldSpaceDivisionDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(12, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, lightAssignment.deviceOffsetsAlloc, 0, 0);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, globalLightBuffer, 1, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.postWorldSpaceDivisionDescriptor, lightAssignment.worldSpaceDivisionAlloc, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	//VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, lightAssignment.worldSpaceDivisionAlloc, 2, 0);
	VKRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	VKRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor2 = { lightAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline2 = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 12,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor2.data()
	};

	lightAssignment.postWorldSpaceDivisionPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline2);

	int cullLightDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(4, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	
	
	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.worldSpaceDivisionAlloc, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.deviceOffsetsAlloc, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, lightAssignment.deviceCountsAlloc, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(cullLightDescriptor, globalLightBuffer, 3, 0);


	ShaderComputeLayout* layout = VKRenderer::gRenderInstance->GetComputeLayout(4);

	std::array computeDescriptors = { mainIndirectDrawData.indirectCullDescriptor, cullLightDescriptor };

	ComputeIntermediaryPipelineInfo mainCullComputeSetup = {
			.x = 4096 / layout->x,
			.y = 1,
			.z = 1,
			.pipelinename = 4,
			.descCount = 2,
			.descriptorsetid = computeDescriptors.data()
	};

	mainIndirectDrawData.indirectCullPipeline = VKRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&mainCullComputeSetup);




	mainIndirectDrawData.indirectDrawDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalMeshLocation, 0, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalVertexBuffer, 1, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, globalLightBuffer, 3, 0);

	std::array<int, 1> indirectDrawDescriptors = {
		mainIndirectDrawData.indirectDrawDescriptor,

	};

	
	normalDebugAlloc = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(VkDrawIndirectCommand)*10, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT);

	int normalDrawDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(13, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(normalDrawDescriptor, globalMeshLocation, 0, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(normalDrawDescriptor, globalVertexBuffer, 1, 0);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferView(normalDrawDescriptor, mainIndirectDrawData.indirectGlobalIDsAlloc, 2, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

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

	//int what = VKRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&normalDrawCreate, true);
	

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

	mainIndirectDrawData.indirectDrawPipeline = VKRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&indirectDrawCreate, true);

	LightSource source1 = { .color = Vector4f(1.0f, 0.0, 0.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, -80.0f, 9.0f) };
	LightSource source2 = { .color = Vector4f(1.0f, 1.0, 1.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, -40.0f, 9.0f) };
	LightSource source4 = { .color = Vector4f(1.0f, 1.0f, 0.0, 0.0f), .pos = Vector4f(-5.0f, 0.0f, 40.0f, 9.0f) };
	

	VKRenderer::gRenderInstance->transferPool.Create(&source1, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	VKRenderer::gRenderInstance->transferPool.Create(&source2, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	VKRenderer::gRenderInstance->transferPool.Create(&source3, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::MEMORY);
	VKRenderer::gRenderInstance->transferPool.Create(&source4, sizeof(LightSource), globalLightBuffer, sizeof(LightSource)* globalLightCount++, TransferType::CACHED);
	
	c.CamLookAt(Vector3f(0.0f, 0.0f, 15.0f), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(VKRenderer::gRenderInstance->GetSwapChainWidth() / (float)VKRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, DegToRad(45.0f));
	
	WriteCameraMatrix(VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
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
		VKRenderer::gRenderInstance->IncreaseMSAA();
	}

	if (keyActions[KC_ONE].state == PRESSED)
	{
		VKRenderer::gRenderInstance->DecreaseMSAA();
	}
}

void ApplicationLoop::CleanupRuntime()
{
	VKRenderer::gRenderInstance->WaitOnRender();

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

void ApplicationLoop::LoadThreadedWrapper(const std::string file)
{
	ThreadManager::LaunchASyncThread(std::bind(std::mem_fn(&ApplicationLoop::LoadObjectThreaded), this, std::placeholders::_1, file));
}

void ApplicationLoop::LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string file)
{

	//std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	std::string out = file;

	if (file[0] == 0x22)
	{
		out = file.substr(1, file.length() - 2);
	}



	this->LoadObject(out);

	flag->store(true);
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

		std::vector<std::string> comandargs{};

		FindWords(output, comandargs);

		this->AddCommandTS(comandargs);

		if (output == "end") break;

		std::osyncstream(std::cout) << "Hit enter and then write command > ";
	}
}