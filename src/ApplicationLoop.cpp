#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"
#include "Exporter.h"
#include "Logger.h"
#include "TextureDictionary.h"
#include "SMBTexture.h"
#include "AppAllocator.h"

#include <cassert>

ApplicationLoop* loop;

#define MAX_GEOMETRY 2048
#define MAX_MESHES 4096
#define MAX_MESH_TEXTURES 8192
#define MAX_IMAGE_DIM 4096

enum PipelineHandles
{
	GENERIC = 0,
	TEXT,
	INTERPOLATE,
	POLYNOMIAL,
	RENDEROBJCULL,
	DEBUGDRAW,
	DEBUGCULL,
	PREFIXSUM,
	PREFIXADD,
	WORLDORGANIZE,
	WORLDASSIGN,
	LIGHTORGANIZE,
	LIGHTASSIGN,
	NORMALDEBUGDRAW,
	SKYBOX,
	OUTLINE
};

static char StartupMemory[32 * KiB];
static SlabAllocator StartupMemoryAllocator { StartupMemory, sizeof(StartupMemory) };

std::array<std::string, 3> commandsStrings =
{
	"end",
	"load",
	"positiong",
};

static std::array<StringView, 8> pds = {
	StartupMemoryAllocator.AllocateFromNullStringCopy("GenericPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("TextPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("DebugPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("NormalDebugPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("SkyboxPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("OutlinePipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("FullscreenPipeline.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMap.xml")
};

static std::array<StringView, 19> layouts = {
		StartupMemoryAllocator.AllocateFromNullStringCopy("3DTexturedLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("TextLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("InterpolateMeshLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PolynomialLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("IndirectCull.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("DebugDraw.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("IndirectDebug.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PrefixSum.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PrefixSumAdd.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("WorldObjectDivison.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("MeshWorldAssignments.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("LightObjectDivision.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("LightWorldAssignment.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("NormalDebug.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("Skybox.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("OutlineLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("FullscreenLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMapLayout.xml"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMapClipping.xml")
};

static std::array<StringView, 5> mainLayoutAttachments =
{
	StartupMemoryAllocator.AllocateFromNullStringCopy("MSAAPostProcess.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("BasicShadowMapAtt.xml"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowAtlasUse.xml")
};

enum MaterialFlags
{
	ALPHACUTTOFFMAP = 1,
	TANGENTNORMALMAPPED = 2,
	ALBEDOMAPPED = 4,
	WORLDNORMALMAPPED = 8,
	VERTEXNORMAL = 16
};

struct GPUMaterial
{
	int materialFlags;
	float shininess;
	int unused2;
	int unused3;
	Vector4f albedoColor;
	Vector4ui textureHandles; // y - normalMapCoordinates // x- albedo
	Vector4f emissiveColor;
	Vector4f specularColor;
};

struct GPUMeshRenderable
{
	int meshIndex;
	int lightCount;
	int instanceCount;
	int geomIndex;
	int lightIndices[4];
	int materialStart;
	int blendLayersStart;
	int materialCount;
	int pad2;
	Matrix4f transform;
};

struct GPUMeshDetails
{
	int vertexFlags;
	int stride;
	int indexCount;
	int firstIndex;
	int vertexByteOffset;
	int pad1;
	int pad2;
	int pad3;
	Sphere sphere;
};

struct GPUGeometryDetails
{
	AxisBox minMaxBox;
};

struct GPUGeometryRenderable
{
	int geomDescIndex;
	int renderableStart;
	int renderableCount;
	int pad1;
	Matrix4f transform;
};

enum BlendMaterialType
{
	ConstantAlpha = 1,
	BlendMap = 2,
};

struct GPUBlendDetails
{
	BlendMaterialType type;
	union {
		float alphaBlend;
		int alphaMapHandle;
	};
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

struct GPULightSource
{
	Vector4f color; //w is intensity;
	Vector4f pos; //w is radius for point right
	Vector4f direction; //w is unused;
	Vector4f ancillary; //for spot, x, y are cosine theta cutoffs
};

enum class DebugDrawType
{
	DEBUGBOX = 1,
	DEBUGSPHERE = 2,
};

struct GPUDebugRenderable
{
	Vector4f center; // fourth component is radius for sphere type
	Vector4f scale;
	Vector4f color;
	Vector4f halfExtents;
};


struct ShadowMapBase
{
	int frameGraphIndex;
	int resourceIndex;
	int atlasWidth;
	int atlasHeight;
	int avgShadowWidth;
	int avgShadowHeight;
	int totalShadowMaps;
	int zoneAlloc;

	int shadowMapCountsAlloc;
	int shadowMapCountsAllocSize;
	int shadowMapOffsetsAlloc;
	int shadowMapOffsetsAllocSize;
	int shadowMapAssignmentsAlloc;
	int shadowMapAssignmentsAllocSize;
	int shadowMapAtlasViewsAlloc;
	int shadowMapAtlasViewsAllocSize;
	int shadowMapViewProjAlloc;
	int shadowMapViewProjAllocSize;
	int shadowMapObjectIDsAlloc;
	int shadowMapObjectIDsAllocSize;
	int shadowMapObjectCountAlloc;
	int shadowMapIndirectBufferAlloc;
	int shadowMapIndirectBufferAllocSize;

	int shadowClippingDescriptor1;
	int shadowClippingDescriptor2;
	int shadowClippingPipeline;
};


struct ShadowMapDebugPipelineData
{
	int fullScreenPipeline;
	int fullScreenDescriptorSet;
	int shadowMapPipeline;
	int shadowMapDescriptorSet;
};

struct ShadowMapView
{
	float xOff;
	float yOff;
	float xScale;
	float yScale;
};

struct GeometryCPUData
{
	int meshCount;
	int meshStart;
};

struct MeshCPUData
{
	int vertexFlags;
	int verticesCount;
	int vertexSize;
	int indicesCount;
	int indexSize;
	int deviceIndices;
	int deviceVertices;
	int meshDeviceIndex;
};

struct BlendRanges
{
	int blendStart;
	int blendCount;
};

struct MaterialRanges
{
	int materialsStart;
	int materialsCount;
};

struct RenderableGeomCPUData
{
	int geomIndex;
	int geomInstanceLocalMemoryCount;
	int geomInstanceLocalMemoryStart;
	int renderableMeshCount; 
	int renderableMeshStart;
};

struct RenderableMeshCPUData
{
	int meshIndex;
	int meshInstanceLocalMemoryCount;
	int meshInstanceLocalMemoryStart;
	int renderableIndex;
	int meshMaterialRangeIndex;
	int meshMaterialCount;
	int meshBlendRangeIndex;
	int meshBlendRangeCount;
	int instanceCount;
};

struct UniformGrid
{
	Vector4f max;
	Vector4f min;
	int numberOfDivision;
};

static EntryHandle mainPresentationSwapChain = EntryHandle();
static int mainHostBuffer = -1;
static int mainDeviceBuffer = -1;

static IndirectDrawData mainIndirectDrawData;
static IndirectDrawData debugIndirectDrawData;

static WorldSpaceGPUPartition worldSpaceAssignment;
static WorldSpaceGPUPartition lightAssignment;


static int globalIndexBuffer = 0;
static int globalIndexBufferSize = 1024 * KiB;
static int globalVertexBuffer = 0;
static int globalVertexBufferSize = 1024 * KiB;

static int globalLightBufferSize = 1024 * KiB;
static int globalLightMax = globalLightBufferSize / sizeof(GPULightSource);
static int globalLightTypesBufferSize = globalLightMax * sizeof(LightType);

static int globalLightTypesBuffer = 0;
static int globalLightBuffer = 0;
static int globalLightCount = 0;

static int normalDebugAlloc;

static int globalDebugStructAlloc = 0;
static int globalDebugStructAllocSize = 10 * KiB;
static int globalDebugTypesAlloc = 0;
static int globalDebugStructCount = 0;

static int globalBufferLocation;
static int globalBufferDescriptor;
static int globalTexturesDescriptor;

static int globalMeshLocation;
static int globalMeshSize = 24 * KiB;
static int globalMeshCount = 0;

static int globalGeometryDescriptionsLocation;
static int globalGeometryDescriptionsSize = 1 * KiB;
static int globalGeometryDescriptionsCount = 0;

static int globalGeometryRenderableLocation;
static int globalGeometryRenderableSize = 1 * KiB;
static int globalGeometryRenderableCount = 0;

static int globalRenderableLocation;
static int globalRenderableSize = 24 * KiB;
static int globalRenderableCount = 0;

static int globalMaterialIndicesLocation;
static int globalMaterialIndicesSize = 4 * KiB;
static int globalMaterialRangeCount = 0;

static int globalMaterialsLocation; 
static int globalMaterialsSize = 4 * KiB;
static int globalMaterialsIndex = 0;

static int globalBlendDetailsLocation;
static int globalBlendRangesLocation;
static int globalBlendDetailsSize = 1 * KiB;
static int globalBlendRangesSize = 1 * KiB;
static int globalBlendDetailCount = 0;
static int globalBlendRangeCount = 0;

static int globalMaterialsMax = globalMaterialsSize / sizeof(GPUMaterial);
static int globalBlendDetailMax = globalBlendDetailsSize / sizeof(GPUBlendDetails);
static int globalBlendRangeMax = globalBlendRangesSize / sizeof(BlendRanges);
static int globalMaterialRangeMax = globalMaterialIndicesSize / sizeof(uint32_t);
static int globalRenderableMax = globalRenderableSize / sizeof(GPUMeshRenderable);
static int globalDebugStructMaxCount = globalDebugStructAllocSize / sizeof(GPUDebugRenderable);
static int globalMeshCountMax = globalMeshSize / sizeof(GPUMeshDetails);
static int globalGeometryDescriptionsCountMax = globalGeometryDescriptionsSize / sizeof(GPUGeometryDetails);
static int globalGeometryRenderableCountMax = globalGeometryRenderableSize / sizeof(GPUGeometryRenderable);

static int shadowMapIndex = 0;

static char vertexAndIndicesMemory[16 * MiB];
static char meshObjectSpecificMemory[4 * KiB];
static char geometryObjectSpecificMemory[4 * KiB];
static char mainTextureCacheMemory[256 * MiB];
static char mainOSDataManagement[16 * KiB];

static SlabAllocator osAllocator(mainOSDataManagement, sizeof(mainOSDataManagement));
static SlabAllocator vertexAndIndicesAlloc(vertexAndIndicesMemory, sizeof(vertexAndIndicesMemory));
static SlabAllocator meshObjectSpecificAlloc(meshObjectSpecificMemory, sizeof(meshObjectSpecificMemory));
static SlabAllocator geometryObjectSpecificAlloc(geometryObjectSpecificMemory, sizeof(geometryObjectSpecificMemory));

static DeviceSlabAllocator indexBufferAlloc(globalIndexBufferSize);

static DeviceSlabAllocator vertexBufferAlloc(globalVertexBufferSize);

static TextureDictionary mainDictionary;

static OSThreadHandle threadHandle;

static int currentFrameGraphIndex = 4;
static int mainComputeQueueIndex = 0;
static int mainFullScreenPipeline = 0;

static GPULightSource mainPointLight = { 
	.color = Vector4f(229, 211, 191, 130.0), 
	.pos = Vector4f(0.0f, 5.0f, 0.0f, 15.0f),
	.direction= Vector4f(0.54,0.75,0.38,0.0), 
	.ancillary= Vector4f(0.04,0.07,0.03,0.0) 
};

static GPULightSource mainDirectionalLight =
{
	Vector4f(229.0, 211.0, 191.0, 2.0),
	Vector4f(0.0,0.0,0.0,0.0),
	Vector4f(.75, -0.33, -.25, 1.0),
	Vector4f(0.001,0.0018,0.0012,0.0),
};

static GPULightSource mainSpotLight =
{
	Vector4f(229.0, 211.0, 191.0, 50.5),
	Vector4f(-10.0,5.0, 4.0,15.0),
	Vector4f(0.0, 0.0, -1.0, 0.0),
	Vector4f(DegToRad(0.0),DegToRad(10.0),0.0,0.0),
};

static ArrayAllocator<void*, MAX_MESHES> geometryObjectData{};
static ArrayAllocator<MeshCPUData, MAX_MESHES> meshCPUData{};
static ArrayAllocator<GeometryCPUData, MAX_GEOMETRY> geometryCPUData{};
static ArrayAllocator<RenderableGeomCPUData, MAX_GEOMETRY> renderablesGeomObjects{};
static ArrayAllocator<RenderableMeshCPUData, MAX_MESHES> renderablesMeshObjects{};
static ArrayAllocator<int, MAX_MESH_TEXTURES> meshTextureHandles{};

static void ProcessKeys(GenericKeyAction keyActions[KC_COUNT]);

static UniformGrid mainGrid = {	
	.max = Vector4f(100.0f, 50.0f, 100.0f, 0.0),
	.min = Vector4f(-100.0f, -50.0f, -100.0f, 0.0),
	.numberOfDivision = 5,
};

static int skyboxPipeline = 0;
static EntryHandle skyboxCubeImage = EntryHandle();

static char RenderInstanceMemoryPool[256 * MiB];
static SlabAllocator RenderInstanceMemoryAllocator{ RenderInstanceMemoryPool, sizeof(RenderInstanceMemoryPool) };

static char RenderInstanceTemporaryPool[64 * KiB];
static RingAllocator RenderInstanceTemporaryAllocator{ RenderInstanceTemporaryPool, sizeof(RenderInstanceTemporaryPool) };

static char AppInstanceTempMemory[64 * KiB];
static RingAllocator AppInstanceTempAllocator{ AppInstanceTempMemory, sizeof(AppInstanceTempMemory) };


static char GlobalInputScratchMemory[128 * MiB];
static SlabAllocator GlobalInputScratchAllocator{ GlobalInputScratchMemory, sizeof(GlobalInputScratchMemory) };

static char ThreadSharedCmdMemory[4 * KiB];
static MessageQueue ThreadSharedMessageQueue{ ThreadSharedCmdMemory, sizeof(ThreadSharedCmdMemory) };

static char LoggerMessageMemory[64 * KiB];
static Logger mainAppLogger{ LoggerMessageMemory, 16 * KiB };

static EntryHandle mainLinearSampler = EntryHandle();

static uint32_t imageIndex = 0;

static int graphNoMSAAIndex = -1;
static int graphMSAAIndex = -1;
static int MSAAPost = -1;
static int BasicShadow = -1;
static int MSAAShadowMapping = -1;
static int frameGraphsCount = 0;

static ShadowMapBase mainShadowMapManager{};

static ShadowMapDebugPipelineData smdpd{};

static int mainShadowWidth = 4096;
static int mainShadowHeight = 4096;

static bool stopThreadServer = false;

static void ScanSTDIN(void*);
static int AddLight(GPULightSource& lightDesc, LightType type);
static void UpdateLight(GPULightSource& lightDesc, int lightIndex);
static int CreateBlendRange(int* blendIDs, int blendCount);
static int CreateBlendDetails(BlendMaterialType type, float constantAlpha);
static int CreateBlendDetails(BlendMaterialType type, int mapID);
static EntryHandle ReadCubeImage(StringView* name, int textureCount, TextureIOType ioType);
static int Read2DImage(StringView* name, int mipCounts, TextureIOType ioType);
static void CreateUniformGrid();
static SMBImageFormat ConvertAppImageFormatToSMBFormat(ImageFormat format);
static ImageFormat ConvertSMBImageToAppImage(SMBImageFormat fmt);
static void LoadObjectThreaded(void* data);;

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

void ApplicationLoop::Execute()
{

	OSSyncMemoryRequirements syncMemReq = OSGetSyncMemoryRequirements(10);
	OSWindowMemoryRequirements winMemReq = OSGetWindowMemoryRequirements(1);
	OSFileMemoryRequirements fileMemReq = OSGetFileMemoryRequirements(30);
	OSThreadMemoryRequirements threadMemReq = OSGetThreadMemoryRequirements(5);

	OSSeedFileMemory(
		osAllocator.Allocate(fileMemReq.dataSize, fileMemReq.alignment),
		fileMemReq.dataSize,
		30
	);

	OSSeedSyncMemory(
		osAllocator.Allocate(syncMemReq.dataSize, syncMemReq.alignment),
		syncMemReq.dataSize,
		10
	);

	OSSeedThreadMemory(
		osAllocator.Allocate(threadMemReq.dataSize, threadMemReq.alignment),
		threadMemReq.dataSize,
		5
	);

	OSSeedWindowMemory(
		osAllocator.Allocate(winMemReq.dataSize, winMemReq.alignment),
		winMemReq.dataSize,
		1
	);

	StringView *mainInputString = AppInstanceTempAllocator.AllocateFromNullString(args.inputFile.string().c_str());

	OSGetSTDOutput(&mainAppLogger.fileHandle);

	if (args.justexport)
	{
		SMBFile mainSMB(*mainInputString, &GlobalInputScratchAllocator, &mainAppLogger);

		StringView fileName{};

		fileName.stringData = (char*)GlobalInputScratchAllocator.Allocate(150);

		FileManager::ExtractFileNameFromPath(mainInputString, &fileName);

		FileManager::SetFileCurrentDirectory(&fileName);

		ExportChunksFromFile(mainSMB, &GlobalInputScratchAllocator);

		cleaned = true;
	}
	else
	{
		InitializeRuntime();

		CreateCrateObject();

		CreateCornerWall(10.0f, 10.0f, 2.0f, 1.0f);

		LoadObject(*mainInputString);

		CreateUniformGrid();

		LARGE_INTEGER startTime;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER frequency;

		uint64_t frameCounter = 0;
		double FPS = 60.0f;

		char windowText[30];

		StringView view { windowText, 0 };

		auto fps = [&frameCounter, &currentTime, &startTime, &frequency, &FPS, &windowText, &view, this]()
			{
				double elapsed;
				QueryPerformanceCounter(&currentTime);

				elapsed = static_cast<double>((currentTime.QuadPart - startTime.QuadPart)) / frequency.QuadPart;

				if (elapsed >= 1.0) {
					FPS = static_cast<double>(frameCounter) / elapsed;
			
					view.charCount = snprintf(windowText, 30, "FPS : %.2f", FPS);

					mainWindow->SetWindowTitle(view);
				
					frameCounter = 0;
					QueryPerformanceCounter(&startTime);
					return 1;
				}

				return 0;
			};

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startTime);

		mainIndirectDrawData.commandBufferCount = globalRenderableCount;

		debugIndirectDrawData.commandBufferCount = globalDebugStructCount;

		uint32_t framesInFlight = GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT;

		int updateMainDrawCommand = framesInFlight, updateMainDebugCommand = framesInFlight, updateLights = framesInFlight;

		int mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;

		int mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;

		int previousGlobalLightCount = globalLightCount;

		GlobalRenderer::gRenderInstance->AddCommandQueue(mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance->AddCommandQueue(currentFrameGraphIndex, ATTACHMENT_COMMANDS);

		GlobalRenderer::gRenderInstance->EndFrame();

		DeviceHandleArrayUpdate samplerUpdate;

		samplerUpdate.resourceCount = 1;
		samplerUpdate.resourceDstBegin = 0;
		samplerUpdate.resourceHandles = &mainLinearSampler;

		GlobalRenderer::gRenderInstance->UpdateShaderResourceArray(globalTexturesDescriptor, 0, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

		while (running)
		{
			mainWindow->PollEvents();

			if (mainWindow->ShouldCloseWindow()) break;

			ProcessKeys(mainWindow->windowData.info.actions);

			bool cameraMove = MoveCamera(FPS);

			if (previousGlobalLightCount != globalLightCount || updateLights > 0)
			{
				if (!updateLights) updateLights = framesInFlight;

				previousGlobalLightCount = globalLightCount;

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.preWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.prefixSumPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.postWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, mainShadowMapManager.shadowClippingPipeline);

				updateLights--;
			}

			if (cameraMove || mainDrawRenderableCount != mainIndirectDrawData.commandBufferCount || updateMainDrawCommand > 0)
			{
				if (!updateMainDrawCommand || cameraMove) {
					updateMainDrawCommand = framesInFlight;
					GlobalRenderer::gRenderInstance->InsertTransferCommand(mainIndirectDrawData.commandBufferCountAlloc, 8, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
				}

				updateMainDrawCommand--;

				mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.preWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.prefixSumPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.postWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, mainIndirectDrawData.indirectCullPipeline);
			}

			if (cameraMove || debugIndirectDrawData.commandBufferCount != mainDebugDrawRenderableCount || updateMainDebugCommand > 0)
			{
				if (!updateMainDebugCommand || cameraMove)
				{
					GlobalRenderer::gRenderInstance->InsertTransferCommand(debugIndirectDrawData.commandBufferCountAlloc, 8, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
					updateMainDebugCommand = framesInFlight;
				}
				mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;
				GlobalRenderer::gRenderInstance->AddPipelineToComputeQueue(mainComputeQueueIndex, debugIndirectDrawData.indirectCullPipeline);
				updateMainDebugCommand--;
			}

			if (mainWindow->windowData.info.HandleResizeRequested())
			{
				GlobalRenderer::gRenderInstance->RecreateSwapChain(mainPresentationSwapChain);
				c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance->GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance->GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));
				UpdateCameraMatrix();
				RecreateFrameGraphAttachments(GlobalRenderer::gRenderInstance->GetSwapChainWidth(mainPresentationSwapChain), GlobalRenderer::gRenderInstance->GetSwapChainHeight(mainPresentationSwapChain));
			}

			imageIndex = GlobalRenderer::gRenderInstance->BeginFrame(mainPresentationSwapChain);

			if (imageIndex != ~0ui32) 
			{
				if (currentFrameGraphIndex == MSAAShadowMapping)
				{
					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(smdpd.shadowMapPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 1);
				}
				else if (currentFrameGraphIndex == BasicShadow)
				{
					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(smdpd.fullScreenPipeline, currentFrameGraphIndex, 0);
				}
				else 
				{
					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 0);

					if (currentFrameGraphIndex == MSAAPost)
						GlobalRenderer::gRenderInstance->AddPipelineToRPGraphicsQueue(mainFullScreenPipeline, currentFrameGraphIndex, 1);
				}

				GlobalRenderer::gRenderInstance->DrawScene(imageIndex);

				GlobalRenderer::gRenderInstance->SubmitFrame(mainPresentationSwapChain, imageIndex);
			}
			
			GlobalRenderer::gRenderInstance->EndFrame();	

			ProcessCommands();

			ThreadManager::ASyncThreadsDone();

			fps();

			frameCounter++;

			mainIndirectDrawData.commandBufferCount = globalRenderableCount;

			debugIndirectDrawData.commandBufferCount = globalDebugStructCount;

			mainAppLogger.ProcessMessage();
			
		}
	}
}

void ApplicationLoop::CreateTexturePools()
{
	std::array<ImageFormat, 4> formats = {
		ImageFormat::DXT1,
		ImageFormat::DXT3,
		ImageFormat::B8G8R8A8,
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
	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&c.View, globalBufferLocation, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 0, TransferType::MEMORY);
}

EntryHandle ApplicationLoop::GetPoolIndexByFormat(ImageFormat format)
{
	EntryHandle ret = EntryHandle();
	switch (format)
	{
	case ImageFormat::DXT1:
		ret = mainDictionary.texturePoolHandle[0];
		break;
	case ImageFormat::DXT3:
		ret = mainDictionary.texturePoolHandle[1];
		break;
	case ImageFormat::B8G8R8A8:
		ret = mainDictionary.texturePoolHandle[2];
		break;
	case ImageFormat::B8G8R8A8_UNORM:
		ret = mainDictionary.texturePoolHandle[3];
		break;
	}
	return ret;
}

void ApplicationLoop::CreateCornerWall(float width, float height, float xDiv, float yDiv)
{
	VertexInputDescription inputDescription[2];

	inputDescription[0].byteoffset = 0;
	inputDescription[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	inputDescription[0].vertexusage = VertexUsage::POSITION;

	inputDescription[1].byteoffset = sizeof(Vector4f);
	inputDescription[1].format = ComponentFormatType::R32G32B32_SFLOAT;
	inputDescription[1].vertexusage = VertexUsage::NORMAL;

	AxisBox box;

	xDiv = max(xDiv, 1.0f);
	yDiv = max(yDiv, 1.0f);

	int vertexCount = (int)(xDiv+1.0f) * (int)( yDiv+1.0f);

	float xStart = -width / 2.0f;
	float xEnd = width / 2.0f;
	float zStart = -height / 2.0f;
	float zEnd = height / 2.0f;

	struct GridVertex
	{
		Vector4f pos;
		Vector3f normal;
	};

	int vertexTotalSize = vertexCount * sizeof(GridVertex);

	GridVertex* poses = (GridVertex*)AppInstanceTempAllocator.CAllocate(vertexTotalSize, 16);

	box.min = Vector4f(xStart, 0.0f, zStart, 1.0);
	box.max = Vector4f(xEnd, 1.0f, zEnd, 1.0);

	Vector3f xstep = Vector3f(width / xDiv, 0.0, 0.0);
	Vector3f ystep = Vector3f(0.0, 0.0, height / yDiv);

	Vector3f base = Vector3f(xStart, 0.0f, zStart);

	int index = 0;

	int totalRow = (int)(yDiv + 1.0f);
	int totalColumn = (int)(xDiv + 1.0f);

	for (float i = 0; i < (float)totalRow; i++)
	{
		for (float j = 0; j < (float)totalColumn; j++)
		{
			Vector3f where = base + (xstep * j);
			poses[index].pos = Vector4f(where.x, where.y, where.z, 1.0);
			poses[index++].normal = Vector3f(0.0, 1.0, 0.0);
		}

		base =  base + (ystep);
	}

	int16_t indexCount = (totalRow * totalColumn) + ((totalColumn - 2) * 2);

	int16_t* indices = (int16_t*)AppInstanceTempAllocator.CAllocate(indexCount * 2, 2);

	int indicesAlloc = 0;

	for (int j = 0; j < (int)xDiv; j++)
	{
		int16_t topleft = (j);
		int16_t topright = (j + 1);
		int16_t bottomleft = j + totalColumn;
		int16_t bottomright = (j + 1) + totalColumn;

		indices[indicesAlloc++] = topleft;
		indices[indicesAlloc++] = topright;
		indices[indicesAlloc++] = bottomleft;
		indices[indicesAlloc++] = bottomright;
	}

	for (int i = 1; i < (int)yDiv; i++)
	{
		int16_t stitch1 = (totalColumn - 1) + (totalColumn * i);
		int16_t stitch2 = (totalColumn * i);

		indices[indicesAlloc++] = stitch1;
		indices[indicesAlloc++] = stitch2;

		for (int j = 0; j < (int)xDiv; j++)
		{

			int16_t topleft = (j) + (totalColumn * i);
			int16_t topright = (j + 1) + (totalColumn * i);
			int16_t bottomleft = j + (totalColumn * i+1);
			int16_t bottomright = (j + 1) + (totalColumn*i+1);

			indices[indicesAlloc++] = topleft;
			indices[indicesAlloc++] = topright;
			indices[indicesAlloc++] = bottomleft;
			indices[indicesAlloc++] = bottomright;
		}
	}

	int compressedSize = GetCompressedSize(inputDescription, 2), vertexFlags = 0;

	void* compPoses = (void*)AppInstanceTempAllocator.CAllocate(vertexCount * compressedSize, 16);

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * vertexCount, 16);
	int indexAlloc = indexBufferAlloc.Allocate(indexCount * 2, 64);

	int totalDataSize = CompressMeshFromVertexStream(inputDescription, 2, sizeof(GridVertex), vertexCount, box, poses, compPoses, &compressedSize, &vertexFlags);

	auto rendInst = GlobalRenderer::gRenderInstance;

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, width);

	int geomIndex = CreateGPUGeometryDetails(box);

	int meshIndex = CreateMeshHandle(compPoses, indices, vertexFlags, vertexCount, compressedSize, 2, indexCount, sphere, vertexAlloc, indexAlloc);

	rendInst->UpdateDriverMemory(compPoses, globalVertexBuffer, vertexCount * compressedSize, vertexAlloc, TransferType::CACHED);
	rendInst->UpdateDriverMemory(indices, globalIndexBuffer, indexCount * 2, indexAlloc, TransferType::CACHED);

	RenderableGeomCPUData* geomRendCPUData;

	std::tie(std::ignore, geomRendCPUData) = renderablesGeomObjects.Allocate();

	Matrix4f sideFrontPanel = CreateRotationMatrixMat4(Vector3f(0.0f, 0.0f, 1.0), DegToRad(-90.0f));

	sideFrontPanel.translate = Vector4f(10.0f, 3.0, -5.0, 1.0);

	Matrix4f backPanel = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0), DegToRad(-90.0f));

	backPanel.translate = Vector4f(5.0f, 3.0, -10.0, 1.0);

	int gridMaterial = { CreateMaterial(VERTEXNORMAL, nullptr, 0, Vector4f(1.0, 0.0, 0.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0)) };

	int gridMaterialRange = AddMaterialToDeviceMemory(1, &gridMaterial);

	geomRendCPUData->renderableMeshStart = globalRenderableCount;

	geomRendCPUData->renderableMeshCount = 3;

	int gpuGeomRenderable = CreateGPUGeometryRenderable(Identity4f(), geomIndex, geomRendCPUData->renderableMeshStart, geomRendCPUData->renderableMeshCount);

	CreateRenderable({ { 1.0, 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0, 0.0 }, { 0.0 , 0.0, 1.0, 0.0 }, { 5.0f, -2.0, -5.0, 1.0 } }, gpuGeomRenderable, gridMaterialRange, 1, -1, meshIndex, 1);

	CreateRenderable(sideFrontPanel, gpuGeomRenderable, gridMaterialRange, 1, -1, meshIndex, 1);
	
	CreateRenderable(backPanel, gpuGeomRenderable, gridMaterialRange, 1, -1, meshIndex, 1);

	mainAppLogger.AddLogMessage(LOGINFO, "Created Corner Object", 21);

	return;
}


void ApplicationLoop::CreateCrateObject()
{
	VertexInputDescription inputDescription[7];

	inputDescription[0].byteoffset = 0;
	inputDescription[0].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	inputDescription[0].vertexusage = VertexUsage::POSITION;

	inputDescription[1].byteoffset = 16;
	inputDescription[1].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	inputDescription[1].vertexusage = VertexUsage::COLOR0;

	inputDescription[2].byteoffset = 32;
	inputDescription[2].format = ComponentFormatType::R32G32B32_SFLOAT;
	inputDescription[2].vertexusage = VertexUsage::NORMAL;

	inputDescription[3].byteoffset = 44;
	inputDescription[3].format = ComponentFormatType::R32G32_SFLOAT;
	inputDescription[3].vertexusage = VertexUsage::TEX0;

	inputDescription[4].byteoffset = 52;
	inputDescription[4].format = ComponentFormatType::R32G32_SFLOAT;
	inputDescription[4].vertexusage = VertexUsage::TEX1;	

	inputDescription[5].byteoffset = 60;
	inputDescription[5].format = ComponentFormatType::R32G32_SFLOAT;
	inputDescription[5].vertexusage = VertexUsage::TEX2;

	inputDescription[6].byteoffset = 68;
	inputDescription[6].format = ComponentFormatType::R32G32B32A32_SFLOAT;
	inputDescription[6].vertexusage = VertexUsage::TANGENTS;





	uint16_t BoxIndices[52] = {
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

	Vector4f BoxVerts[24] =
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


	Vector4f BoxColors[24] =
	{
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),

	
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),

		
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),

		
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),

		
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),

		
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1),
		Vector4f(1,1,1,1)
	};


	Vector2f texturesCoordinate[24] = {
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
		Vector2f(1.0, 0.0),
		Vector2f(0.0, 0.0),
		Vector2f(1.0, 1.0),
		Vector2f(0.0, 1.0),
	};

	Vector3f totalNormals[24];
	Vector4f tangents[24];

	CreateBitTangentFromNormal(BoxVerts, texturesCoordinate, BoxIndices, 52, 24, tangents, totalNormals, &AppInstanceTempAllocator);

	struct MyVertex
	{
		Vector4f pos;
		Vector4f color;
		Vector3f normal;
		Vector2f texCoords;
		Vector2f texCoords2;
		Vector2f texCoords3;
		Vector4f tangent;
	};

	MyVertex compVerts[24];

	AxisBox BOX =
	{
		.min = Vector4f(-1.0, -1.0, -1.0, 0.0),
		.max = Vector4f(1.0, 1.0, 1.0, 0.0),
	};

	for (int i = 0; i < 24; i++)
	{
		compVerts[i].pos = BoxVerts[i];
		compVerts[i].color = BoxColors[i];
		compVerts[i].normal = totalNormals[i];
		compVerts[i].texCoords = texturesCoordinate[i];
		compVerts[i].texCoords2 = texturesCoordinate[i];
		compVerts[i].texCoords3 = texturesCoordinate[i];
		compVerts[i].tangent = tangents[i];
	}

	int compressedSize = GetCompressedSize(inputDescription, 7), vertexFlags = 0;

	void* compressedVertexData = AppInstanceTempAllocator.Allocate(compressedSize * 24, 16);

	int totalDataSize = CompressMeshFromVertexStream(inputDescription, 7, sizeof(MyVertex), 24, BOX, compVerts, compressedVertexData, &compressedSize, &vertexFlags);

	auto rendInst = GlobalRenderer::gRenderInstance;

	Matrix4f crateMatrix = Identity4f();

	crateMatrix.translate = Vector4f(-10.0, 5.0, 0.0f, 1.0f);

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, 1.5f);

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * 24, 16);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	StringView blendname = AppInstanceTempAllocator.AllocateFromNullStringCopy("blendmap.bmp");

	StringView skyname = AppInstanceTempAllocator.AllocateFromNullStringCopy("sky.bmp");

	StringView normalmapname = AppInstanceTempAllocator.AllocateFromNullStringCopy("WNN2.bmp");

	StringView albedomapname = AppInstanceTempAllocator.AllocateFromNullStringCopy("WNN.bmp");

	int alebdoMapped = Read2DImage(&albedomapname, 1, BMP);

	int normalMapped = Read2DImage(&normalmapname, 1, BMP);

	int blendMapped = Read2DImage(&blendname, 1, BMP);
	int skymapped = Read2DImage(&skyname, 1, BMP);

	int blendStart = CreateBlendDetails(BlendMaterialType::ConstantAlpha, 1.0f);
	int blendStart2 = CreateBlendDetails(BlendMaterialType::BlendMap, blendMapped);

	std::array blends = { blendStart, blendStart2 };

	int blendRange = CreateBlendRange(blends.data(), 2);

	std::array arr = { alebdoMapped, normalMapped, skymapped };

	std::array<int, 2> materialIDs = { CreateMaterial(ALBEDOMAPPED | TANGENTNORMALMAPPED, arr.data(), 2, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0)),
		CreateMaterial(ALBEDOMAPPED, arr.data()+2, 1, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.80, .80, .80, 1.0), 256.0, Vector4f(.04, .06, 0.08, 1.0))};

	int materialRangeCount = 2;
	int materialRangeStart = AddMaterialToDeviceMemory(materialRangeCount, materialIDs.data());

	int geomIndex = CreateGPUGeometryDetails(BOX);
	
	int meshIndex = CreateMeshHandle(compVerts, BoxIndices, vertexFlags, 24, compressedSize, 2, 52, sphere, vertexAlloc, indexAlloc);

	RenderableGeomCPUData* rendGeomCPUData;

	std::tie(std::ignore, rendGeomCPUData) = renderablesGeomObjects.Allocate();

	rendGeomCPUData->renderableMeshStart = globalRenderableCount;

	rendGeomCPUData->renderableMeshCount = 1;

	int gpuGeomRenderable = CreateGPUGeometryRenderable(Identity4f(), geomIndex, rendGeomCPUData->renderableMeshStart, rendGeomCPUData->renderableMeshCount);

	CreateRenderable(crateMatrix, gpuGeomRenderable, materialRangeStart, materialRangeCount, blendRange, meshIndex, 1);
	
	rendInst->UpdateDriverMemory(compressedVertexData, globalVertexBuffer, compressedSize * 24,  vertexAlloc, TransferType::CACHED);
	rendInst->UpdateDriverMemory(BoxIndices, globalIndexBuffer,  sizeof(BoxIndices),  indexAlloc, TransferType::CACHED);

	mainAppLogger.AddLogMessage(LOGINFO, "Created Crate Object", 20);
}

void ApplicationLoop::SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file)
{
	auto rendInst = GlobalRenderer::gRenderInstance;

	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;

	int meshCount = geoDef->numRenderables;

	GeometryCPUData* geom = nullptr;

	RenderableGeomCPUData* geomRenderableCPUData = nullptr;

	std::tie(std::ignore, geomRenderableCPUData) = renderablesGeomObjects.Allocate();

	std::tie(geomRenderableCPUData->geomIndex, geom) = geometryCPUData.Allocate();
	
	geom->meshStart = meshCPUData.AllocateN(meshCount);

	geom->meshCount = meshCount;

	Matrix4f *geomSpecificData = (Matrix4f*)geometryObjectSpecificAlloc.Allocate(sizeof(Matrix4f));

	geomRenderableCPUData->geomInstanceLocalMemoryCount = geometryObjectData.Allocate(geomSpecificData);

	*geomSpecificData = Identity4f();
		
	(*geomSpecificData).translate = Vector4f(0.f, 0.f, 0.f, 1.0f);

	Matrix4f rotation = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0f), DegToRad(90.0f));

	*geomSpecificData = *geomSpecificData * rotation;

	int geomBlend = CreateBlendDetails(BlendMaterialType::ConstantAlpha, 1.0f);

	int blendRange = CreateBlendRange(&geomBlend, 1);

	int geomDescIndex = CreateGPUGeometryDetails(geoDef->axialBox);

	geomRenderableCPUData->renderableMeshStart = globalRenderableCount;
	
	geomRenderableCPUData->renderableMeshCount = meshCount;

	int gpuGeomIndex = CreateGPUGeometryRenderable(*geomSpecificData, geomDescIndex, geomRenderableCPUData->renderableMeshStart, geomRenderableCPUData->renderableMeshCount);

	for (int i = 0; i < meshCount; i++)
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

		SMBCopyVertexData(geoDef, i, file, vertexData, decompressed, &GlobalInputScratchAllocator);

		int vertexFlags = POSITION | TEXTURE0;

		switch (type)
		{
		case PosPack6_CNorm_C16Tex1_Bone2:
		{
			vertexFlags = POSITION | TEXTURE0 | NORMAL | BONES2;
			break;
		}
		case PosPack6_C16Tex2_Bone2:
		{
			vertexFlags = POSITION | TEXTURE0 | TEXTURE1 | BONES2;
			break;
		}
		case PosPack6_C16Tex1_Bone2:
		{
			vertexFlags = POSITION | TEXTURE0 | BONES2;
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
		rendInst->UpdateDriverMemory(indices, globalIndexBuffer, sizeof(uint16_t) * indexCount,  indexAlloc, TransferType::MEMORY);

		rendInst->UpdateDriverMemory(vertexData, globalVertexBuffer, vertexSize * vertexCount,  vertexAlloc, TransferType::MEMORY);
		

		int base = geoDef->materialStart[i];

		int textureStart = meshTextureHandles.AllocateN(geoDef->materialsCount[i]);
		int textureCount = geoDef->materialsCount[i];

		for (int j = 0; j < textureCount; j++)
		{
			meshTextureHandles.Update(textureStart + i, geoDef->materialsId[base + i]);
		}

		std::array<int, 1> materialHandles = { CreateMaterial(ALBEDOMAPPED | ((textureCount > 1) ? WORLDNORMALMAPPED : type == PosPack6_C16Tex1_Bone2 ? 0 : VERTEXNORMAL), &geoDef->materialsId[base], textureCount, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.1, .1, .1, 1.0), 1.5, Vector4f(0.0, 0.0, 0.0, 1.0))};

		int materialRangeStart = AddMaterialToDeviceMemory(1, materialHandles.data());
		int materialRangeCount = 1;

		CreateMeshHandle(geom->meshStart + i, vertexData, indices,
			vertexFlags, vertexCount, vertexSize, 2, 
			indexCount,
			geoDef->spheres[i], 
			vertexAlloc, indexAlloc
		);

		CreateRenderable(Identity4f(), gpuGeomIndex, materialRangeStart, materialRangeCount, blendRange, geom->meshStart + i, 1);
	}

	CreateSphereDebugStruct(geoDef->axialBox, 24, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(0.0, 1.0, 0.0, 0.0));

	CreateAABBDebugStruct(geoDef->axialBox, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.5, 0.5, 1.0, 0.0));

	mainAppLogger.AddLogMessage(LOGINFO, "Created SMB Object", 18);
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
	GPUDebugRenderable drawStruct;
	DebugDrawType type = DebugDrawType::DEBUGSPHERE;

	if (globalDebugStructCount == globalDebugStructMaxCount)
	{
		return -1;
	}

	int debugStructLocation = globalDebugStructCount++;

	drawStruct.center = Vector4f(center.x, center.y, center.z, 1.0f);

	drawStruct.scale = scale;
	drawStruct.color = color;

	drawStruct.halfExtents = Vector4f(r, std::bit_cast<float, uint32_t>(count), 1.0, 1.0);


	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&type,  globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

	return debugStructLocation;
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
	GPUDebugRenderable drawStruct;

	if (globalDebugStructCount == globalDebugStructMaxCount)
	{
		return -1;
	}

	int debugStructLocation = globalDebugStructCount++;
	
	drawStruct.center = Vector4f(center.x , center.y, center.z, 1.0f);
	drawStruct.scale = scale;
	drawStruct.color = color;
	drawStruct.halfExtents = halfExtents;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&type, globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

	return debugStructLocation;
}

void ApplicationLoop::LoadSMBFile(SMBFile &file)
{

	const int MAX_GEO_FILES = 10;
	const int MAX_TEXTURES = 10;

	int totalTextureCount = 0, totalMeshCount = 0;
	auto& chunk = file.chunks;

	SMBTexture* textures = (SMBTexture*)AppInstanceTempAllocator.CAllocate(sizeof(SMBTexture) * MAX_TEXTURES);

	SMBGeoChunk* geoDefs = (SMBGeoChunk*)AppInstanceTempAllocator.Allocate(sizeof(SMBGeoChunk) * MAX_GEO_FILES);

	for (size_t i = 0; i<file.numResources; i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{
			
			SMBGeoChunk* geoDef = &geoDefs[totalMeshCount++];

			size_t seekpos = chunk[i].offsetInHeader;

			OSSeekFile(&file.fileHandle, seekpos, BEGIN);

			char* geomHeader = (char*)AppInstanceTempAllocator.Allocate(chunk[i].headerSize);

			OSReadFile(&file.fileHandle, chunk[i].headerSize, geomHeader);

		    ProcessGeometryClass(geomHeader, totalTextureCount, geoDef, chunk[i].contigOffset + file.fileOffset, chunk[i].fileOffset + file.numContiguousBytes + file.fileOffset);
	
			break;
		}
		case TEXTURE:
		{
			std::construct_at(&textures[totalTextureCount], file, chunk[i]);	
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

	TextureDetails** details = (TextureDetails**)AppInstanceTempAllocator.Allocate(sizeof(TextureDetails*) * totalTextureCount);

	int index = mainDictionary.AllocateNTextureHandles(totalTextureCount, details);

	for (int ii = 0; ii < totalTextureCount; ii++)
	{
		SMBTexture& texture = textures[ii];

		ImageFormat format = ConvertSMBImageToAppImage(texture.type);

		texture.data = (char*)mainDictionary.AllocateImageCache(texture.cumulativeSize);

		uint32_t* sizesCached = (uint32_t*)mainDictionary.AllocateImageCache(sizeof(uint32_t) * texture.miplevels);

		memcpy(sizesCached, texture.imageSizes, sizeof(uint32_t) * texture.miplevels);

		texture.ReadTextureData(file);

		mainDictionary.textureHandles[ii + index] =
			GlobalRenderer::gRenderInstance->CreateImageHandle(
				texture.cumulativeSize,
				texture.width,
				texture.height,
				texture.miplevels,
				format,
				GetPoolIndexByFormat(format),
				mainLinearSampler
			);

		GlobalRenderer::gRenderInstance->UpdateImageMemory(
			texture.data, 
			mainDictionary.textureHandles[ii + index], 
			sizesCached, 
			texture.cumulativeSize, 
			texture.width, 
			texture.height, 
			texture.miplevels,
			1,
			format
		);
	}

	DeviceHandleArrayUpdate update; 

	update.resourceCount = totalTextureCount;
	update.resourceDstBegin = index;
	update.resourceHandles = mainDictionary.textureHandles.data() + index;

	GlobalRenderer::gRenderInstance->UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);

	
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


	debugIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndirectCommand),  debugIndirectDrawData.commandBufferSize, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	debugIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	debugIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);


	debugIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(DEBUGCULL, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferAlloc, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectCullDescriptor, &globalDebugTypesAlloc, 0, 1, 2);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, &globalBufferLocation, nullptr,  0, 1, 3);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, &globalDebugStructAlloc, nullptr, 0, 1, 4);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCountAlloc, nullptr, 0, 1, 5);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCount, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(debugIndirectDrawData.indirectCullDescriptor, 5, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);


	std::array debugCullDescriptors = { debugIndirectDrawData.indirectCullDescriptor };

	ShaderComputeLayout* debugCullDescriptorLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(DEBUGCULL);

	ComputeIntermediaryPipelineInfo debugCullPipelineCreate = {
			.x = 4096 / debugCullDescriptorLayout->x,
			.y = 1,
			.z = 1,
			.pipelinename = DEBUGCULL,
			.descCount = 1,
			.descriptorsetid = debugCullDescriptors.data()
	};


	debugIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&debugCullPipelineCreate);

	debugIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(DEBUGDRAW, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(debugIndirectDrawData.indirectDrawDescriptor, &globalDebugStructAlloc, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(debugIndirectDrawData.indirectDrawDescriptor, &globalDebugTypesAlloc, 0, 1, 2);

	std::array<int, 2> indirectDebugDrawDescriptors = {
		globalBufferDescriptor,
		debugIndirectDrawData.indirectDrawDescriptor,
	};

	GraphicsIntermediaryPipelineInfo indirectDebugDrawPipelineCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = DEBUGDRAW,
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

	mainIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), count, alignof(VkDrawIndexedIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	mainIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	mainIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(RENDEROBJCULL, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	mainIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &globalBufferLocation, nullptr, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCountAlloc, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &globalRenderableLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectCullDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 7);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 5, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainIndirectDrawData.indirectCullDescriptor, 4, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);



	mainIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(GENERIC, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalMeshLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalVertexBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalLightBuffer, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, &globalLightTypesBuffer,  0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalMaterialsLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalRenderableLocation, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, &globalMaterialIndicesLocation, 0, 1, 7);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalBlendDetailsLocation, nullptr, 0, 1, 8);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainIndirectDrawData.indirectDrawDescriptor, &globalBlendRangesLocation, 0, 1, 9);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 10);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainIndirectDrawData.indirectDrawDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 11);

	std::array<int, 1> indirectDrawDescriptors = {
		mainIndirectDrawData.indirectDrawDescriptor,
	};

	shadowMapIndex = mainDictionary.AllocateNTextureHandles(GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT, nullptr);

	GlobalRenderer::gRenderInstance->UploadFrameAttachmentResource(MSAAShadowMapping, 1, globalTexturesDescriptor, 3, shadowMapIndex);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectDrawDescriptor, &shadowMapIndex, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainIndirectDrawData.indirectDrawDescriptor, &GlobalRenderer::gRenderInstance->currentFrame, 1);

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



	int cullLightDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(RENDEROBJCULL, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, &lightAssignment.deviceOffsetsAlloc, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, &lightAssignment.deviceCountsAlloc, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(cullLightDescriptor, &globalLightBuffer, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(cullLightDescriptor, &globalLightTypesBuffer, 0, 1, 4 );


	ShaderComputeLayout* layout = GlobalRenderer::gRenderInstance->GetComputeLayout(RENDEROBJCULL);

	std::array computeDescriptors = { mainIndirectDrawData.indirectCullDescriptor, cullLightDescriptor };

	ComputeIntermediaryPipelineInfo mainCullComputeSetup = {
			.x = 4096 / layout->x,
			.y = 1,
			.z = 1,
			.pipelinename = RENDEROBJCULL,
			.descCount = 2,
			.descriptorsetid = computeDescriptors.data()
	};

	mainIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&mainCullComputeSetup);

	/*

	normalDebugAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(sizeof(VkDrawIndirectCommand) * 10, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, 0);

	int normalDrawDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(NORMALDEBUGDRAW, 2, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

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
	.pipelinename = NORMALDEBUGDRAW,
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


	int outlineDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(OUTLINE, 2, 3);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, &globalMeshLocation, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, &globalVertexBuffer, nullptr, 0, 1, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(outlineDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(outlineDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 5);

	static Vector4f black = { 0.0, 0.0, 0.0, 1.0 };

	static float outlineLength = 1.025;

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(outlineDescriptor, &black, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(outlineDescriptor, &outlineLength, 1);

	std::array<int, 2> indirectOutline = { outlineDescriptor };

	GraphicsIntermediaryPipelineInfo outlineDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = OUTLINE,
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

	//int outlinePipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&outlineDrawCreate, true);


}

void CreateMeshWorldAssignment(int count)
{
	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(PREFIXSUM);

	worldSpaceAssignment.totalElementsCount = count;
	worldSpaceAssignment.totalSumsNeeded = (int)floor(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	worldSpaceAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	worldSpaceAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	worldSpaceAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	if (worldSpaceAssignment.totalSumsNeeded)
	{
		worldSpaceAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
		
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else 
	{
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, nullptr, nullptr, 0, 0, 2);
	}


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 1);
	

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

	std::array prefixSumDescriptor = { worldSpaceAssignment.prefixSumDescriptors };

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = PREFIXSUM,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};



	worldSpaceAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);
	
	if (worldSpaceAssignment.totalSumsNeeded)
	{

		worldSpaceAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { worldSpaceAssignment.sumAfterDescriptors, };

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXSUM,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		worldSpaceAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);

		worldSpaceAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXADD, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



		std::array incrementSumsDescriptor = { worldSpaceAssignment.sumAppliedToBinDescriptors, };

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXADD,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};

		worldSpaceAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(WORLDORGANIZE);

	uint32_t assignmentGroupCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)assignmentLayout->x);

	worldSpaceAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	worldSpaceAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(WORLDORGANIZE, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &worldSpaceAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalRenderableLocation, nullptr, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { worldSpaceAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = WORLDORGANIZE,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	worldSpaceAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);


	worldSpaceAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(WORLDASSIGN, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &worldSpaceAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { worldSpaceAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = WORLDASSIGN,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	worldSpaceAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);
}

void CreateLightAssignments(int count)
{
	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(PREFIXSUM);

	lightAssignment.totalElementsCount = count;
	lightAssignment.totalSumsNeeded = (int)floor(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	lightAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	lightAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	lightAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, &lightAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.prefixSumDescriptors, &lightAssignment.totalElementsCount, 0);

	if (lightAssignment.totalSumsNeeded)
	{
		lightAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else
	{
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.prefixSumDescriptors, nullptr, nullptr, 0, 0, 2);
	}

	std::array prefixSumDescriptor = { lightAssignment.prefixSumDescriptors };

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = PREFIXSUM,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};

	lightAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);

	if (lightAssignment.totalSumsNeeded)
	{

		lightAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAfterDescriptors, &lightAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { lightAssignment.sumAfterDescriptors, };

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXSUM,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		lightAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);



		lightAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(PREFIXADD, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);



		std::array incrementSumsDescriptor = { lightAssignment.sumAppliedToBinDescriptors, };

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXADD,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};


		lightAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance->GetComputeLayout(LIGHTASSIGN);

	uint32_t assignmentGroupCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)assignmentLayout->x);


	lightAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	lightAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(LIGHTORGANIZE, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, &lightAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightBuffer, nullptr, 0, 1, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightTypesBuffer, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightCount, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { lightAssignment.preWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTORGANIZE,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	lightAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);


	lightAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(LIGHTASSIGN, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightTypesBuffer, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(lightAssignment.postWorldSpaceDivisionDescriptor, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(lightAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { lightAssignment.postWorldSpaceDivisionDescriptor, };

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTASSIGN,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	lightAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);
}

void CreateShadowMapManager(int maxShadowMapAssignment, int maxObjCount, int shadowMapHeight, int shadowMapWidth, int shadowMapAtlasHeight, int shadowMapAtlasWidth)
{
	mainShadowMapManager.atlasHeight = shadowMapAtlasHeight;
	mainShadowMapManager.atlasWidth = shadowMapAtlasWidth;
	mainShadowMapManager.avgShadowHeight = shadowMapHeight;
	mainShadowMapManager.avgShadowWidth = shadowMapWidth;
	mainShadowMapManager.frameGraphIndex = MSAAShadowMapping;
	mainShadowMapManager.resourceIndex = 1;
	mainShadowMapManager.totalShadowMaps = (int)((float)(shadowMapAtlasHeight * shadowMapAtlasWidth) / (float)(shadowMapWidth * shadowMapHeight));
	mainShadowMapManager.zoneAlloc = 0;

	

	mainShadowMapManager.shadowMapCountsAllocSize =  maxObjCount;
	mainShadowMapManager.shadowMapCountsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapCountsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapOffsetsAllocSize =  maxObjCount;
	mainShadowMapManager.shadowMapOffsetsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapOffsetsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapAssignmentsAllocSize = maxShadowMapAssignment * maxObjCount;
	mainShadowMapManager.shadowMapAssignmentsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapAssignmentsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	mainShadowMapManager.shadowMapObjectIDsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapObjectIDsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapObjectIDsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapObjectCountAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapIndirectBufferAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapIndirectBufferAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), mainShadowMapManager.shadowMapIndirectBufferAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT); ;
	

	mainShadowMapManager.shadowMapViewProjAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapViewProjAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(Matrix4f) * 2, mainShadowMapManager.shadowMapViewProjAllocSize, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapAtlasViewsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapAtlasViewsAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(ShadowMapView), mainShadowMapManager.shadowMapAtlasViewsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT);


	BufferArrayUpdate shadowViewProjUp{};
	BufferArrayUpdate shadowAtlasViews{};

	shadowViewProjUp.allocationCount = 1;
	shadowViewProjUp.resourceDstBegin = 0;
	shadowViewProjUp.allocationIndices = &mainShadowMapManager.shadowMapViewProjAlloc;
	
	shadowAtlasViews.allocationCount = 1;
	shadowAtlasViews.resourceDstBegin = 0;
	shadowAtlasViews.allocationIndices = &mainShadowMapManager.shadowMapAtlasViewsAlloc;


	GlobalRenderer::gRenderInstance->UpdateBufferResourceArray(globalTexturesDescriptor, 1, ShaderResourceType::STORAGE_BUFFER, &shadowViewProjUp);
	GlobalRenderer::gRenderInstance->UpdateBufferResourceArray(globalTexturesDescriptor, 2, ShaderResourceType::UNIFORM_BUFFER, &shadowAtlasViews);

	
	mainShadowMapManager.shadowClippingDescriptor1 = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(18, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	mainShadowMapManager.shadowClippingDescriptor2 = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(18, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapIndirectBufferAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapObjectCountAlloc, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &globalRenderableLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor1, &globalGeometryRenderableLocation, nullptr, 0, 1, 6);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(mainShadowMapManager.shadowClippingDescriptor2, &globalLightBuffer, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapCountsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(mainShadowMapManager.shadowClippingDescriptor2, &globalLightTypesBuffer, 0, 1, 4);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainShadowMapManager.shadowClippingDescriptor1, &mainIndirectDrawData.commandBufferCount, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainShadowMapManager.shadowClippingDescriptor1, &globalLightCount, 1);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor1, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor1, 2, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor1, 3, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor2, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor2, 2, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBarrier(mainShadowMapManager.shadowClippingDescriptor2, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	std::array shadowClipDesc = { mainShadowMapManager.shadowClippingDescriptor1, mainShadowMapManager.shadowClippingDescriptor2 };

	ComputeIntermediaryPipelineInfo shadowClipPipelineInfo = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 18,
			.descCount = 2,
			.descriptorsetid = shadowClipDesc.data()
	};
	
	mainShadowMapManager.shadowClippingPipeline = GlobalRenderer::gRenderInstance->CreateComputeVulkanPipelineObject(&shadowClipPipelineInfo);

	smdpd.shadowMapDescriptorSet = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(17, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &globalMeshLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &globalVertexBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 4);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferView(smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 5);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapViewProjAlloc, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapAtlasViewsAlloc, nullptr, 0, 1, 7);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 8);
	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(smdpd.shadowMapDescriptorSet, &globalGeometryRenderableLocation, nullptr, 0, 1, 9);

	

	
	GraphicsIntermediaryPipelineInfo indirectShadowDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = 17,
		.descCount = 1,
		.descriptorsetid = &smdpd.shadowMapDescriptorSet,
		.indexBufferHandle = globalIndexBuffer,
		.indexSize = 2,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = mainShadowMapManager.shadowMapIndirectBufferAlloc,
		.indirectDrawCount = mainShadowMapManager.shadowMapIndirectBufferAllocSize,
		.indirectCountAllocation = mainShadowMapManager.shadowMapObjectCountAlloc
	};


	smdpd.shadowMapPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&indirectShadowDrawCreate, false);

	

	
	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;


	smdpd.fullScreenDescriptorSet = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(16, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->UploadFrameAttachmentResource(MSAAShadowMapping, 1, smdpd.fullScreenDescriptorSet, 0, 0);
	GlobalRenderer::gRenderInstance->UpdateShaderResourceArray(smdpd.fullScreenDescriptorSet, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(smdpd.fullScreenDescriptorSet, &GlobalRenderer::gRenderInstance->currentFrame, 0);


	std::array<int, 1> fullScreenDesc = { smdpd.fullScreenDescriptorSet };

	GraphicsIntermediaryPipelineInfo fullscreenInfo = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 4,
		.pipelinename = 16,
		.descCount = 1,
		.descriptorsetid = fullScreenDesc.data(),
		.indexBufferHandle = ~0,
		.indexCount = 0,
		.instanceCount = 1,
		.indexSize = 0,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	smdpd.fullScreenPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&fullscreenInfo, false);
}

void ApplicationLoop::RecreateFrameGraphAttachments(uint32_t width, uint32_t height)
{
	if (BasicShadow >= 0)
	{
		GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, BasicShadow, 0, nullptr);
	}

	if (MSAAPost >= 0)
	{
		GlobalRenderer::gRenderInstance->CreatePerFrameAttachment(MSAAPost, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT, width, height, nullptr);
		GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, MSAAPost, 1, nullptr);
	}
	
	if (MSAAShadowMapping >= 0)
	{
		GlobalRenderer::gRenderInstance->CreatePerFrameAttachment(MSAAShadowMapping, 0, 3, 4096, 4096, nullptr);
		GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, MSAAShadowMapping, 1, nullptr);
		GlobalRenderer::gRenderInstance->UploadFrameAttachmentResource(MSAAShadowMapping, 1, globalTexturesDescriptor, 3, shadowMapIndex);
	}
}

void ApplicationLoop::CreateMSAAPostFullScreen()
{
	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;

	int mainFullScreen = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(16, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->UploadFrameAttachmentResource(MSAAPost, 1, mainFullScreen, 0, 0);
	GlobalRenderer::gRenderInstance->UpdateShaderResourceArray(mainFullScreen, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(mainFullScreen, &GlobalRenderer::gRenderInstance->currentFrame, 0);

	GraphicsIntermediaryPipelineInfo fullscreenInfo = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 4,
		.pipelinename = 16,
		.descCount = 1,
		.descriptorsetid = &mainFullScreen,
		.indexBufferHandle = ~0,
		.indexCount = 0,
		.instanceCount = 1,
		.indexSize = 0,
		.indexOffset = (uint32_t)0,

		.vertexOffset = (uint32_t)0,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	mainFullScreenPipeline = GlobalRenderer::gRenderInstance->CreateGraphicsVulkanPipelineObject(&fullscreenInfo, false);
}

void ApplicationLoop::CreateSkyBox()
{
	uint16_t BoxIndices[36] = {
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

	Vector4f BoxVerts[24] =
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

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(BoxVerts, globalVertexBuffer, sizeof(BoxVerts), vertexAlloc, TransferType::CACHED);
	GlobalRenderer::gRenderInstance->UpdateDriverMemory(BoxIndices, globalIndexBuffer, sizeof(BoxIndices), indexAlloc, TransferType::CACHED);

	StringView names[6] = {
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face4.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face1.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face5.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face2.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face6.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face3.bmp"),
	};

	skyboxCubeImage = ReadCubeImage(names, 6, BMP);

	static Matrix4f matrix = Identity4f();

	matrix.translate = Vector4f(-30.0, 0.0, 0.0, 1.0f);

	int camSkyboxData = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(SKYBOX, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	int skyboxDesc = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(SKYBOX, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(camSkyboxData, &globalBufferLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.BindSampledImageToShaderResource(skyboxDesc, &skyboxCubeImage, 1, 0, 0);
	GlobalRenderer::gRenderInstance->descriptorManager.UploadConstant(skyboxDesc, &matrix, 0);

	std::array<int, 2> skyboxDescs = { camSkyboxData, skyboxDesc };

	GraphicsIntermediaryPipelineInfo skyboxInfo = {
		.drawType = 0,
		.vertexBufferHandle = globalVertexBuffer,
		.vertexCount = 24,
		.pipelinename = SKYBOX,
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
}

void ApplicationLoop::InitializeRuntime()
{
	queueSema.Create();

	threadHandle = ThreadManager::LaunchOSBackgroundThread(ScanSTDIN, &stopThreadServer);

	mainDictionary.textureCache = (uintptr_t)mainTextureCacheMemory;
	
	mainDictionary.textureSize = sizeof(mainTextureCacheMemory);

	mainWindow = new WindowManager();

	mainWindow->CreateMainWindow();

	GlobalRenderer::gRenderInstance = new RenderInstance(&RenderInstanceMemoryAllocator, &RenderInstanceTemporaryAllocator);

	GlobalRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow, mainLayoutAttachments.size());

	mainDeviceBuffer = GlobalRenderer::gRenderInstance->CreateUniversalBuffer(64 * MiB, BufferType::DEVICE_MEMORY_TYPE);
	mainHostBuffer = GlobalRenderer::gRenderInstance->CreateUniversalBuffer(128 * MiB, BufferType::HOST_MEMORY_TYPE);

	ImageFormat requestedColorFormats = ImageFormat::B8G8R8A8;

	ImageFormat mainColorFormat = GlobalRenderer::gRenderInstance->FindSupportedBackBufferColorFormat(&requestedColorFormats, 1);

	ImageFormat requestedDSVFormats = ImageFormat::D24UNORMS8STENCIL;

	ImageFormat mainDepthFormat = GlobalRenderer::gRenderInstance->FindSupportedDepthFormat(&requestedDSVFormats, 1);

	int mainRTVIndex = GlobalRenderer::gRenderInstance->CreateRSVMemoryPool(512 * MiB, mainColorFormat, 4096, 4096);

	int mainDSVIndex = GlobalRenderer::gRenderInstance->CreateRSVMemoryPool(1024 * MiB, mainDepthFormat, 4096, 4096);

	MSAAPost = GlobalRenderer::gRenderInstance->CreateAttachmentGraph(&mainLayoutAttachments[0], nullptr);
	BasicShadow = GlobalRenderer::gRenderInstance->CreateAttachmentGraph(&mainLayoutAttachments[1], nullptr);
	MSAAShadowMapping = GlobalRenderer::gRenderInstance->CreateAttachmentGraph(&mainLayoutAttachments[2], nullptr);

	currentFrameGraphIndex = MSAAShadowMapping;

	frameGraphsCount = 3;

	mainPresentationSwapChain = GlobalRenderer::gRenderInstance->CreateSwapChainHandle(mainColorFormat, 800, 600);

	std::array<AttachmentClear, 1> ShadowMapViewerClears {
		CLEARCOLOR, {0.0, 0.0, 0.0, 0.0},
	};
	std::array<AttachmentClear, 4> MSAAPostClears {
		CLEARCOLOR, {0.0, 0.0, 0.0, 0.0},
		NOCLEAR, {0.0, 0.0, 0.0, 0.0},
		CLEARCOLOR, {0.0, 0.0, 0.0, 0.0},
		CLEARDEPTH, {1.0, 0}
	};

	std::array<AttachmentClear, 4> MSAAShadowMappingClears {
		NOCLEAR, {0.0, 0.0, 0.0, 0.0},
		CLEARDEPTH, {1.0, 0},
		CLEARCOLOR, {0.0, 0.0, 0.0, 0.0},
		CLEARDEPTH, {1.0, 0}
	};

	GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, BasicShadow, 0, ShadowMapViewerClears.data());
	GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, MSAAPost, 1, MSAAPostClears.data());
	GlobalRenderer::gRenderInstance->CreateSwapChainAttachment(mainPresentationSwapChain, MSAAShadowMapping, 1, MSAAShadowMappingClears.data());
	
	GlobalRenderer::gRenderInstance->CreatePerFrameAttachment(MSAAPost, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT, 800, 600, MSAAPostClears.data());
	GlobalRenderer::gRenderInstance->CreatePerFrameAttachment(MSAAShadowMapping, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT, mainShadowWidth, mainShadowHeight, MSAAShadowMappingClears.data());

	GlobalRenderer::gRenderInstance->CreateGraphicsQueueForAttachments(MSAAPost, 0, 10);
	GlobalRenderer::gRenderInstance->CreateGraphicsQueueForAttachments(MSAAPost, 1, 1);
	GlobalRenderer::gRenderInstance->CreateGraphicsQueueForAttachments(MSAAShadowMapping, 0, 10);
	GlobalRenderer::gRenderInstance->CreateGraphicsQueueForAttachments(MSAAShadowMapping, 1, 10);
	GlobalRenderer::gRenderInstance->CreateGraphicsQueueForAttachments(BasicShadow, 0, 1);

	GlobalRenderer::gRenderInstance->CreatePipelines(pds.data(), pds.size());

	GlobalRenderer::gRenderInstance->CreateShaderGraphs(layouts.data(), layouts.size());

	mainLinearSampler = GlobalRenderer::gRenderInstance->CreateSampler(7);

	std::array frameGraphs = { MSAAPost, MSAAShadowMapping, BasicShadow };
	std::array frameRenderPassSelection = { 0, 1, 0 };

	std::array fullScreenFrameGraphs = { MSAAPost, BasicShadow };

	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(0, 0, frameGraphs.data(), frameRenderPassSelection.data(),  2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(1, 1, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(5, 2, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(13, 3, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(14, 4, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(15, 5, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(16, 6, fullScreenFrameGraphs.data(), frameRenderPassSelection.data() + 1, 2);
	GlobalRenderer::gRenderInstance->CreateGraphicRenderStateObject(17, 7, frameGraphs.data() + 1, frameRenderPassSelection.data(), 1);

	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(2);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(3);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(4);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(6);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(7);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(8);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(9);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(10);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(11);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(12);
	GlobalRenderer::gRenderInstance->CreateComputePipelineStateObject(18);

	mainComputeQueueIndex = GlobalRenderer::gRenderInstance->CreateComputeQueue(15);

	CreateTexturePools();

	globalBufferLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT);
	globalIndexBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, globalIndexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	globalVertexBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainDeviceBuffer, globalVertexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalBufferDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(GENERIC, 0, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = GlobalRenderer::gRenderInstance->AllocateShaderResourceSet(GENERIC, 1, GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	globalMeshLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalMeshSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalMaterialsLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalMaterialsSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	globalLightBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalLightBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalLightTypesBuffer = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalLightTypesBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalDebugStructAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalDebugStructAllocSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalDebugTypesAlloc = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), globalDebugStructMaxCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalMaterialIndicesLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalMaterialIndicesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	globalRenderableLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	globalBlendDetailsLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalBlendDetailsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalBlendRangesLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalBlendRangesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalGeometryDescriptionsLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalGeometryDescriptionsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalGeometryRenderableLocation = GlobalRenderer::gRenderInstance->GetAllocFromBuffer(mainHostBuffer, globalGeometryRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);


	GlobalRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalBufferDescriptor, &globalBufferLocation, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance->descriptorManager.SetVariableArrayCount(globalTexturesDescriptor, 3, 512);

	std::array mainDrawBindGroups = { globalBufferDescriptor, globalTexturesDescriptor };

	GlobalRenderer::gRenderInstance->CreateRenderGraphData(MSAAPost, mainDrawBindGroups.data(), 2);
	GlobalRenderer::gRenderInstance->CreateRenderGraphData(MSAAShadowMapping, mainDrawBindGroups.data(), 2);
	GlobalRenderer::gRenderInstance->CreateRenderGraphData(BasicShadow, nullptr, 0);


	CreateSkyBox();
	CreateDebugCommandBuffers(globalDebugStructMaxCount);
	CreateLightAssignments(globalLightMax);
	CreateMeshWorldAssignment(mainGrid.numberOfDivision * mainGrid.numberOfDivision * mainGrid.numberOfDivision);
	CreateGenericMeshCommandBuffers(globalMeshCountMax);
	CreateShadowMapManager(4, globalMeshCountMax, 1024, 1024, mainShadowWidth, mainShadowHeight);
	CreateMSAAPostFullScreen();

	AddLight(mainSpotLight, LightType::SPOT);
	AddLight(mainDirectionalLight, LightType::DIRECTIONAL);
	
	c.CamLookAt(Vector3f(25.0 * -mainDirectionalLight.direction.x, 25.0 * -mainDirectionalLight.direction.y,  25.0 * -mainDirectionalLight.direction.z), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance->GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance->GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));

	WriteCameraMatrix(GlobalRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);	
	
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
	if (!wordCounts.size()) {
		queueSema.Notify();
		return;
	}
	int wordCount = wordCounts.front();

	wordCounts.pop();
	queueSema.Notify();

	StringView* commandType = (StringView*)(ThreadSharedMessageQueue.bufferLocation);

	ExecuteCommands(*commandType, wordCount);
}

void ApplicationLoop::AddCommandTS(int wordCount)
{
	SemaphoreGuard lock(std::ref(queueSema));
	wordCounts.push(wordCount);
}

void ApplicationLoop::SetRunning(bool set)
{
	running = set;
}

void ApplicationLoop::LoadObject(const StringView& file)
{
	SMBFile SMB(file, &GlobalInputScratchAllocator, &mainAppLogger);

	LoadSMBFile(SMB);
}

void ApplicationLoop::LoadThreadedWrapper(StringView& file)
{
	ThreadManager::LaunchOSASyncThread(LoadObjectThreaded, &file);
}

int ApplicationLoop::FindWords(const char* words, int charCount)
{
	int wordCount = 1;
	size_t size = charCount;
	int i = 0, j = 1;
	int wordSize; 
	while (j < size) {

		if (words[j] == 0x20)
		{
			wordSize = j - i;
			StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
			char* stringData = (char*)AppInstanceTempAllocator.Allocate(wordSize);
			out->stringData = stringData;
			out->charCount = wordSize;
			memcpy(stringData, words + i, wordSize);
			wordCount++;
			j++;
			i = j;
		}
		else if (words[j] == 0x22) //capture quote
		{
			while (j <= size - 1 && words[j++] != 0x22)
			{
			
			}
			wordSize = j - i;
			StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
			char* stringData = (char*)AppInstanceTempAllocator.Allocate(wordSize);
			out->stringData = stringData;
			out->charCount = wordSize;
			memcpy(stringData, words + i, wordSize);
			wordCount++;
			i = j + 1;
		}
		else 
		{
			j++;
		}
	}

	wordSize = j - i;
	StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
	char* stringData = (char*)AppInstanceTempAllocator.Allocate(wordSize);
	out->stringData = stringData;
	out->charCount = wordSize;
	memcpy(stringData, words + i, wordSize);

	return wordCount;
}

int ApplicationLoop::AddMaterialToDeviceMemory(int count, int* ids)
{
	int ret = globalMaterialRangeCount;

	if (globalMaterialRangeMax <= globalMaterialRangeCount+count)
	{
		return -1;
	}

	globalMaterialRangeCount += count;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(ids, globalMaterialIndicesLocation, sizeof(uint32_t) * count, sizeof(uint32_t) * ret, TransferType::CACHED);

	return ret;
}

int ApplicationLoop::CreateRenderable(const Matrix4f& mat, int geomIndex,  int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount)
{
	RenderableMeshCPUData* meshCpuRenderable = nullptr;

	int meshRenderableIndex = -1;

	if (globalRenderableCount == globalRenderableMax)
	{
		return -1;
	}

	std::tie(meshRenderableIndex, meshCpuRenderable) = renderablesMeshObjects.Allocate();

	GPUMeshRenderable renderable{};

	int renderableLocation = globalRenderableCount++;

	meshCpuRenderable->instanceCount = renderable.instanceCount = instanceCount;
	meshCpuRenderable->meshIndex = renderable.meshIndex = meshIndex;
	meshCpuRenderable->meshBlendRangeIndex = renderable.blendLayersStart = blendStart;
	meshCpuRenderable->meshMaterialRangeIndex = renderable.materialStart = materialStart;
	meshCpuRenderable->meshBlendRangeCount = meshCpuRenderable->meshMaterialCount = renderable.materialCount = materialCount;
	meshCpuRenderable->renderableIndex = meshRenderableIndex;
	renderable.geomIndex = geomIndex;
	
	renderable.transform = mat;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&renderable, globalRenderableLocation, sizeof(GPUMeshRenderable), sizeof(GPUMeshRenderable) * renderableLocation, TransferType::CACHED);

	return renderableLocation;
}

int ApplicationLoop::CreateMaterial(
	int flags,
	int* texturesIDs,
	int textureCount,
	const Vector4f& color
)
{
	GPUMaterial material{};
	uint32_t* ptr = material.textureHandles.comp;

	if (globalMaterialsIndex == globalMaterialsMax)
	{
		return -1;
	}

	material.albedoColor = color;

	material.materialFlags = flags;

	for (int i = 0; i < textureCount; i++)
	{
		ptr[i] = texturesIDs[i];
	}

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * globalMaterialsIndex, TransferType::CACHED);

	return globalMaterialsIndex++;
}

int ApplicationLoop::CreateMaterial(
	int flags,
	int* texturesIDs,
	int textureCount,
	const Vector4f& diffuseColor,
	const Vector4f& specularColor,
	float shininess,
	const Vector4f& emissiveColor
)
{
	GPUMaterial material{};

	uint32_t* ptr = material.textureHandles.comp;

	if (globalMaterialsIndex == globalMaterialsMax)
	{
		return -1;
	}

	material.albedoColor = diffuseColor;
	material.emissiveColor = emissiveColor;
	material.specularColor = specularColor;
	material.shininess = shininess;

	material.materialFlags = flags;

	for (int i = 0; i < textureCount; i++)
	{
		ptr[i] = texturesIDs[i];
	}

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * globalMaterialsIndex, TransferType::CACHED);

	return globalMaterialsIndex++;
}

void ApplicationLoop::CreateMeshHandle(
	int meshIndex,
	void* vertexData, void* indexData,
	int vertexFlags, int vertexCount, int vertexStride,
	int indexStride, int indexCount,
	Sphere& sphere,
	int vertexAlloc, int indexAlloc
)
{
	MeshCPUData* mesh = nullptr;

	if (globalMeshCount == globalMeshCountMax)
	{
		return;
	}

	mesh = meshCPUData.Update(meshIndex);

	mesh->indicesCount = indexCount;

	mesh->verticesCount = vertexCount;

	mesh->vertexSize = vertexStride;

	mesh->indexSize = indexStride;

	mesh->meshDeviceIndex = meshIndex;

	mesh->vertexFlags = vertexFlags;

	mesh->deviceIndices = indexAlloc;

	mesh->deviceVertices = vertexAlloc;

	GPUMeshDetails* handles = (GPUMeshDetails*)meshObjectSpecificAlloc.Allocate(sizeof(GPUMeshDetails));

	handles->vertexFlags = vertexFlags;
	handles->stride = vertexStride;

	handles->indexCount = mesh->indicesCount;
	handles->firstIndex = indexAlloc / mesh->indexSize;
	handles->vertexByteOffset = vertexAlloc;

	//memcpy(&handles->minMaxBox, &box, sizeof(AxisBox));
	memcpy(&handles->sphere, &sphere, sizeof(Vector4f));

	int meshSpecificAlloc = globalMeshCount++;

	mesh->meshDeviceIndex = meshSpecificAlloc;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(handles, globalMeshLocation, sizeof(GPUMeshDetails), meshSpecificAlloc * sizeof(GPUMeshDetails), TransferType::MEMORY);
}


int ApplicationLoop::CreateMeshHandle(
	void* vertexData, void* indexData,
	int vertexFlags, int vertexCount, int vertexStride,
	int indexStride, int indexCount,
	 Sphere& sphere,
	int vertexAlloc, int indexAlloc
)
{
	MeshCPUData* mesh = nullptr;

	uint32_t meshIndex;

	if (globalMeshCount == globalMeshCountMax)
	{
		return -1;
	}

	std::tie(meshIndex, mesh) = meshCPUData.Allocate();

	mesh->indicesCount = indexCount;

	mesh->verticesCount = vertexCount;

	mesh->vertexSize = vertexStride;

	mesh->indexSize = indexStride;

	mesh->meshDeviceIndex = meshIndex;

	mesh->vertexFlags = vertexFlags;

	mesh->deviceIndices = indexAlloc;

	mesh->deviceVertices = vertexAlloc;

	GPUMeshDetails* handles = (GPUMeshDetails*)meshObjectSpecificAlloc.Allocate(sizeof(GPUMeshDetails));

	handles->vertexFlags = vertexFlags;
	handles->stride = vertexStride;

	handles->indexCount = mesh->indicesCount;
	handles->firstIndex = indexAlloc / mesh->indexSize;
	handles->vertexByteOffset = vertexAlloc;

	//memcpy(&handles->minMaxBox, &box, sizeof(AxisBox));
	memcpy(&handles->sphere, &sphere, sizeof(Vector4f));

	int meshSpecificAlloc = globalMeshCount++;

	mesh->meshDeviceIndex = meshSpecificAlloc;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(handles, globalMeshLocation, sizeof(GPUMeshDetails), meshSpecificAlloc * sizeof(GPUMeshDetails), TransferType::MEMORY);

	return meshIndex;
}

void ApplicationLoop::SetPositionOfGeometry(int geomIndex, const Vector3f& pos)
{

	auto rendInst = GlobalRenderer::gRenderInstance;
	RenderableGeomCPUData* geom = &renderablesGeomObjects.dataArray[geomIndex];

	int meshCount = geom->renderableMeshCount;
	int meshStart = geom->renderableMeshStart; 

	for (int i = 0; i < meshCount; i++)
	{
		RenderableMeshCPUData* mesh = &renderablesMeshObjects.dataArray[meshStart + i];

		GPUMeshRenderable renderable{};

		int renderableLocation = mesh->renderableIndex;

		renderable.instanceCount = mesh->instanceCount;
		renderable.meshIndex = mesh->meshIndex;
		renderable.blendLayersStart = mesh->meshBlendRangeIndex;
		renderable.materialStart = mesh->meshMaterialRangeIndex;
		renderable.materialCount = mesh->meshMaterialCount;

		renderable.transform = Identity4f();;

		renderable.transform.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

		rendInst->UpdateDriverMemory(&renderable, globalRenderableLocation, sizeof(GPUMeshRenderable), sizeof(GPUMeshRenderable) * renderableLocation, TransferType::CACHED);
	}
}

void ApplicationLoop::ExecuteCommands(const StringView& command, int wordCount)
{

	if (strncmp(command.stringData, "load", 4) == 0)
	{
		//LoadThreadedWrapper(args.at(0));
	}
	else if (strncmp(command.stringData, "end", 4) == 0)
	{
		SetRunning(false);
	}
	else if (strncmp(command.stringData, "positiong", 9) == 0)
	{
		if (wordCount != 5)
			return;
		StringView* args = (StringView*)(ThreadSharedMessageQueue.bufferLocation);

		int geomIndex = std::stoi(std::string(args[1].stringData, args[1].charCount));
		float x1 = std::stof(std::string(args[2].stringData, args[2].charCount));
		float y1 = std::stof(std::string(args[3].stringData, args[3].charCount));
		float z1 = std::stof(std::string(args[4].stringData, args[4].charCount));
		SetPositionOfGeometry(geomIndex, Vector3f(x1, y1, z1));
	}

	ThreadSharedMessageQueue.Read();
}

int ApplicationLoop::CreateGPUGeometryDetails(const AxisBox& minMaxBox)
{
	auto rendInst = GlobalRenderer::gRenderInstance;
	
	int geomDescriptionsIndex = -1;

	if (globalGeometryDescriptionsCount == globalGeometryDescriptionsCountMax)
	{
		return geomDescriptionsIndex;
	}

	geomDescriptionsIndex = globalGeometryDescriptionsCount++;

	GPUGeometryDetails details{};

	details.minMaxBox = minMaxBox;

	rendInst->UpdateDriverMemory(&details, globalGeometryDescriptionsLocation, sizeof(GPUGeometryDetails), sizeof(GPUGeometryDetails) * geomDescriptionsIndex, TransferType::CACHED);

	return geomDescriptionsIndex;
}

int ApplicationLoop::CreateGPUGeometryRenderable(const Matrix4f& matrix, int geomDesc, int renderableStart, int renderableCount)
{
	auto rendInst = GlobalRenderer::gRenderInstance;
	int geomRenderableIndex = -1;
	if (globalGeometryRenderableCount == globalGeometryRenderableCountMax)
	{
		return geomRenderableIndex;
	}

	geomRenderableIndex = globalGeometryRenderableCount++;

	GPUGeometryRenderable renderable{};

	renderable.geomDescIndex = geomDesc;
	renderable.renderableStart = renderableStart;
	renderable.renderableCount = renderableCount;
	renderable.transform = matrix;

	rendInst->UpdateDriverMemory(&renderable, globalGeometryRenderableLocation, sizeof(GPUGeometryRenderable), sizeof(GPUGeometryRenderable) * geomRenderableIndex, TransferType::CACHED);

	return geomRenderableIndex;
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

		int wordCount = loop->FindWords(inputBuffer, numberOfBytesRead - 2);

		loop->AddCommandTS(wordCount);

		std::osyncstream(std::cout) << "Hit enter and then write command > ";
	}


	return;
}

int CreateBlendRange(int* blendIDs, int blendCount)
{
	int loc = globalBlendRangeCount;

	globalBlendRangeCount += blendCount;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(blendIDs, globalBlendRangesLocation, sizeof(int) * blendCount, sizeof(int) * loc, TransferType::CACHED);

	return loc;
}

int CreateBlendDetails(BlendMaterialType type, float constantAlpha)
{
	int loc = globalBlendDetailCount++;

	GPUBlendDetails details;

	details.type = type;
	details.alphaBlend = constantAlpha;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&details, globalBlendDetailsLocation, sizeof(GPUBlendDetails),  sizeof(GPUBlendDetails) * loc, TransferType::CACHED);

	return loc;
}

int CreateBlendDetails(BlendMaterialType type, int mapID)
{
	int loc = globalBlendDetailCount++;

	GPUBlendDetails details;

	details.type = type;
	details.alphaMapHandle = mapID;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&details, globalBlendDetailsLocation, sizeof(GPUBlendDetails), sizeof(GPUBlendDetails) * loc, TransferType::CACHED);

	return loc;
}

void CreateUniformGrid()
{
	float div = (float)(mainGrid.numberOfDivision);

	int count = mainGrid.numberOfDivision;

	Vector4f extent = (mainGrid.max - mainGrid.min) / div;

	float xMove = extent.x;
	float yMove = extent.y;
	float zMove = extent.z;

	Vector4f maxHalf = mainGrid.max - (extent * 0.5);

	Vector4f half = extent / 2.0;

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

SMBImageFormat ConvertAppImageFormatToSMBFormat(ImageFormat format)
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

void LoadObjectThreaded(void* data)
{
	StringView* file = (StringView*)data;

	int charCount = file->charCount;
	int offset = 0;

	if (file->stringData[0] == 0x22)
	{
		offset++;
		charCount--;
	}
	
	if (file->stringData[file->charCount - 1] == 0x22)
	{
		charCount--;
	}

	StringView truncatedView{ file->stringData + offset, charCount };

	loop->LoadObject(truncatedView);
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
		GlobalRenderer::gRenderInstance->IncreaseMSAA(currentFrameGraphIndex, 0);
	}

	if (keyActions[KC_ONE].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance->DecreaseMSAA(currentFrameGraphIndex, 0);
	}

	static int dir = -1;

	if (keyActions[KC_Q].state == PRESSED)
	{
		int next = currentFrameGraphIndex + dir;

		if (next < 0)
		{
			dir = 1;
			next = 0;
		}
		else if (next >= frameGraphsCount)
		{
			dir = -1;
			next = frameGraphsCount - 1;
		}

		currentFrameGraphIndex = next;

		GlobalRenderer::gRenderInstance->ResetCommandList();
		GlobalRenderer::gRenderInstance->AddCommandQueue(mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance->AddCommandQueue(currentFrameGraphIndex, ATTACHMENT_COMMANDS);
	}
}


void UpdateLight(GPULightSource& lightDesc, int lightIndex)
{
	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&lightDesc, globalLightBuffer, sizeof(GPULightSource), sizeof(GPULightSource) * lightIndex, TransferType::CACHED);
}

EntryHandle ReadCubeImage(StringView* name, int textureCount, TextureIOType ioType)
{
	TextureDetails* details;

	int textureStart = mainDictionary.AllocateNTextureHandles(1, &details);

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(name->stringData, name->charCount, READ, &outHandle);

	int size = outHandle.fileLength;

	void* fileData = GlobalInputScratchAllocator.Allocate(size);

	OSReadFile(&outHandle, size, (char*)fileData);

	OSCloseFile(&outHandle);

	int filePointer = ReadBMPDetails((char*)fileData, details);

	details->data = (char*)mainDictionary.AllocateImageCache(details->dataSize);

	details->currPointer = details->data;

	for (int i = 1; i < textureCount; i++)
	{
		ReadBMPData((char*)fileData, filePointer, details);

		nRet = OSOpenFile(name->stringData, name->charCount, READ, &outHandle);

		size = outHandle.fileLength;

		fileData = GlobalInputScratchAllocator.Allocate(size);

		OSReadFile(&outHandle, size, (char*)fileData);

		OSCloseFile(&outHandle);

		TextureDetails stubDetails{};

		filePointer = ReadBMPDetails((char*)fileData, &stubDetails);

		if (stubDetails.width != details->width) {
			printf("Mismatched Image array\n");
		}

		details->currPointer = (char*)mainDictionary.AllocateImageCache(details->dataSize);
	}

	ReadBMPData((char*)fileData, filePointer, details);

	details->arrayLayers = textureCount;

	EntryHandle ret = mainDictionary.textureHandles[textureStart] =
		GlobalRenderer::gRenderInstance->CreateCubeImageHandle(
			textureCount * details->dataSize,
			details->width,
			details->height,
			details->miplevels,
			details->type,
			loop->GetPoolIndexByFormat(details->type),
			mainLinearSampler);

	GlobalRenderer::gRenderInstance->UpdateImageMemory(
		details->data,
		ret,
		nullptr,
		textureCount * details->dataSize,
		details->width,
		details->height,
		details->miplevels,
		details->arrayLayers,
		details->type
	);

	return ret;
}

int Read2DImage(StringView* name, int mipCounts, TextureIOType ioType)
{
	TextureDetails* details;

	int textureStart = mainDictionary.AllocateNTextureHandles(1, &details);

	uint32_t* mipSizes = (uint32_t*)mainDictionary.AllocateImageCache(sizeof(uint32_t) * mipCounts);

	int totalBlobSize = 0;
	EntryHandle ret;

	for (int i = 0; i < mipCounts; i++)
	{
		TextureDetails stubDetails{};

		OSFileHandle outHandle{};

		int nRet = OSOpenFile(name->stringData, name->charCount, READ, &outHandle);

		int size = outHandle.fileLength;

		void* fileData = GlobalInputScratchAllocator.Allocate(size);

		OSReadFile(&outHandle, size, (char*)fileData);

		OSCloseFile(&outHandle);

		int filePointer = ReadBMPDetails((char*)fileData, &stubDetails);

		stubDetails.data = (char*)mainDictionary.AllocateImageCache(stubDetails.dataSize);

		stubDetails.currPointer = stubDetails.data;

		totalBlobSize += stubDetails.dataSize;

		mipSizes[i] = stubDetails.dataSize;

		ReadBMPData((char*)fileData, filePointer, &stubDetails);

		if (!i)
		{
			memcpy(details, &stubDetails, sizeof(TextureDetails));
		}

	}


	details->miplevels = mipCounts;
	details->arrayLayers = 1;

	ret = mainDictionary.textureHandles[textureStart] =
		GlobalRenderer::gRenderInstance->CreateImageHandle(
			totalBlobSize,
			details->width,
			details->height,
			details->miplevels,
			details->type,
			loop->GetPoolIndexByFormat(details->type),
			mainLinearSampler);

	GlobalRenderer::gRenderInstance->UpdateImageMemory(
		details->data,
		mainDictionary.textureHandles[textureStart],
		mipSizes,
		totalBlobSize,
		details->width,
		details->height,
		details->miplevels,
		details->arrayLayers,
		details->type
	);

	DeviceHandleArrayUpdate update;

	update.resourceCount = 1;
	update.resourceDstBegin = textureStart;
	update.resourceHandles = mainDictionary.textureHandles.data() + textureStart;

	GlobalRenderer::gRenderInstance->UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);

	return textureStart;
}

int AddLight(GPULightSource& lightDesc, LightType type)
{

	if (globalLightCount == globalLightMax)
	{
		return -1;
	}

	int lightLocation = globalLightCount++;

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&lightDesc, globalLightBuffer, sizeof(GPULightSource), sizeof(GPULightSource) * lightLocation, TransferType::CACHED);

	GlobalRenderer::gRenderInstance->UpdateDriverMemory(&type, globalLightTypesBuffer, sizeof(LightType), sizeof(LightType) * lightLocation, TransferType::CACHED);

	if (type == LightType::DIRECTIONAL)
	{
		Vector3f pos;

		float distance = 25.0f;

		pos = Vector3f(-lightDesc.direction.x, -lightDesc.direction.y, -lightDesc.direction.z);

		pos = pos * distance;

		LTM ltm{};

		LookAt(&ltm, pos, Vector3f(0.0, 0.0, 0.0), Vector3f(0.0, 1.0, 0.0));

		struct
		{
			Matrix4f view;
			Matrix4f proj;
		} Upload{
			.view = CreateViewMatrix(&ltm),
			.proj = CreateOrthographicMatrix(-10.0, 10.0f, -10.0f, 10.0f, 1.0f, 100.0f)
		};

		GlobalRenderer::gRenderInstance->UpdateDriverMemory(&Upload, mainShadowMapManager.shadowMapViewProjAlloc, sizeof(Upload), sizeof(Upload) * lightLocation, TransferType::CACHED);

		ShadowMapView view{};

		int shadowXScale = mainShadowMapManager.atlasWidth / mainShadowMapManager.avgShadowWidth;
		int shadowYScale = mainShadowMapManager.atlasHeight / mainShadowMapManager.avgShadowHeight;

		view.xOff = (float)(mainShadowMapManager.avgShadowWidth) * (mainShadowMapManager.zoneAlloc % shadowXScale);
		view.xOff /= (float)mainShadowMapManager.atlasWidth;

		view.yOff = (float)(mainShadowMapManager.avgShadowHeight) * (mainShadowMapManager.zoneAlloc++ / shadowYScale);
		view.yOff /= (float)mainShadowMapManager.atlasHeight;
		view.xScale = 1.0f / (float)shadowXScale;
		view.yScale = 1.0f / (float)shadowYScale;

		GlobalRenderer::gRenderInstance->UpdateDriverMemory(&view, mainShadowMapManager.shadowMapAtlasViewsAlloc, sizeof(view), sizeof(view) * lightLocation, TransferType::CACHED);

	}

	return lightLocation;
}