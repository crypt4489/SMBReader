#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"
#include "Exporter.h"
#include "logger/Logger.h"
#include "TextureDictionary.h"
#include "SMBTexture.h"
#include "allocator/AppAllocator.h"

#include <cassert>

ApplicationLoop* loop;

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
static SlabAllocator StartupMemoryAllocator{ StartupMemory, sizeof(StartupMemory) };

static std::array<StringView, 9> pds = {
	StartupMemoryAllocator.AllocateFromNullStringCopy("GenericPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("TextPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("DebugPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("NormalDebugPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("SkyboxPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("OutlinePipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("FullscreenPipeline.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMap.pld"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("JointVisualPipeline.pld"),
};

static std::array<StringView, 20> layouts = {
		StartupMemoryAllocator.AllocateFromNullStringCopy("3DTexturedLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("TextLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("InterpolateMeshLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PolynomialLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("IndirectCull.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("DebugDraw.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("IndirectDebug.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PrefixSum.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("PrefixSumAdd.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("WorldObjectDivison.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("MeshWorldAssignments.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("LightObjectDivision.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("LightWorldAssignment.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("NormalDebug.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("Skybox.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("OutlineLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("FullscreenLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMapLayout.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowMapClipping.sgr"),
		StartupMemoryAllocator.AllocateFromNullStringCopy("JointVisualSL.sgr"),
};

static std::array<StringView, 5> mainLayoutAttachments =
{
	StartupMemoryAllocator.AllocateFromNullStringCopy("MSAAPostProcess.adf"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("BasicShadowMapAtt.adf"),
	StartupMemoryAllocator.AllocateFromNullStringCopy("ShadowAtlasUse.adf")
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
static const int globalDebugStructAllocSize = 10 * KiB;
static int globalDebugTypesAlloc = 0;
static int globalDebugStructCount = 0;

static int globalBufferLocation;
static int globalBufferDescriptor;
static int globalTexturesDescriptor;

static int globalMeshLocation;
static const int globalMeshSize = 24 * KiB;
static int globalMeshCount = 0;

static int globalGeometryDescriptionsLocation;
static const int globalGeometryDescriptionsSize = 1 * KiB;
static int globalGeometryDescriptionsCount = 0;

static int globalGeometryRenderableLocation;
static const int globalGeometryRenderableSize = 1 * KiB;
static int globalGeometryRenderableCount = 0;

static int globalRenderableLocation;
static const int globalRenderableSize = 24 * KiB;
static int globalRenderableCount = 0;

static int globalMaterialIndicesLocation;
static const int globalMaterialIndicesSize = 4 * KiB;
static int globalMaterialRangeCount = 0;

static int globalMaterialsLocation; 
static const int globalMaterialsSize = 4 * KiB;
static int globalMaterialsIndex = 0;

static int globalBlendDetailsLocation;
static int globalBlendRangesLocation;
static const int globalBlendDetailsSize = 1 * KiB;
static const int globalBlendRangesSize = 1 * KiB;
static int globalBlendDetailCount = 0;
static int globalBlendRangeCount = 0;

static const int globalMaterialsMax = globalMaterialsSize / sizeof(GPUMaterial);
static const int globalBlendDetailMax = globalBlendDetailsSize / sizeof(GPUBlendDetails);
static const int globalBlendRangeMax = globalBlendRangesSize / sizeof(BlendRanges);
static const int globalMaterialRangeMax = globalMaterialIndicesSize / sizeof(uint32_t);
static const int globalRenderableMax = globalRenderableSize / sizeof(GPUMeshRenderable);
static const int globalDebugStructMaxCount = globalDebugStructAllocSize / sizeof(GPUDebugRenderable);
static const int globalMeshCountMax = globalMeshSize / sizeof(GPUMeshDetails);
static const int globalGeometryDescriptionsCountMax = globalGeometryDescriptionsSize / sizeof(GPUGeometryDetails);
static const int globalGeometryRenderableCountMax = globalGeometryRenderableSize / sizeof(GPUGeometryRenderable);

static int shadowMapIndex = 0;

static char vertexAndIndicesMemory[16 * MiB];
static char geometryObjectSpecificMemory[4 * KiB];
static char mainTextureCacheMemory[256 * MiB];
static char mainOSDataManagement[KiB];

static SlabAllocator osAllocator(mainOSDataManagement, sizeof(mainOSDataManagement));
static SlabAllocator vertexAndIndicesAlloc(vertexAndIndicesMemory, sizeof(vertexAndIndicesMemory));
static SlabAllocator geometryObjectSpecificAlloc(geometryObjectSpecificMemory, sizeof(geometryObjectSpecificMemory));

static DeviceSlabAllocator indexBufferAlloc(globalIndexBufferSize);

static DeviceSlabAllocator vertexBufferAlloc(globalVertexBufferSize);

static TextureDictionary mainDictionary;

static OSThreadHandle threadHandle;

static int currentFrameGraphIndex = 4;
static int mainComputeQueueIndex = 0;
static int mainFullScreenPipeline = 0;

static int jointMeshCPU = -1;
static int jointMeshPipeline = -1;


static int jointMeshWorldMatrixMaxCount = 164;
static int jointMeshWorldMatrixCount = 0;

static int jointMeshWorldMatrix = -1;
static int jointMeshOrientations = -1;
static int jointMeshPositions = -1;
static int jointMeshScales = -1;
static int jointMeshParentIndices = -1;

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


static ArrayAllocator<int, MAX_MESH_TEXTURES> meshTextureHandles{};
static ArrayAllocator<MeshCPUData, globalMeshCountMax> meshCPUData{};
static ArrayAllocator<GeometryCPUData, globalGeometryDescriptionsCountMax> geometryCPUData{};
static ArrayAllocator<RenderableGeomCPUData, globalGeometryRenderableCountMax> renderablesGeomObjects{};
static ArrayAllocator<RenderableMeshCPUData, globalRenderableMax> renderablesMeshObjects{};

static void ProcessKeys(GenericKeyAction keyActions[KC_COUNT]);

static UniformGrid mainGrid = {	
	.max = Vector4f(100.0f, 50.0f, 100.0f, 0.0),
	.min = Vector4f(-100.0f, -50.0f, -100.0f, 0.0),
	.numberOfDivision = 5,
};

static int skyboxPipeline = 0;
static int skyboxCubeImage = -1;

static char RenderInstanceMemoryPool[256 * MiB];
static char RenderInstanceTemporaryPool[64 * KiB];
static char AppInstanceTempMemory[64 * KiB];
static char GlobalInputScratchMemory[128 * MiB];
static char ThreadSharedCmdMemory[4 * KiB];
static char LoggerMessageMemory[64 * KiB];

static SlabAllocator RenderInstanceMemoryAllocator{ RenderInstanceMemoryPool, sizeof(RenderInstanceMemoryPool) };
static RingAllocator RenderInstanceTemporaryAllocator{ RenderInstanceTemporaryPool, sizeof(RenderInstanceTemporaryPool) };
static RingAllocator AppInstanceTempAllocator{ AppInstanceTempMemory, sizeof(AppInstanceTempMemory) };
static SlabAllocator GlobalInputScratchAllocator{ GlobalInputScratchMemory, sizeof(GlobalInputScratchMemory) };
static MessageQueue ThreadSharedMessageQueue{ ThreadSharedCmdMemory, sizeof(ThreadSharedCmdMemory) };
static Logger mainAppLogger{ LoggerMessageMemory, sizeof(LoggerMessageMemory)};

static int mainLinearSampler = -1;

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

static WindowManager mainWindow;

static void ScanSTDIN(void*);
static int AddLight(GPULightSource& lightDesc, LightType type);
static void UpdateLight(GPULightSource& lightDesc, int lightIndex);
static int CreateBlendRange(int gpuBlendRangeID, int* blendIDs, int blendCount);
static int CreateBlendDetails(int gpuBlendDescID, BlendMaterialType type, float constantAlpha);
static int CreateBlendDetails(int gpuBlendDescID, BlendMaterialType type, int mapID);
static int ReadCubeImage(StringView* name, int textureCount, TextureIOType ioType);
static int Read2DImage(StringView* name, int mipCounts, TextureIOType ioType);
static void CreateUniformGrid();
static SMBImageFormat ConvertAppImageFormatToSMBFormat(ImageFormat format);
static ImageFormat ConvertSMBImageToAppImage(SMBImageFormat fmt);
static void LoadObjectThreaded(void* data);;
static void PrintDebugMemoryAllocation();
static void CreateBitTangentFromNormal(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator);

ApplicationLoop::ApplicationLoop(ProgramArgs& _args) :
	args(_args),
	queueSema(Semaphore()),
	running(true),
	cleaned(true)
{
	loop = this;
	Execute();
}

ApplicationLoop::~ApplicationLoop() { 
	if (!cleaned) 
	{ 
		CleanupRuntime(); 
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

	StringView mainInputString = AppInstanceTempAllocator.AllocateFromNullStringCopy(args.inputFile.string().c_str());

	OSGetSTDOutput(&mainAppLogger.fileHandle);

	if (args.justexport)
	{
		const int MAX_SMB_STRING_NAME = 150;

		SMBFile mainSMB{};

		int loadSMBRet = mainSMB.LoadFile(mainInputString, &GlobalInputScratchAllocator, &mainAppLogger);

		if (loadSMBRet)
		{
			mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Cannot read SMB file for export"));
			mainAppLogger.ProcessMessage();
			return;
		}

		StringView fileName{};

		fileName.stringData = (char*)GlobalInputScratchAllocator.Allocate(MAX_SMB_STRING_NAME);

		FileManager::ExtractFileNameFromPath(&mainInputString, &fileName);

		FileManager::SetFileCurrentDirectory(&fileName);

		ExportChunksFromFile(&mainSMB, &GlobalInputScratchAllocator);
	}
	else
	{
		InitializeRuntime();

		cleaned = false;

		CreateCrateObject();

		CreateCornerWall(10.0f, 10.0f, 2.0f, 1.0f);



		LoadObject(mainInputString);

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

					mainWindow.SetWindowTitle(view);
				
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

		uint32_t framesInFlight = GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT;

		int updateMainDrawCommand = framesInFlight, updateMainDebugCommand = framesInFlight, updateLights = framesInFlight;

		int mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;

		int mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;

		int previousGlobalLightCount = globalLightCount;

		GlobalRenderer::gRenderInstance.AddCommandQueue(mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance.AddCommandQueue(currentFrameGraphIndex, ATTACHMENT_COMMANDS);

		//GlobalRenderer::gRenderInstance.EndFrame();

		DeviceHandleArrayUpdate samplerUpdate;

		samplerUpdate.resourceCount = 1;
		samplerUpdate.resourceDstBegin = 0;
		samplerUpdate.resourceHandles = &mainLinearSampler;

		GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 0, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

		while (running)
		{
			mainWindow.PollEvents();

			if (mainWindow.ShouldCloseWindow()) break;

			ProcessKeys(mainWindow.windowData.info.actions);

			bool cameraMove = MoveCamera(FPS);

			if (previousGlobalLightCount != globalLightCount || updateLights > 0)
			{
				if (!updateLights) updateLights = framesInFlight;

				previousGlobalLightCount = globalLightCount;

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.preWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.prefixSumPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.postWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, mainShadowMapManager.shadowClippingPipeline);

				updateLights--;
			}

			if (cameraMove || mainDrawRenderableCount != mainIndirectDrawData.commandBufferCount || updateMainDrawCommand > 0)
			{
				if (!updateMainDrawCommand || cameraMove) {
					updateMainDrawCommand = framesInFlight;
					GlobalRenderer::gRenderInstance.InsertTransferCommand(mainIndirectDrawData.commandBufferCountAlloc, 8, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
				}

				updateMainDrawCommand--;

				mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.preWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.prefixSumPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.postWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, mainIndirectDrawData.indirectCullPipeline);
			}

			if (cameraMove || debugIndirectDrawData.commandBufferCount != mainDebugDrawRenderableCount || updateMainDebugCommand > 0)
			{
				if (!updateMainDebugCommand || cameraMove)
				{
					GlobalRenderer::gRenderInstance.InsertTransferCommand(debugIndirectDrawData.commandBufferCountAlloc, 8, 0, 0, COMPUTE_BARRIER, WRITE_SHADER_RESOURCE | READ_SHADER_RESOURCE);
					updateMainDebugCommand = framesInFlight;
				}
				mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;
				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, debugIndirectDrawData.indirectCullPipeline);
				updateMainDebugCommand--;
			}

			if (mainWindow.windowData.info.HandleResizeRequested())
			{
				GlobalRenderer::gRenderInstance.RecreateSwapChain(mainPresentationSwapChain);
				c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));
				UpdateCameraMatrix();
				RecreateFrameGraphAttachments(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain), GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain));
			}

			imageIndex = GlobalRenderer::gRenderInstance.BeginFrame(mainPresentationSwapChain);

			if (imageIndex != ~0ui32) 
			{
				if (currentFrameGraphIndex == MSAAShadowMapping)
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(smdpd.shadowMapPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(jointMeshPipeline, currentFrameGraphIndex, 1);
				}
				else if (currentFrameGraphIndex == BasicShadow)
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(smdpd.fullScreenPipeline, currentFrameGraphIndex, 0);
				}
				else 
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(jointMeshPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 0);

					if (currentFrameGraphIndex == MSAAPost)
						GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainFullScreenPipeline, currentFrameGraphIndex, 1);


				}

				GlobalRenderer::gRenderInstance.DrawScene(imageIndex);

				GlobalRenderer::gRenderInstance.SubmitFrame(mainPresentationSwapChain, imageIndex);
			}
			else
			{
				mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Missed image handle during drawing, closing app"));
				running = false;
			}
			
			GlobalRenderer::gRenderInstance.EndFrame();	

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

	auto& rendInst = GlobalRenderer::gRenderInstance;

	for (int i = 0; i < 4; i++)
	{
		mainDictionary.texturePoolsFormat[i] = formats[i];
		mainDictionary.texturePoolsSize[i] = 128 * MiB;
		mainDictionary.texturePoolsAllocatedSize[i] = 0;
		
		int texturePoolHandle = rendInst.CreateImagePool(
			mainDictionary.texturePoolsSize[i],
			formats[i], MAX_IMAGE_DIM, MAX_IMAGE_DIM, false
		);

		if (texturePoolHandle < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, "Failed to create a texture image pool", 37);
		}

		mainDictionary.texturePoolHandle[i] = texturePoolHandle;

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

	WriteCameraMatrix();
}

void ApplicationLoop::WriteCameraMatrix()
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&c.View, globalBufferLocation, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 0, TransferType::MEMORY);
}

int ApplicationLoop::GetPoolIndexByFormat(ImageFormat format)
{
	int ret = -1;
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
	default:
		mainAppLogger.AddLogMessage(LOGERROR, "unhandled texture format", 24);
		break;
	}
	return ret;
}

void ApplicationLoop::CreateCornerWall(float width, float height, float xDiv, float yDiv)
{
	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, width);

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

	int totalDataSize = CompressMeshFromVertexStream(inputDescription, 2, sizeof(GridVertex), vertexCount, box, poses, compPoses, &compressedSize, &vertexFlags);

	auto& rendInst = GlobalRenderer::gRenderInstance;

	int geomDetailsIndex = -1, meshGPUIndex = -1, meshCPUIndex = -1;

	geomDetailsIndex = AllocateGPUGeometryDetails(1);

	meshGPUIndex = AllocateGPUMeshDetails(1);

	meshCPUIndex = AllocateCPUMeshDetails(1);

	int geomRenderableCPUDataIndex = AllocateCPUGeometryInstances(1);

	int gpuGeomRenderable = AllocateGPUGeometryInstances(1);

	int gpuMeshRendrables = AllocateGPUMeshRenderable(3);

	int cpuMeshRenderable = AllocateCPUMeshRenderable(3);

	int materialHandleIndex = AllocateGPUMaterialData(1);

	int materialRangeIndex = AllocateGPUMaterialRanges(1);

	if (geomDetailsIndex < 0 || meshGPUIndex < 0 || meshCPUIndex < 0 || geomRenderableCPUDataIndex < 0 || gpuGeomRenderable < 0 || gpuMeshRendrables < 0 || cpuMeshRenderable < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Cannot create corner object", 27);
		mainAppLogger.ProcessMessage();
		return;
	}

	CreateGPUGeometryDetails(geomDetailsIndex, box);

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * vertexCount, 16);
	int indexAlloc = indexBufferAlloc.Allocate(indexCount * sizeof(uint16_t), 16);

	CreateMeshHandle(meshCPUIndex, meshGPUIndex, compPoses, indices, vertexFlags, vertexCount, compressedSize, 2, indexCount, sphere, vertexAlloc, indexAlloc);

	Matrix4f sideFrontPanel = CreateRotationMatrixMat4(Vector3f(0.0f, 0.0f, 1.0), DegToRad(-90.0f));

	sideFrontPanel.translate = Vector4f(10.0f, 3.0, -5.0, 1.0);

	Matrix4f backPanel = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0), DegToRad(-90.0f));

	backPanel.translate = Vector4f(5.0f, 3.0, -10.0, 1.0);

	Matrix4f floorPanel = { { 1.0, 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0, 0.0 }, { 0.0 , 0.0, 1.0, 0.0 }, { 5.0f, -2.0, -5.0, 1.0 } };

	rendInst.UpdateDriverMemory(compPoses, globalVertexBuffer, vertexCount * compressedSize, vertexAlloc, TransferType::CACHED);
	rendInst.UpdateDriverMemory(indices, globalIndexBuffer, indexCount * 2, indexAlloc, TransferType::CACHED);

	CreateMaterial(materialHandleIndex, VERTEXNORMAL, nullptr, 0, Vector4f(1.0, 0.0, 0.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0));

	CreateMaterialRange(materialRangeIndex, 1, &materialHandleIndex);

	RenderableGeomCPUData* geomRendCPUData = renderablesGeomObjects.Update(geomRenderableCPUDataIndex);

	geomRendCPUData->renderableMeshStart = gpuMeshRendrables;

	geomRendCPUData->renderableMeshCount = 3;

	geomRendCPUData->geomIndex = gpuGeomRenderable;

	CreateGPUGeometryRenderable(geomRenderableCPUDataIndex, gpuGeomRenderable, Identity4f(), geomDetailsIndex, geomRendCPUData->renderableMeshStart, geomRendCPUData->renderableMeshCount);

	CreateRenderable(cpuMeshRenderable, gpuMeshRendrables, floorPanel, gpuGeomRenderable, materialRangeIndex, 1, -1, meshGPUIndex, 1);

	CreateRenderable(cpuMeshRenderable + 1, gpuMeshRendrables + 1, sideFrontPanel, gpuGeomRenderable, materialRangeIndex, 1, -1, meshGPUIndex, 1);
	
	CreateRenderable(cpuMeshRenderable + 2, gpuMeshRendrables +2, backPanel, gpuGeomRenderable, materialRangeIndex, 1, -1, meshGPUIndex, 1);

	mainAppLogger.AddLogMessage(LOGINFO, "Created Corner Object", 21);

	return;
}



void ApplicationLoop::CreateJointVisualObject(int numberOfJoints)
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

	struct MyVertex
	{
		Vector4f pos;
		Vector4f color;
	};


	MyVertex compVerts[24] =
	{
	{ Vector4f(1.0f,  2.0f,  1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,  2.0f, -1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f, -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f, -2.0f, -1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },

	{ Vector4f(-1.0f,  2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(-1.0f,  2.0f, -1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(-1.0f, -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(-1.0f, -2.0f, -1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },

	{ Vector4f(-1.0f,  2.0f,  1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,   2.0f,  1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(-1.0f,  2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,   2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },

	{ Vector4f(-1.0f, -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,  -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(-1.0f, -2.0f, -1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,  -2.0f, -1.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },

	{ Vector4f(-1.0f,  2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(1.0f,   2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(-1.0f, -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	{ Vector4f(1.0f,  -2.0f,  1.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },

	{ Vector4f(-1.0f,  2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,   2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(-1.0f, -2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) },
	{ Vector4f(1.0f,  -2.0f, -1.0f, 1.0f), Vector4f(0.0f, 1.0f, 1.0f, 1.0f) }
	};


	static AxisBox BOX =
	{
		.min = Vector4f(-1.0, -2.0, -1.0, 0.0),
		.max = Vector4f(1.0, 2.0, 1.0, 0.0),
	};

	auto& rendInst = GlobalRenderer::gRenderInstance;

	Matrix4f crateMatrix = Identity4f();

	crateMatrix.translate = Vector4f(-5.0, 5.0, 0.0f, 1.0f);

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, 2.5f);

	int geomDetailsIndex = -1, meshGPUIndex = -1, meshCPUIndex = -1;

	int vertexAlloc = vertexBufferAlloc.Allocate(sizeof(MyVertex) * 24, 64);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	static Matrix4f matrix = Identity4f();

	matrix.translate = Vector4f(-10.0, 0.0, 0.0, 1.0f);

	int camJointData = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(19, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	int JointDesc = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(19, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(nullptr, camJointData, &globalBufferLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(nullptr, JointDesc, &jointMeshWorldMatrix, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(nullptr, JointDesc, &jointMeshParentIndices, 0, 1, 1);

	std::array<int, 2> Descs = { camJointData, JointDesc };

	GraphicsIntermediaryPipelineInfo jointInfo = {
		.drawType = 0,
		.vertexBufferHandle = globalVertexBuffer,
		.vertexCount = 24,
		.pipelinename = 19,
		.descCount = 2,
		.descriptorsetid = Descs.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexCount = 36,
		.instanceCount = (uint32_t)numberOfJoints,
		.indexSize = 2,
		.indexOffset = (uint32_t)indexAlloc,
		.vertexOffset = (uint32_t)vertexAlloc,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	jointMeshPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&jointInfo, false);

	rendInst.UpdateDriverMemory(compVerts, globalVertexBuffer, sizeof(compVerts), vertexAlloc, TransferType::CACHED);
	rendInst.UpdateDriverMemory(BoxIndices, globalIndexBuffer, sizeof(BoxIndices), indexAlloc, TransferType::CACHED);

	mainAppLogger.AddLogMessage(LOGINFO, "Created Joint Object", 20);
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

	auto& rendInst = GlobalRenderer::gRenderInstance;

	Matrix4f crateMatrix = Identity4f();

	crateMatrix.translate = Vector4f(-10.0, 5.0, 0.0f, 1.0f);

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, 1.5f);

	int geomDetailsIndex = -1, meshGPUIndex = -1, meshCPUIndex = -1;

	geomDetailsIndex = AllocateGPUGeometryDetails(1);

	meshGPUIndex = AllocateGPUMeshDetails(1);

	meshCPUIndex = AllocateCPUMeshDetails(1);

	int cpuGeomRenderable = AllocateCPUGeometryInstances(1);

	int gpuGeomRenderable = AllocateGPUGeometryInstances(1);

	int gpuMeshRenderable = AllocateGPUMeshRenderable(1);

	int cpuMeshRenderable = AllocateCPUMeshRenderable(1);

	int materialHandleIndex = AllocateGPUMaterialData(1);

	int materialRangeIndex = AllocateGPUMaterialRanges(1);

	int blendDetailsIndex = AllocateGPUBlendDescriptions(2);

	int blendRangesIndex = AllocateGPUBlendRanges(2);

	if (geomDetailsIndex < 0 || meshGPUIndex < 0 || meshCPUIndex < 0 || cpuGeomRenderable < 0 || gpuGeomRenderable < 0 || gpuMeshRenderable < 0 || cpuMeshRenderable < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Cannot create crate object", 26);
		mainAppLogger.ProcessMessage();
		return;
	}

	StringView blendname = AppInstanceTempAllocator.AllocateFromNullStringCopy("blendmap.bmp");

	StringView skyname = AppInstanceTempAllocator.AllocateFromNullStringCopy("sky.bmp");

	StringView normalmapname = AppInstanceTempAllocator.AllocateFromNullStringCopy("WNN2.bmp");

	StringView albedomapname = AppInstanceTempAllocator.AllocateFromNullStringCopy("WNN.bmp");

	int alebdoMapped = Read2DImage(&albedomapname, 1, BMP);
	int normalMapped = Read2DImage(&normalmapname, 1, BMP);
	int blendMapped = Read2DImage(&blendname, 1, BMP);
	int skymapped = Read2DImage(&skyname, 1, BMP);

	CreateBlendDetails(blendDetailsIndex, BlendMaterialType::ConstantAlpha, 1.0f);
	CreateBlendDetails(blendDetailsIndex + 1, BlendMaterialType::BlendMap, blendMapped);

	std::array blendIndices = { blendDetailsIndex, blendDetailsIndex + 1 };

	CreateBlendRange(blendRangesIndex, blendIndices.data(), 2);

	std::array textureHandles = { alebdoMapped, normalMapped, skymapped };

	CreateMaterial(materialHandleIndex, ALBEDOMAPPED | TANGENTNORMALMAPPED, textureHandles.data(), 2, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0));

	CreateMaterial(materialHandleIndex + 1, ALBEDOMAPPED, textureHandles.data() + 2, 1, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.80, .80, .80, 1.0), 256.0, Vector4f(.04, .06, 0.08, 1.0));

	std::array<int, 2> materialIDs = { materialHandleIndex, materialHandleIndex + 1 };

	int materialRangeCount = 2;
	int materialRangeStart = CreateMaterialRange(materialRangeIndex, materialRangeCount, materialIDs.data());

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * 24, 16);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	CreateGPUGeometryDetails(geomDetailsIndex, BOX);
	
	CreateMeshHandle(meshCPUIndex, meshGPUIndex, compVerts, BoxIndices, vertexFlags, 24, compressedSize, 2, 52, sphere, vertexAlloc, indexAlloc);

	RenderableGeomCPUData* rendGeomCPUData = renderablesGeomObjects.Update(cpuGeomRenderable);

	rendGeomCPUData->renderableMeshStart = gpuMeshRenderable;

	rendGeomCPUData->renderableMeshCount = 1;

	rendGeomCPUData->geomIndex = gpuGeomRenderable;

	CreateGPUGeometryRenderable(cpuGeomRenderable, gpuGeomRenderable, crateMatrix, geomDetailsIndex, rendGeomCPUData->renderableMeshStart, rendGeomCPUData->renderableMeshCount);

	CreateRenderable(cpuMeshRenderable, gpuMeshRenderable, Identity4f(), gpuGeomRenderable, materialRangeStart, materialRangeCount, blendRangesIndex, meshGPUIndex, 1);
	
	rendInst.UpdateDriverMemory(compressedVertexData, globalVertexBuffer, compressedSize * 24,  vertexAlloc, TransferType::CACHED);
	rendInst.UpdateDriverMemory(BoxIndices, globalIndexBuffer,  sizeof(BoxIndices),  indexAlloc, TransferType::CACHED);

	mainAppLogger.AddLogMessage(LOGINFO, "Created Crate Object", 20);
}

void ApplicationLoop::SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile* file, int* textureHandles, int textureBase)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;

	uint32_t frames = rendInst.MAX_FRAMES_IN_FLIGHT;

	int meshCount = geoDef->numRenderables;

	int geomDetailsIndex = -1, meshGPUIndex = -1, meshCPUIndex = -1;

	geomDetailsIndex = AllocateGPUGeometryDetails(1);

	meshGPUIndex = AllocateGPUMeshDetails(meshCount);

	meshCPUIndex = AllocateCPUMeshDetails(meshCount);

	int cpuGeomRenderable = AllocateCPUGeometryInstances(1);

	int gpuGeomRenderable = AllocateGPUGeometryInstances(1);

	int gpuMeshRenderable = AllocateGPUMeshRenderable(meshCount);

	int cpuMeshRenderable = AllocateCPUMeshRenderable(meshCount);

	int materialHandleIndex = AllocateGPUMaterialData(meshCount);

	int materialRangeIndex = AllocateGPUMaterialRanges(meshCount);

	int blendDetailsIndex = AllocateGPUBlendDescriptions(1);

	int blendRangesIndex = AllocateGPUBlendRanges(1);

	if (geomDetailsIndex < 0 || meshGPUIndex < 0 || meshCPUIndex < 0 || cpuGeomRenderable < 0 || gpuGeomRenderable < 0 || gpuMeshRenderable < 0 || cpuMeshRenderable < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Cannot smb object", 17);
		mainAppLogger.ProcessMessage();
		return;
	}

	CreateBlendDetails(blendDetailsIndex, BlendMaterialType::ConstantAlpha, 1.0f);

	CreateBlendRange(blendRangesIndex, &blendDetailsIndex, 1);

	CreateGPUGeometryDetails(geomDetailsIndex, geoDef->axialBox);

	RenderableGeomCPUData* rendGeomCPUData = renderablesGeomObjects.Update(cpuGeomRenderable);

	rendGeomCPUData->renderableMeshStart = gpuMeshRenderable;
	
	rendGeomCPUData->renderableMeshCount = meshCount;

	rendGeomCPUData->geomIndex = gpuGeomRenderable;

	int base = 0;

	Matrix4f geomSpecificData;

	geomSpecificData = Identity4f();

	geomSpecificData.translate = Vector4f(0.f, 0.f, 0.f, 1.0f);

	Matrix4f rotation = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0f), DegToRad(90.0f));

	geomSpecificData = geomSpecificData * rotation;

	CreateGPUGeometryRenderable(cpuGeomRenderable, gpuGeomRenderable, geomSpecificData, geomDetailsIndex, gpuMeshRenderable, meshCount);

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

		uint16_t* indices = (uint16_t*)vertexAndIndicesAlloc.Allocate(sizeof(uint16_t) * indexCount);

		SMBCopyIndices(geoDef, i, file, indices);

		int vertexAlloc = vertexBufferAlloc.Allocate(vertexSize * vertexCount, 16);

		int indexAlloc = indexBufferAlloc.Allocate(sizeof(uint16_t) * indexCount, 1);
		
		rendInst.UpdateDriverMemory(indices, globalIndexBuffer, sizeof(uint16_t) * indexCount,  indexAlloc, TransferType::MEMORY);

		rendInst.UpdateDriverMemory(vertexData, globalVertexBuffer, vertexSize * vertexCount,  vertexAlloc, TransferType::MEMORY);
		
		int textureStart = meshTextureHandles.AllocateN(geoDef->materialsCount[i]);
		int textureCount = geoDef->materialsCount[i];

		for (int j = 0; j < textureCount; j++)
		{
			meshTextureHandles.Update(textureStart + j, textureHandles[base+j] + textureBase);
		}

		base += textureCount;

		CreateMaterial(
			materialHandleIndex + i,
			ALBEDOMAPPED | ((textureCount > 1) ? WORLDNORMALMAPPED : type == PosPack6_C16Tex1_Bone2 ? 0 : VERTEXNORMAL), 
			&meshTextureHandles.dataArray[textureStart], 
			textureCount, 
			Vector4f(1.0, 1.0, 1.0, 1.0), 
			Vector4f(.1, .1, .1, 1.0), 1.5, 
			Vector4f(0.0, 0.0, 0.0, 1.0)
		);

		int outMaterialHandle = materialHandleIndex + i;
		int materialRangeCount = 1;
		
		CreateMaterialRange(materialRangeIndex + i, materialRangeCount, &outMaterialHandle);
		
		CreateMeshHandle(meshCPUIndex + i, meshGPUIndex + i, vertexData, indices,
			vertexFlags, vertexCount, vertexSize, 2, 
			indexCount,
			geoDef->spheres[i], 
			vertexAlloc, indexAlloc
		);

		CreateRenderable(cpuMeshRenderable + i, gpuMeshRenderable + i, Identity4f(), gpuGeomRenderable, materialRangeIndex + i, materialRangeCount, blendRangesIndex, meshGPUIndex + i, 1);
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
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many debug structs created"));
		return -1;
	}

	int debugStructLocation = globalDebugStructCount++;

	drawStruct.center = Vector4f(center.x, center.y, center.z, 1.0f);

	drawStruct.scale = scale;
	drawStruct.color = color;

	drawStruct.halfExtents = Vector4f(r, std::bit_cast<float, uint32_t>(count), 1.0, 1.0);


	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&type,  globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

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
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many debug structs created"));
		return -1;
	}

	int debugStructLocation = globalDebugStructCount++;
	
	drawStruct.center = Vector4f(center.x , center.y, center.z, 1.0f);
	drawStruct.scale = scale;
	drawStruct.color = color;
	drawStruct.halfExtents = halfExtents;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&type, globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

	return debugStructLocation;
}

void ApplicationLoop::ProcessSMBFile(SMBFile *file)
{
	const int MAX_GEO_FILES = 2;
	const int MAX_TEXTURES = 10;

	int totalTextureCount = 0, totalMeshCount = 0;
	auto& chunk = file->chunks;

	SMBTexture* textures = (SMBTexture*)AppInstanceTempAllocator.CAllocate(sizeof(SMBTexture) * MAX_TEXTURES);

	SMBGeoChunk* geoDefs = (SMBGeoChunk*)AppInstanceTempAllocator.Allocate(sizeof(SMBGeoChunk) * MAX_GEO_FILES);

	for (size_t i = 0; i<file->numResources; i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{
			SMBGeoChunk* geoDef = &geoDefs[totalMeshCount];

			size_t seekpos = chunk[i].offsetInHeader;

			OSSeekFile(&file->fileHandle, seekpos, BEGIN);

			char* geomHeader = (char*)GlobalInputScratchAllocator.Allocate(chunk[i].headerSize);

			OSReadFile(&file->fileHandle, chunk[i].headerSize, geomHeader);

		    int geometryPorcessRet = ProcessGeometryClass(geomHeader, totalTextureCount, geoDef, chunk[i].contigOffset + file->fileOffset, chunk[i].fileOffset + file->numContiguousBytes + file->fileOffset, &mainAppLogger, &GlobalInputScratchAllocator);

			if (geometryPorcessRet < 0)
			{
				mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Geometry failed from SMB failed loading"));
				break;
			}

			totalMeshCount++;

			break;
		}
		case TEXTURE:
		{
			int texErrorRet = textures[totalTextureCount].CreateTextureDetails(file, chunk[i]);	
			if (texErrorRet < 0)
			{
				mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Texture from SMB failed loading"));
				break;
			}
			totalTextureCount++;
			break;
		}
		case GR2:
			mainAppLogger.AddLogMessage(LogMessageType::LOGINFO, AppInstanceTempAllocator.AllocateFromNullStringCopy("Unhandled SMB Chunk"));
			break;
		case Joints:
		{
			char* jointStrData = GetJointNames(&GlobalInputScratchAllocator, file, &chunk[i]);

			SMBSkeleton skel{};

			int skelCode = GetBoneData(&GlobalInputScratchAllocator, &skel, file);

			int startingJointLocation = jointMeshWorldMatrixCount;

			Matrix4f* worldMats = (Matrix4f*)GlobalInputScratchAllocator.Allocate(sizeof(Matrix4f) * skel.jointCount);
			
			for (int joint = 0; joint < skel.jointCount; joint++)
			{
				Matrix4f jointRot = CreateRotationMatFromQuat(skel.joints[joint].granny_orientation);
				
				Matrix4f scale = CreateScaleMatrix(skel.joints[joint].granny_scale);

				Matrix4f translation = Identity4f();

				translation.translate.x = skel.joints[joint].granny_position.x;
				translation.translate.y = skel.joints[joint].granny_position.y;
				translation.translate.z = skel.joints[joint].granny_position.z;

				uint32_t parentIdx = skel.joints[joint].granny_parentIndex;

				Matrix4f worldMat;

				if (parentIdx == 0xFFFFFFFF) {
					worldMat = translation * jointRot * scale; // root joint
				}
				else {
					worldMat =  worldMats[parentIdx] * translation * jointRot * scale;
				}

				worldMats[joint] = worldMat;

				GlobalRenderer::gRenderInstance.UpdateDriverMemory(&worldMat, jointMeshWorldMatrix, sizeof(Matrix4f), (startingJointLocation + joint) * sizeof(Matrix4f), TransferType::CACHED);
				GlobalRenderer::gRenderInstance.UpdateDriverMemory(&skel.joints[joint].granny_parentIndex, jointMeshParentIndices, sizeof(uint32_t), (startingJointLocation + joint) * sizeof(uint32_t), TransferType::CACHED);
			}


			jointMeshWorldMatrixCount += skel.jointCount;

			CreateJointVisualObject(skel.jointCount);

			break;
		}
		default:
			mainAppLogger.AddLogMessage(LogMessageType::LOGINFO, AppInstanceTempAllocator.AllocateFromNullStringCopy("Unhandled SMB Chunk"));
			break;
		}
	}

	int globalTextureStartIndex = 0;

	if (totalTextureCount)
	{
		TextureDetails** details = (TextureDetails**)AppInstanceTempAllocator.Allocate(sizeof(TextureDetails*) * totalTextureCount);

		globalTextureStartIndex = mainDictionary.AllocateNTextureHandles(totalTextureCount, details);

		for (int ii = 0; ii < totalTextureCount; ii++)
		{
			SMBTexture& texture = textures[ii];

			ImageFormat format = ConvertSMBImageToAppImage(texture.type);

			texture.data = (char*)mainDictionary.AllocateImageCache(texture.cumulativeSize);

			texture.ReadTextureData(file);

			mainDictionary.textureHandles[ii + globalTextureStartIndex] =
				GlobalRenderer::gRenderInstance.CreateImageHandle(
					texture.cumulativeSize,
					texture.width,
					texture.height,
					texture.miplevels,
					format,
					GetPoolIndexByFormat(format),
					mainLinearSampler
				);

			GlobalRenderer::gRenderInstance.UpdateImageMemory(
				texture.data,
				mainDictionary.textureHandles[ii + globalTextureStartIndex],
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
		update.resourceDstBegin = globalTextureStartIndex;
		update.resourceHandles = mainDictionary.textureHandles.data() + globalTextureStartIndex;

		GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);
	}

	for (int i = 0; i < totalMeshCount; i++) 
	{
		SMBGeoChunk* geoDef = &geoDefs[i];

		int* flattenMatHandles = (int*)AppInstanceTempAllocator.Allocate(sizeof(int) * geoDef->numMaterials);

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
						flattenMatHandles[hh + base] = gg;
						break;
					}
				}

				if (gg == totalTextureCount)
				{
					flattenMatHandles[hh + base] = 0;
					mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, "Unhandled Mesh Material Image", 20);
				}
			}
			base += count;
		}

		SMBGeometricalObject(geoDef, file, flattenMatHandles, globalTextureStartIndex);
	}

	mainAppLogger.ProcessMessage();
}

int CreateDebugCommandBuffers(int count)
{
	debugIndirectDrawData.commandBufferCount = 0;
	debugIndirectDrawData.commandBufferSize = count;


	ShaderResourceSetContext debugRSContext{ &mainAppLogger, false };

	debugIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndirectCommand),  debugIndirectDrawData.commandBufferSize, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	debugIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	debugIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);


	debugIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(DEBUGCULL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferAlloc, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &globalDebugTypesAlloc, 0, 1, 2);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &globalBufferLocation, nullptr,  0, 1, 3);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &globalDebugStructAlloc, nullptr, 0, 1, 4);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCountAlloc, nullptr, 0, 1, 5);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, &debugIndirectDrawData.commandBufferCount, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&debugRSContext, debugIndirectDrawData.indirectCullDescriptor, 5, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	if (debugRSContext.contextFailed)
	{
		debugRSContext.contextLogger->ProcessMessage();
		return -1;
	}


	std::array debugCullDescriptors = { debugIndirectDrawData.indirectCullDescriptor };

	ShaderComputeLayout* debugCullDescriptorLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(DEBUGCULL);

	ComputeIntermediaryPipelineInfo debugCullPipelineCreate = {
			.x = 4096 / debugCullDescriptorLayout->x,
			.y = 1,
			.z = 1,
			.pipelinename = DEBUGCULL,
			.descCount = 1,
			.descriptorsetid = debugCullDescriptors.data()
	};


	debugIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&debugCullPipelineCreate);

	if (debugIndirectDrawData.indirectCullPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Indirect cull pipeline failed creation", 38);
		return -1;
	}

	debugIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(DEBUGDRAW, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&debugRSContext, debugIndirectDrawData.indirectDrawDescriptor, &globalDebugStructAlloc, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&debugRSContext, debugIndirectDrawData.indirectDrawDescriptor, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&debugRSContext, debugIndirectDrawData.indirectDrawDescriptor, &globalDebugTypesAlloc, 0, 1, 2);

	std::array<int, 2> indirectDebugDrawDescriptors = {
		globalBufferDescriptor,
		debugIndirectDrawData.indirectDrawDescriptor,
	};

	if (debugRSContext.contextFailed)
	{
		debugRSContext.contextLogger->ProcessMessage();
		return -1;
	}

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


	debugIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&indirectDebugDrawPipelineCreate, false);

	if (debugIndirectDrawData.indirectDrawPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "indirect draw pipeline failed creation", 38);
		return -1;
	}

	return 0;
}

int CreateGenericMeshCommandBuffers(int count)
{
	mainIndirectDrawData.commandBufferSize = count;
	mainIndirectDrawData.commandBufferCount = 0;

	ShaderResourceSetContext genericMeshRSContext{ &mainAppLogger, false };

	mainIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), count, alignof(VkDrawIndexedIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	mainIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	mainIndirectDrawData.indirectCullDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(RENDEROBJCULL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	mainIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);


	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &globalBufferLocation, nullptr, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCountAlloc, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &globalRenderableLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 7);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, 5, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshRSContext, mainIndirectDrawData.indirectCullDescriptor, 4, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);



	mainIndirectDrawData.indirectDrawDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(GENERIC, 2, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	


	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalMeshLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalVertexBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalLightBuffer, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalLightTypesBuffer,  0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalMaterialsLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalRenderableLocation, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalMaterialIndicesLocation, 0, 1, 7);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalBlendDetailsLocation, nullptr, 0, 1, 8);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalBlendRangesLocation, 0, 1, 9);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 10);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 11);

	std::array<int, 3> indirectDrawDescriptors = {
		globalBufferDescriptor,
		globalTexturesDescriptor,
		mainIndirectDrawData.indirectDrawDescriptor,
	};

	shadowMapIndex = mainDictionary.AllocateNTextureHandles(GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, nullptr);

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, globalTexturesDescriptor, 3, shadowMapIndex);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &shadowMapIndex, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, mainIndirectDrawData.indirectDrawDescriptor, &GlobalRenderer::gRenderInstance.currentFrame, 1);

	if (genericMeshRSContext.contextFailed)
	{
		genericMeshRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	GraphicsIntermediaryPipelineInfo indirectDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = GENERIC,
		.descCount = 3,
		.descriptorsetid = indirectDrawDescriptors.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexSize = 2,
		.indexOffset = 0,
		.vertexOffset = 0,
		.indirectAllocation = mainIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = mainIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = mainIndirectDrawData.commandBufferCountAlloc
	};

	mainIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&indirectDrawCreate, false);

	if (mainIndirectDrawData.indirectDrawPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "indirect draw pipeline failed creation", 39);
		return -1;
	}

	int cullLightDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(RENDEROBJCULL, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);


	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, cullLightDescriptor, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, cullLightDescriptor, &lightAssignment.deviceOffsetsAlloc, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, cullLightDescriptor, &lightAssignment.deviceCountsAlloc, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, cullLightDescriptor, &globalLightBuffer, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, cullLightDescriptor, &globalLightTypesBuffer, 0, 1, 4 );

	if (genericMeshRSContext.contextFailed)
	{
		genericMeshRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ShaderComputeLayout* layout = GlobalRenderer::gRenderInstance.GetComputeLayout(RENDEROBJCULL);

	std::array computeDescriptors = { mainIndirectDrawData.indirectCullDescriptor, cullLightDescriptor };

	ComputeIntermediaryPipelineInfo mainCullComputeSetup = {
			.x = 4096 / layout->x,
			.y = 1,
			.z = 1,
			.pipelinename = RENDEROBJCULL,
			.descCount = 2,
			.descriptorsetid = computeDescriptors.data()
	};

	mainIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&mainCullComputeSetup);

	int outlineDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(OUTLINE, 2, 3);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, outlineDescriptor, &globalMeshLocation, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, outlineDescriptor, &globalVertexBuffer, nullptr, 0, 1, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshRSContext, outlineDescriptor, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, outlineDescriptor, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, outlineDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshRSContext, outlineDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 5);

	static Vector4f black = { 0.0, 0.0, 0.0, 1.0 };

	static float outlineLength = 1.025;

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, outlineDescriptor, &black, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshRSContext, outlineDescriptor, &outlineLength, 1);

	std::array<int, 2> indirectOutline = { outlineDescriptor };

	if (genericMeshRSContext.contextFailed)
	{
		genericMeshRSContext.contextLogger->ProcessMessage();
		return -1;
	}

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

	//int outlinePipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&outlineDrawCreate, true);

	/*
	
	if (outlinePipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Outline pipeline failed creation", 39);
		return -1;
	}
	
	*/

	return 0;
}

int CreateMeshWorldAssignment(int count)
{
	ShaderResourceSetContext genericMeshWorldRSContext{ &mainAppLogger, false };

	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(PREFIXSUM);

	worldSpaceAssignment.totalElementsCount = count;
	worldSpaceAssignment.totalSumsNeeded = (int)floor(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)prefixLayout->x);

	worldSpaceAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	worldSpaceAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	worldSpaceAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	if (worldSpaceAssignment.totalSumsNeeded)
	{
		worldSpaceAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
		
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else 
	{
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, nullptr, nullptr, 0, 0, 2);
	}


	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 1);
	

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.prefixSumDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

	std::array prefixSumDescriptor = { worldSpaceAssignment.prefixSumDescriptors };

	if (genericMeshWorldRSContext.contextFailed)
	{
		genericMeshWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = PREFIXSUM,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};



	worldSpaceAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);

	if (worldSpaceAssignment.prefixSumPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "World Space sum after failed creation", 39);
		return -1;
	}
	
	if (worldSpaceAssignment.totalSumsNeeded)
	{

		worldSpaceAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);
		GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, &worldSpaceAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { worldSpaceAssignment.sumAfterDescriptors, };

		if (genericMeshWorldRSContext.contextFailed)
		{
			genericMeshWorldRSContext.contextLogger->ProcessMessage();
			return -1;
		}

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXSUM,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		worldSpaceAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);

		if (worldSpaceAssignment.sumAfterPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, "World Space sum after failed creation", 39);
			return -1;
		}

		worldSpaceAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXADD, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.sumAppliedToBinDescriptors, &worldSpaceAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array incrementSumsDescriptor = { worldSpaceAssignment.sumAppliedToBinDescriptors, };

		if (genericMeshWorldRSContext.contextFailed)
		{
			genericMeshWorldRSContext.contextLogger->ProcessMessage();
			return -1;
		}

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)worldSpaceAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXADD,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};

		worldSpaceAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);

		if (worldSpaceAssignment.sumAppliedToBinPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, "World Space sum applied failed creation", 39);
			return -1;
		}
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(WORLDORGANIZE);

	uint32_t assignmentGroupCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)assignmentLayout->x);

	worldSpaceAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	worldSpaceAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(WORLDORGANIZE, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &worldSpaceAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalRenderableLocation, nullptr, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { worldSpaceAssignment.preWorldSpaceDivisionDescriptor, };

	if (genericMeshWorldRSContext.contextFailed)
	{
		genericMeshWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = WORLDORGANIZE,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	worldSpaceAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);

	if (worldSpaceAssignment.preWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "World Space pre division failed creation", 40);
		return -1;
	}


	worldSpaceAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(WORLDASSIGN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &worldSpaceAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &worldSpaceAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &globalGeometryRenderableLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, &mainIndirectDrawData.commandBufferCount, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMeshWorldRSContext, worldSpaceAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { worldSpaceAssignment.postWorldSpaceDivisionDescriptor, };

	if (genericMeshWorldRSContext.contextFailed)
	{
		genericMeshWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = WORLDASSIGN,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	worldSpaceAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);

	if (worldSpaceAssignment.postWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "World Space assign failed creation", 34);
		return -1;
	}


	return 0;
}

int CreateLightAssignments(int count)
{
	ShaderResourceSetContext genericLightWorldRSContext{ &mainAppLogger, false };

	ShaderComputeLayout* prefixLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(PREFIXSUM);

	lightAssignment.totalElementsCount = count;
	lightAssignment.totalSumsNeeded = (int)floor(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	uint32_t prefixCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)prefixLayout->x);

	lightAssignment.prefixSumDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	lightAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	lightAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, &lightAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, &lightAssignment.totalElementsCount, 0);

	if (lightAssignment.totalSumsNeeded)
	{
		lightAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	}
	else
	{
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.prefixSumDescriptors, nullptr, nullptr, 0, 0, 2);
	}

	std::array prefixSumDescriptor = { lightAssignment.prefixSumDescriptors };

	if (genericLightWorldRSContext.contextFailed)
	{
		genericLightWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo worldAssignmentPrefix = {
			.x = prefixCount,
			.y = 1,
			.z = 1,
			.pipelinename = PREFIXSUM,
			.descCount = 1,
			.descriptorsetid = prefixSumDescriptor.data()
	};

	lightAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&worldAssignmentPrefix);

	if (lightAssignment.prefixSumPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Prefix Sum After failed creation", 32);
		return -1;
	}

	if (lightAssignment.totalSumsNeeded)
	{

		lightAssignment.sumAfterDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 2);
		GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, &lightAssignment.totalSumsNeeded, 0);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, 1, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.sumAfterDescriptors, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array prefixSumOverflowDescriptor = { lightAssignment.sumAfterDescriptors, };

		ComputeIntermediaryPipelineInfo prefixSumComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXSUM,
				.descCount = 1,
				.descriptorsetid = prefixSumOverflowDescriptor.data()
		};

		if (genericLightWorldRSContext.contextFailed)
		{
			genericLightWorldRSContext.contextLogger->ProcessMessage();
			return -1;
		}

		lightAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&prefixSumComputePipeline);

		if (lightAssignment.sumAfterPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, "World Sum After failed creation", 31);
			return -1;
		}

		lightAssignment.sumAppliedToBinDescriptors = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(PREFIXADD, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
		GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.deviceSumsAlloc, nullptr, 0, 1, 1);
		GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.sumAppliedToBinDescriptors, &lightAssignment.totalElementsCount, 0);

		GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.sumAppliedToBinDescriptors, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

		std::array incrementSumsDescriptor = { lightAssignment.sumAppliedToBinDescriptors, };

		if (genericLightWorldRSContext.contextFailed)
		{
			genericLightWorldRSContext.contextLogger->ProcessMessage();
			return -1;
		}

		ComputeIntermediaryPipelineInfo incrementSumsComputePipeline = {
				.x = (uint32_t)lightAssignment.totalSumsNeeded,
				.y = 1,
				.z = 1,
				.pipelinename = PREFIXADD,
				.descCount = 1,
				.descriptorsetid = incrementSumsDescriptor.data()
		};


		lightAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&incrementSumsComputePipeline);

		if (lightAssignment.sumAppliedToBinPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, "World Sum failed creation", 25);
			return -1;
		}
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(LIGHTASSIGN);

	uint32_t assignmentGroupCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)assignmentLayout->x);

	lightAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	lightAssignment.preWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(LIGHTORGANIZE, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, &lightAssignment.deviceCountsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightBuffer, nullptr, 0, 1, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightTypesBuffer, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, &globalLightCount, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.preWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array preWorldDivDescriptor = { lightAssignment.preWorldSpaceDivisionDescriptor, };

	if (genericLightWorldRSContext.contextFailed)
	{
		genericLightWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTORGANIZE,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	lightAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&preWorldDivComputePipeline);

	if (lightAssignment.preWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Pre World Division Pipeline failed creation", 43);
		return -1;
	}

	lightAssignment.postWorldSpaceDivisionDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(LIGHTASSIGN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &lightAssignment.deviceOffsetsAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightTypesBuffer, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &mainGrid, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, &globalLightCount, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, 0, COMPUTE_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericLightWorldRSContext, lightAssignment.postWorldSpaceDivisionDescriptor, 2, COMPUTE_BARRIER, READ_SHADER_RESOURCE);

	std::array postWorldDivDescriptor = { lightAssignment.postWorldSpaceDivisionDescriptor, };

	if (genericLightWorldRSContext.contextFailed)
	{
		genericLightWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTASSIGN,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	lightAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&postWorldDivComputePipeline);

	if (lightAssignment.postWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Post World Division failed creation", 35);
		return -1;
	}

	return 0;
}

int CreateShadowMapManager(int maxShadowMapAssignment, int maxObjCount, int shadowMapHeight, int shadowMapWidth, int shadowMapAtlasHeight, int shadowMapAtlasWidth)
{
	ShaderResourceSetContext genericMainShadowMapRSContext{ &mainAppLogger, false };

	mainShadowMapManager.atlasHeight = shadowMapAtlasHeight;
	mainShadowMapManager.atlasWidth = shadowMapAtlasWidth;
	mainShadowMapManager.avgShadowHeight = shadowMapHeight;
	mainShadowMapManager.avgShadowWidth = shadowMapWidth;
	mainShadowMapManager.frameGraphIndex = MSAAShadowMapping;
	mainShadowMapManager.resourceIndex = 1;
	mainShadowMapManager.totalShadowMaps = (int)((float)(shadowMapAtlasHeight * shadowMapAtlasWidth) / (float)(shadowMapWidth * shadowMapHeight));
	mainShadowMapManager.zoneAlloc = 0;

	

	mainShadowMapManager.shadowMapCountsAllocSize =  maxObjCount;
	mainShadowMapManager.shadowMapCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapCountsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapOffsetsAllocSize =  maxObjCount;
	mainShadowMapManager.shadowMapOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapOffsetsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapAssignmentsAllocSize = maxShadowMapAssignment * maxObjCount;
	mainShadowMapManager.shadowMapAssignmentsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapAssignmentsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	mainShadowMapManager.shadowMapObjectIDsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapObjectIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapObjectIDsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapObjectCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapIndirectBufferAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapIndirectBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), mainShadowMapManager.shadowMapIndirectBufferAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT); ;
	

	mainShadowMapManager.shadowMapViewProjAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapViewProjAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(Matrix4f) * 2, mainShadowMapManager.shadowMapViewProjAllocSize, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	mainShadowMapManager.shadowMapAtlasViewsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapAtlasViewsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(ShadowMapView), mainShadowMapManager.shadowMapAtlasViewsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT);


	BufferArrayUpdate shadowViewProjUp{};
	BufferArrayUpdate shadowAtlasViews{};

	shadowViewProjUp.allocationCount = 1;
	shadowViewProjUp.resourceDstBegin = 0;
	shadowViewProjUp.allocationIndices = &mainShadowMapManager.shadowMapViewProjAlloc;
	
	shadowAtlasViews.allocationCount = 1;
	shadowAtlasViews.resourceDstBegin = 0;
	shadowAtlasViews.allocationIndices = &mainShadowMapManager.shadowMapAtlasViewsAlloc;


	GlobalRenderer::gRenderInstance.UpdateBufferResourceArray(globalTexturesDescriptor, 1, ShaderResourceType::STORAGE_BUFFER, &shadowViewProjUp);
	GlobalRenderer::gRenderInstance.UpdateBufferResourceArray(globalTexturesDescriptor, 2, ShaderResourceType::UNIFORM_BUFFER, &shadowAtlasViews);

	
	mainShadowMapManager.shadowClippingDescriptor1 = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(18, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	mainShadowMapManager.shadowClippingDescriptor2 = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(18, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapIndirectBufferAlloc, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &globalMeshLocation, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &mainShadowMapManager.shadowMapObjectCountAlloc, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &globalRenderableLocation, nullptr, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 5);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &globalGeometryRenderableLocation, nullptr, 0, 1, 6);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, &globalLightBuffer, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapCountsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, &globalLightTypesBuffer, 0, 1, 4);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &mainIndirectDrawData.commandBufferCount, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, &globalLightCount, 1);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, 0, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, 2, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor1, 3, INDIRECT_DRAW_BARRIER, READ_INDIRECT_COMMAND);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, 1, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, 2, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBarrier(&genericMainShadowMapRSContext, mainShadowMapManager.shadowClippingDescriptor2, 3, VERTEX_SHADER_BARRIER, READ_SHADER_RESOURCE);

	std::array shadowClipDesc = { mainShadowMapManager.shadowClippingDescriptor1, mainShadowMapManager.shadowClippingDescriptor2 };

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo shadowClipPipelineInfo = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = 18,
			.descCount = 2,
			.descriptorsetid = shadowClipDesc.data()
	};
	
	mainShadowMapManager.shadowClippingPipeline = GlobalRenderer::gRenderInstance.CreateComputeVulkanPipelineObject(&shadowClipPipelineInfo);

	if (mainShadowMapManager.shadowClippingPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Shadow Clipping Pipeline failed creation", 40);
		return -1;
	}

	smdpd.shadowMapDescriptorSet = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(17, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &globalMeshLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &globalVertexBuffer, nullptr, 0, 1, 1);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &globalRenderableLocation, nullptr, 0, 1, 3);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 4);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferView(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 5);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapViewProjAlloc, nullptr, 0, 1, 6);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &mainShadowMapManager.shadowMapAtlasViewsAlloc, nullptr, 0, 1, 7);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &globalGeometryDescriptionsLocation, nullptr, 0, 1, 8);
	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericMainShadowMapRSContext, smdpd.shadowMapDescriptorSet, &globalGeometryRenderableLocation, nullptr, 0, 1, 9);

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}
	
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


	smdpd.shadowMapPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&indirectShadowDrawCreate, false);

	if (smdpd.shadowMapPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Shadow Map Pipeline failed creation", 35);
		return -1;
	}
	
	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;

	smdpd.fullScreenDescriptorSet = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(16, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, smdpd.fullScreenDescriptorSet, 0, 0);
	GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(smdpd.fullScreenDescriptorSet, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMainShadowMapRSContext, smdpd.fullScreenDescriptorSet, &GlobalRenderer::gRenderInstance.currentFrame, 0);

	std::array<int, 1> fullScreenDesc = { smdpd.fullScreenDescriptorSet };

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}

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

	smdpd.fullScreenPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&fullscreenInfo, false);

	if (smdpd.fullScreenPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Shadow Full Screen Pipeline failed creation", 43);
		return -1;
	}

	return 0;
}

void ApplicationLoop::RecreateFrameGraphAttachments(uint32_t width, uint32_t height)
{
	if (BasicShadow >= 0)
	{
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, BasicShadow, 0, nullptr);
	}

	if (MSAAPost >= 0)
	{
		GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(MSAAPost, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, width, height, nullptr);
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, MSAAPost, 1, nullptr);
	}
	
	if (MSAAShadowMapping >= 0)
	{
		GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(MSAAShadowMapping, 0, 3, 4096, 4096, nullptr);
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, MSAAShadowMapping, 1, nullptr);
		GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, globalTexturesDescriptor, 3, shadowMapIndex);
	}
}

int ApplicationLoop::CreateMSAAPostFullScreen()
{
	ShaderResourceSetContext genericMSAARSContext{ &mainAppLogger, false };

	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;

	int mainFullScreen = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(16, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAPost, 1, mainFullScreen, 0, 0);
	GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(mainFullScreen, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericMSAARSContext, mainFullScreen, &GlobalRenderer::gRenderInstance.currentFrame, 0);

	if (genericMSAARSContext.contextFailed)
	{
		genericMSAARSContext.contextLogger->ProcessMessage();
		return -1;
	}

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

	mainFullScreenPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&fullscreenInfo, false);

	if (mainFullScreenPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Main Full Screen Pipeline failed creation", 31);
		return -1;
	}

	return 0;
}

int ApplicationLoop::CreateSkyBox()
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

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(BoxVerts, globalVertexBuffer, sizeof(BoxVerts), vertexAlloc, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(BoxIndices, globalIndexBuffer, sizeof(BoxIndices), indexAlloc, TransferType::CACHED);

	StringView names[6] = {
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face4.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face1.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face5.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face2.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face6.bmp"),
		AppInstanceTempAllocator.AllocateFromNullStringCopy("face3.bmp"),
	};

	ShaderResourceSetContext genericSkyBoxRSontext{ &mainAppLogger, false };

	skyboxCubeImage = ReadCubeImage(names, 6, BMP);

	static Matrix4f matrix = Identity4f();

	matrix.translate = Vector4f(-30.0, 0.0, 0.0, 1.0f);

	int camSkyboxData = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(SKYBOX, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	int skyboxDesc = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(SKYBOX, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&genericSkyBoxRSontext, camSkyboxData, &globalBufferLocation, nullptr, 0, 1, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.BindSampledImageToShaderResource(&genericSkyBoxRSontext, skyboxDesc, &skyboxCubeImage, 1, 0, 0);
	GlobalRenderer::gRenderInstance.descriptorManager.UploadConstant(&genericSkyBoxRSontext, skyboxDesc, &matrix, 0);

	if (genericSkyBoxRSontext.contextFailed)
	{
		genericSkyBoxRSontext.contextLogger->ProcessMessage();
		return -1;
	}

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

	skyboxPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsVulkanPipelineObject(&skyboxInfo, false);

	if (skyboxPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Skybox Pipeline failed creation", 31);
		return -1;
	}

	return 0;
}

void ApplicationLoop::InitializeRuntime()
{
	queueSema.Create();

	threadHandle = ThreadManager::LaunchOSBackgroundThread(ScanSTDIN, &stopThreadServer);

	mainDictionary.textureCache = (uintptr_t)mainTextureCacheMemory;
	
	mainDictionary.textureSize = sizeof(mainTextureCacheMemory);

	mainWindow.CreateMainWindow();

	GlobalRenderer::gRenderInstance.CreateRenderInstance(&RenderInstanceMemoryAllocator, &RenderInstanceTemporaryAllocator);

	GlobalRenderer::gRenderInstance.CreateVulkanRenderer(&mainWindow, mainLayoutAttachments.size());

	mainDeviceBuffer = GlobalRenderer::gRenderInstance.CreateUniversalBuffer(64 * MiB, BufferType::DEVICE_MEMORY_TYPE);
	mainHostBuffer = GlobalRenderer::gRenderInstance.CreateUniversalBuffer(128 * MiB, BufferType::HOST_MEMORY_TYPE);

	ImageFormat requestedColorFormats = ImageFormat::B8G8R8A8;

	ImageFormat mainColorFormat = GlobalRenderer::gRenderInstance.FindSupportedBackBufferColorFormat(&requestedColorFormats, 1);

	ImageFormat requestedDSVFormats = ImageFormat::D24UNORMS8STENCIL;

	ImageFormat mainDepthFormat = GlobalRenderer::gRenderInstance.FindSupportedDepthFormat(&requestedDSVFormats, 1);

	int mainRTVIndex = GlobalRenderer::gRenderInstance.CreateRSVMemoryPool(800 * MiB, mainColorFormat, 4096, 4096);

	int mainDSVIndex = GlobalRenderer::gRenderInstance.CreateRSVMemoryPool(1024 * MiB, mainDepthFormat, 4096, 4096);

	MSAAPost = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(&mainLayoutAttachments[0], nullptr);
	BasicShadow = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(&mainLayoutAttachments[1], nullptr);
	MSAAShadowMapping = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(&mainLayoutAttachments[2], nullptr);

	currentFrameGraphIndex = MSAAShadowMapping;

	frameGraphsCount = 3;

	mainPresentationSwapChain = GlobalRenderer::gRenderInstance.CreateSwapChainHandle(mainColorFormat, 800, 600);

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

	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, BasicShadow, 0, ShadowMapViewerClears.data());
	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, MSAAPost, 1, MSAAPostClears.data());
	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainPresentationSwapChain, MSAAShadowMapping, 1, MSAAShadowMappingClears.data());
	
	GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(MSAAPost, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, 800, 600, MSAAPostClears.data());
	GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(MSAAShadowMapping, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, mainShadowWidth, mainShadowHeight, MSAAShadowMappingClears.data());

	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAPost, 0, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAPost, 1, 1);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAShadowMapping, 0, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAShadowMapping, 1, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(BasicShadow, 0, 1);

	GlobalRenderer::gRenderInstance.CreatePipelines(pds.data(), pds.size());

	GlobalRenderer::gRenderInstance.CreateShaderGraphs(layouts.data(), layouts.size());

	mainLinearSampler = GlobalRenderer::gRenderInstance.CreateSampler(7);

	std::array frameGraphs = { MSAAPost, MSAAShadowMapping, BasicShadow };
	std::array frameRenderPassSelection = { 0, 1, 0 };

	std::array fullScreenFrameGraphs = { MSAAPost, BasicShadow };

	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(0, 0, frameGraphs.data(), frameRenderPassSelection.data(),  2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(1, 1, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(5, 2, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(13, 3, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(14, 4, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(15, 5, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(16, 6, fullScreenFrameGraphs.data(), frameRenderPassSelection.data() + 1, 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(17, 7, frameGraphs.data() + 1, frameRenderPassSelection.data(), 1);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(19, 8, frameGraphs.data(), frameRenderPassSelection.data(), 2);

	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(2);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(3);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(4);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(6);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(7);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(8);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(9);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(10);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(11);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(12);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(18);

	mainComputeQueueIndex = GlobalRenderer::gRenderInstance.CreateComputeQueue(15);

	CreateTexturePools();

	globalBufferLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT);
	globalIndexBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, globalIndexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	globalVertexBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainDeviceBuffer, globalVertexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalBufferDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(GENERIC, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(GENERIC, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	globalMeshLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalMeshSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalMaterialsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalMaterialsSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	globalLightBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalLightBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalLightTypesBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalLightTypesBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalDebugStructAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalDebugStructAllocSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalDebugTypesAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, sizeof(uint32_t), globalDebugStructMaxCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalMaterialIndicesLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalMaterialIndicesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);
	globalRenderableLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	globalBlendDetailsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalBlendDetailsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalBlendRangesLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalBlendRangesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT);

	globalGeometryDescriptionsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalGeometryDescriptionsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	globalGeometryRenderableLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, globalGeometryRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);


	jointMeshWorldMatrix = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(Matrix4f), 1, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	//jointMeshOrientations = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(Vector4f), 1, 16, AllocationType::PERFRAME, ComponentFormatType::R32G32B32A32_SFLOAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	//jointMeshPositions = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(Vector3f), 1, 12, AllocationType::PERFRAME, ComponentFormatType::R32G32B32_SFLOAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	//jointMeshScales = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(float), 1, alignof(float), AllocationType::PERFRAME, ComponentFormatType::R32_SFLOAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);
	jointMeshParentIndices = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(uint32_t), 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT);

	ShaderResourceSetContext globalDescriptorBuilder{ &mainAppLogger, false };

	GlobalRenderer::gRenderInstance.descriptorManager.BindBufferToShaderResource(&globalDescriptorBuilder, globalBufferDescriptor, &globalBufferLocation, nullptr, 0, 1, 0);

	GlobalRenderer::gRenderInstance.descriptorManager.SetVariableArrayCount(&globalDescriptorBuilder, globalTexturesDescriptor, 3, 512);

	std::array mainDrawBindGroups = { globalBufferDescriptor, globalTexturesDescriptor };

	GlobalRenderer::gRenderInstance.CreateRenderGraphData(MSAAPost, mainDrawBindGroups.data(), 2);
	GlobalRenderer::gRenderInstance.CreateRenderGraphData(MSAAShadowMapping, mainDrawBindGroups.data(), 2);
	GlobalRenderer::gRenderInstance.CreateRenderGraphData(BasicShadow, nullptr, 0);

	int creationRetSB = CreateSkyBox();
	int creationRetDB = CreateDebugCommandBuffers(globalDebugStructMaxCount);
	int creationRetLW = CreateLightAssignments(globalLightMax);
	int creationRetMW = CreateMeshWorldAssignment(mainGrid.numberOfDivision * mainGrid.numberOfDivision * mainGrid.numberOfDivision);
	int creationRetGMB = CreateGenericMeshCommandBuffers(globalMeshCountMax);
	int creationRetSM = CreateShadowMapManager(4, globalMeshCountMax, 1024, 1024, mainShadowWidth, mainShadowHeight);
	int creationRetFS = CreateMSAAPostFullScreen();

	if (creationRetSB < 0 ||
		creationRetDB < 0 ||
		creationRetLW < 0 ||
		creationRetMW < 0 ||
		creationRetGMB < 0 ||
		creationRetSM < 0 ||
		creationRetFS < 0)
	{
		mainAppLogger.AddLogMessage(LOGINFO, "Shutting down environment, cannot create minimal sandbox", 56);
		return;
	}

	AddLight(mainSpotLight, LightType::SPOT);
	AddLight(mainDirectionalLight, LightType::DIRECTIONAL);
	
	c.CamLookAt(Vector3f(25.0 * -mainDirectionalLight.direction.x, 25.0 * -mainDirectionalLight.direction.y,  25.0 * -mainDirectionalLight.direction.z), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));

	WriteCameraMatrix();	
	
}

void ApplicationLoop::CleanupRuntime()
{
	stopThreadServer = true;

	OSCloseThread(&threadHandle);

	GlobalRenderer::gRenderInstance.WaitOnRender();

	ThreadManager::DestroyThreadManager();

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
	SMBFile SMB{};

	int loadSmbRet = SMB.LoadFile(file, &GlobalInputScratchAllocator, &mainAppLogger);

	if (!loadSmbRet)
	{
		ProcessSMBFile(&SMB);
		return;
	}

	mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("SMB Load Failed when rendering"));
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
			i = j;
		}
		else 
		{
			j++;
		}
	}

	wordSize = j - i;
	if (wordSize)
	{ 
	StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
	char* stringData = (char*)AppInstanceTempAllocator.Allocate(wordSize);
	out->stringData = stringData;
	out->charCount = wordSize;
	memcpy(stringData, words + i, wordSize);
	}

	return wordCount;
}

int ApplicationLoop::CreateMaterialRange(int gpuMaterialRangeID, int count, int* ids)
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(ids, globalMaterialIndicesLocation, sizeof(uint32_t) * count, sizeof(uint32_t) * gpuMaterialRangeID, TransferType::CACHED);

	return 0;
}

int ApplicationLoop::CreateRenderable(int meshCPURenderableIndex, int meshGPURenderableIndex, const Matrix4f& mat, int geomIndex,  int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount)
{
	RenderableMeshCPUData* meshCpuRenderable = nullptr;

	meshCpuRenderable = renderablesMeshObjects.Update(meshCPURenderableIndex);

	GPUMeshRenderable renderable{};

	meshCpuRenderable->instanceCount = renderable.instanceCount = instanceCount;
	meshCpuRenderable->meshIndex = renderable.meshIndex = meshIndex;
	meshCpuRenderable->meshBlendRangeIndex = renderable.blendLayersStart = blendStart;
	meshCpuRenderable->meshMaterialRangeIndex = renderable.materialStart = materialStart;
	meshCpuRenderable->meshBlendRangeCount = meshCpuRenderable->meshMaterialCount = renderable.materialCount = materialCount;
	meshCpuRenderable->renderableIndex = meshGPURenderableIndex;
	
	renderable.geomIndex = geomIndex;
	renderable.transform = mat;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&renderable, globalRenderableLocation, sizeof(GPUMeshRenderable), sizeof(GPUMeshRenderable) * meshGPURenderableIndex, TransferType::CACHED);

	return 0;
}

int ApplicationLoop::CreateMaterial(
	int gpuMaterialID,
	int flags,
	int* texturesIDs,
	int textureCount,
	const Vector4f& color
)
{	
	GPUMaterial material{};

	uint32_t* ptr = material.textureHandles.comp;

	material.albedoColor = color;

	material.materialFlags = flags;

	for (int i = 0; i < textureCount; i++)
	{
		ptr[i] = texturesIDs[i];
	}

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * gpuMaterialID, TransferType::CACHED);

	return 0;
}

int ApplicationLoop::CreateMaterial(
	int gpuMaterialID,
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

	material.albedoColor = diffuseColor;
	material.emissiveColor = emissiveColor;
	material.specularColor = specularColor;
	material.shininess = shininess;

	material.materialFlags = flags;

	for (int i = 0; i < textureCount; i++)
	{
		ptr[i] = texturesIDs[i];
	}

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * gpuMaterialID, TransferType::CACHED);

	return 0;
}


int ApplicationLoop::CreateMeshHandle(
	int meshCPUDataIndex, int meshGPUDataIndex,
	void* vertexData, void* indexData,
	int vertexFlags, int vertexCount, int vertexStride,
	int indexStride, int indexCount,
	 Sphere& sphere,
	int vertexAlloc, int indexAlloc
)
{
	MeshCPUData* mesh = nullptr;

	mesh = meshCPUData.Update(meshCPUDataIndex);

	mesh->indicesCount = indexCount;

	mesh->verticesCount = vertexCount;

	mesh->vertexSize = vertexStride;

	mesh->indexSize = indexStride;

	mesh->meshDeviceIndex = meshGPUDataIndex;

	mesh->vertexFlags = vertexFlags;

	mesh->deviceIndices = indexAlloc;

	mesh->deviceVertices = vertexAlloc;

	GPUMeshDetails handles{};

	handles.vertexFlags = vertexFlags;
	handles.stride = vertexStride;

	handles.indexCount = mesh->indicesCount;
	handles.firstIndex = indexAlloc / mesh->indexSize;
	handles.vertexByteOffset = vertexAlloc;

	memcpy(&handles.sphere, &sphere, sizeof(Vector4f));

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&handles, globalMeshLocation, sizeof(GPUMeshDetails), meshGPUDataIndex * sizeof(GPUMeshDetails), TransferType::CACHED);

	return 0;
}

void ApplicationLoop::SetPositionOfGeometry(int geomIndex, const Vector3f& pos)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;
	
	RenderableGeomCPUData* geom = nullptr;

	if (geomIndex >= renderablesGeomObjects.allocatorPtr.load())
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Incorrect Geometry Location"));
		return;
	}

	geom = &renderablesGeomObjects.dataArray[geomIndex];

	int geomRenderable = geom->geomIndex;

	Matrix4f transform = Identity4f();;

	transform.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

	rendInst.UpdateDriverMemory(&transform, globalGeometryRenderableLocation, sizeof(Matrix4f), (sizeof(GPUMeshRenderable) * geomRenderable) + 16, TransferType::CACHED);
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
	else if (strncmp(command.stringData, "debugscreen", 12) == 0)
	{
		PrintDebugMemoryAllocation();
	}
	else {
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Unprocessed command entered"));
	}

	ThreadSharedMessageQueue.Read();
}

int ApplicationLoop::CreateGPUGeometryDetails(int geometryDetailsIndex, const AxisBox& minMaxBox)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;

	GPUGeometryDetails details{};

	details.minMaxBox = minMaxBox;

	rendInst.UpdateDriverMemory(&details, globalGeometryDescriptionsLocation, sizeof(GPUGeometryDetails), sizeof(GPUGeometryDetails) * geometryDetailsIndex, TransferType::CACHED);

	return 0;
}

int ApplicationLoop::CreateGPUGeometryRenderable(int geomCPURenderableIndex, int geomGPURenderableIndex, const Matrix4f& matrix, int geomDesc, int renderableStart, int renderableCount)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;

	GPUGeometryRenderable renderable{};

	renderable.geomDescIndex = geomDesc;
	renderable.renderableStart = renderableStart;
	renderable.renderableCount = renderableCount;
	renderable.transform = matrix;

	rendInst.UpdateDriverMemory(&renderable, globalGeometryRenderableLocation, sizeof(GPUGeometryRenderable), sizeof(GPUGeometryRenderable) * geomGPURenderableIndex, TransferType::CACHED);

	return 0;
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

int CreateBlendRange(int gpuBlendRangeID, int* blendIDs, int blendCount)
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(blendIDs, globalBlendRangesLocation, sizeof(int) * blendCount, sizeof(int) * gpuBlendRangeID, TransferType::CACHED);

	return 0;
}

int CreateBlendDetails(int gpuBlendDescID, BlendMaterialType type, float constantAlpha)
{
	GPUBlendDetails details;

	details.type = type;
	details.alphaBlend = constantAlpha;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&details, globalBlendDetailsLocation, sizeof(GPUBlendDetails),  sizeof(GPUBlendDetails) * gpuBlendDescID, TransferType::CACHED);

	return 0;
}

int CreateBlendDetails(int gpuBlendDescID, BlendMaterialType type, int mapID)
{
	GPUBlendDetails details;

	details.type = type;
	details.alphaMapHandle = mapID;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&details, globalBlendDetailsLocation, sizeof(GPUBlendDetails), sizeof(GPUBlendDetails) * gpuBlendDescID, TransferType::CACHED);

	return 0;
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
		GlobalRenderer::gRenderInstance.IncreaseMSAA(currentFrameGraphIndex, 0);
	}

	if (keyActions[KC_ONE].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance.DecreaseMSAA(currentFrameGraphIndex, 0);
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

		GlobalRenderer::gRenderInstance.ResetCommandList();
		GlobalRenderer::gRenderInstance.AddCommandQueue(mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance.AddCommandQueue(currentFrameGraphIndex, ATTACHMENT_COMMANDS);
	}
}


void UpdateLight(GPULightSource& lightDesc, int lightIndex)
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&lightDesc, globalLightBuffer, sizeof(GPULightSource), sizeof(GPULightSource) * lightIndex, TransferType::CACHED);
}

int ReadCubeImage(StringView* name, int textureCount, TextureIOType ioType)
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

		nRet = OSOpenFile(name[i].stringData, name[i].charCount, READ, &outHandle);

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

	int ret = mainDictionary.textureHandles[textureStart] =
		GlobalRenderer::gRenderInstance.CreateCubeImageHandle(
			textureCount * details->dataSize,
			details->width,
			details->height,
			details->miplevels,
			details->type,
			loop->GetPoolIndexByFormat(details->type),
			mainLinearSampler);

	GlobalRenderer::gRenderInstance.UpdateImageMemory(
		details->data,
		ret,
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

	int totalBlobSize = 0;
	
	int ret;

	for (int i = 0; i < mipCounts; i++)
	{
		TextureDetails stubDetails{};

		OSFileHandle outHandle{};

		int nRet = OSOpenFile(name[i].stringData, name[i].charCount, READ, &outHandle);

		int size = outHandle.fileLength;

		void* fileData = GlobalInputScratchAllocator.Allocate(size);

		OSReadFile(&outHandle, size, (char*)fileData);

		OSCloseFile(&outHandle);

		int filePointer = ReadBMPDetails((char*)fileData, &stubDetails);

		stubDetails.data = (char*)mainDictionary.AllocateImageCache(stubDetails.dataSize);

		stubDetails.currPointer = stubDetails.data;

		totalBlobSize += stubDetails.dataSize;

		ReadBMPData((char*)fileData, filePointer, &stubDetails);

		if (!i)
		{
			memcpy(details, &stubDetails, sizeof(TextureDetails));
		}

	}


	details->miplevels = mipCounts;
	details->arrayLayers = 1;

	ret = mainDictionary.textureHandles[textureStart] =
		GlobalRenderer::gRenderInstance.CreateImageHandle(
			totalBlobSize,
			details->width,
			details->height,
			details->miplevels,
			details->type,
			loop->GetPoolIndexByFormat(details->type),
			mainLinearSampler);

	GlobalRenderer::gRenderInstance.UpdateImageMemory(
		details->data,
		mainDictionary.textureHandles[textureStart],
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

	GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);

	return textureStart;
}

int AddLight(GPULightSource& lightDesc, LightType type)
{
	if (globalLightCount == globalLightMax)
	{
		return -1;
	}

	int lightLocation = globalLightCount++;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&lightDesc, globalLightBuffer, sizeof(GPULightSource), sizeof(GPULightSource) * lightLocation, TransferType::CACHED);

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&type, globalLightTypesBuffer, sizeof(LightType), sizeof(LightType) * lightLocation, TransferType::CACHED);

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

		GlobalRenderer::gRenderInstance.UpdateDriverMemory(&Upload, mainShadowMapManager.shadowMapViewProjAlloc, sizeof(Upload), sizeof(Upload) * lightLocation, TransferType::CACHED);

		ShadowMapView view{};

		int shadowXScale = mainShadowMapManager.atlasWidth / mainShadowMapManager.avgShadowWidth;
		int shadowYScale = mainShadowMapManager.atlasHeight / mainShadowMapManager.avgShadowHeight;

		view.xOff = (float)(mainShadowMapManager.avgShadowWidth) * (mainShadowMapManager.zoneAlloc % shadowXScale);
		view.xOff /= (float)mainShadowMapManager.atlasWidth;

		view.yOff = (float)(mainShadowMapManager.avgShadowHeight) * (mainShadowMapManager.zoneAlloc++ / shadowYScale);
		view.yOff /= (float)mainShadowMapManager.atlasHeight;
		view.xScale = 1.0f / (float)shadowXScale;
		view.yScale = 1.0f / (float)shadowYScale;

		GlobalRenderer::gRenderInstance.UpdateDriverMemory(&view, mainShadowMapManager.shadowMapAtlasViewsAlloc, sizeof(view), sizeof(view) * lightLocation, TransferType::CACHED);

	}

	return lightLocation;
}

void PrintDebugMemoryAllocation()
{
	char StringBuffer[512];

	std::pair<int, int> allocDetails{};

	allocDetails = StartupMemoryAllocator.GetUsageAndCapacity();

	int actualSize = snprintf(StringBuffer, 512, "Start up memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = RenderInstanceMemoryAllocator.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Render Instance memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = GlobalInputScratchAllocator.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Global Input memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = osAllocator.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "OS Memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = vertexAndIndicesAlloc.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Main Memory vertex and indices memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = geometryObjectSpecificAlloc.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Geometry memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = indexBufferAlloc.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Main Index memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = vertexBufferAlloc.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Main Vertex memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = mainDictionary.GetCacheUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Main texture memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	GlobalRenderer::gRenderInstance.PrintOutBufferAllocations(&mainAppLogger);

	GlobalRenderer::gRenderInstance.PrintOutTexturePoolAllocations(&mainAppLogger);
}

int ApplicationLoop::AllocateCPUGeometryDetails(int numberOfDetails) {
	return -1;
}

int ApplicationLoop::AllocateGPUGeometryDetails(int numberOfDetails) 
{
	int geomDescriptionsIndex = -1;

	if (globalGeometryDescriptionsCount+numberOfDetails > globalGeometryDescriptionsCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many geometry details created"));
		return geomDescriptionsIndex;
	}

	geomDescriptionsIndex = (globalGeometryDescriptionsCount);

	globalGeometryDescriptionsCount += numberOfDetails;

	return geomDescriptionsIndex;
}

int ApplicationLoop::AllocateCPUMeshDetails(int numberOfDetails) {
	
	int meshIndex;

	meshIndex = meshCPUData.AllocateN(numberOfDetails);

	if (meshIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many CPU meshes created"));
	}

	return meshIndex;
}

int ApplicationLoop::AllocateGPUMeshDetails(int numberOfDetails) {
	
	int meshIndex;

	if (globalMeshCount+numberOfDetails > globalMeshCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many GPU meshes created"));
		return -1;
	}

	meshIndex = globalMeshCount;

	globalMeshCount += numberOfDetails;

	return meshIndex;
}

int ApplicationLoop::AllocateCPUMeshRenderable(int numberOfRenderables) 
{
	int meshRenderableIndex = -1;

	meshRenderableIndex = renderablesMeshObjects.AllocateN(numberOfRenderables);

	if (meshRenderableIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many CPU Mesh renderables created"));	
	}

	return meshRenderableIndex;
}

int ApplicationLoop::AllocateGPUMeshRenderable(int numberOfRenderables) 
{
	int meshRenderableIndex = -1;

	if (globalRenderableCount == globalRenderableMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many GPU Mesh renderables created"));
		return meshRenderableIndex;
	}

	int renderableLocation = (globalRenderableCount);

	globalRenderableCount += numberOfRenderables;

	return renderableLocation;
}

int ApplicationLoop::AllocateCPUGeometryInstances(int numberOfInstances) 
{	
	int geomRendCPUCode = 0;

	geomRendCPUCode = renderablesGeomObjects.AllocateN(numberOfInstances);

	if (geomRendCPUCode < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many CPU geometry renderables created"));
	}

	return geomRendCPUCode;
}

int ApplicationLoop::AllocateGPUGeometryInstances(int numberOfInstances) {
	
	int geomRenderableIndex = -1;

	if (globalGeometryRenderableCount+numberOfInstances > globalGeometryRenderableCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many GPU geometry renderables created"));
		return geomRenderableIndex;
	}

	geomRenderableIndex = globalGeometryRenderableCount;

	globalGeometryRenderableCount += numberOfInstances;

	return geomRenderableIndex;
}


int ApplicationLoop::AllocateGPUMaterialData(int numberOfMaterials)
{
	int materialIndexReturn = -1;

	if (globalMaterialsIndex == globalMaterialsMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many materials created"));
		return materialIndexReturn;
	}

	materialIndexReturn = globalMaterialsIndex;

	globalMaterialsIndex += numberOfMaterials;

	return materialIndexReturn;
}

int ApplicationLoop::AllocateGPUMaterialRanges(int numberOfRanges)
{
	int materialRangeIndex = -1;

	if (globalMaterialRangeMax < globalMaterialRangeCount + numberOfRanges)
	{
		mainAppLogger.AddLogMessage(LOGERROR, AppInstanceTempAllocator.AllocateFromNullStringCopy("Too many material ranges created"));
		return materialRangeIndex;
	}

	materialRangeIndex = globalMaterialRangeCount;

	globalMaterialRangeCount += numberOfRanges;

	return materialRangeIndex;
}

int ApplicationLoop::AllocateGPUBlendDescriptions(int numberOfDescs)
{
	int blendDescIndex = -1;

	if (globalBlendDetailCount + numberOfDescs > globalBlendDetailMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Too many blend details allocated", 32);
		return blendDescIndex;
	}

	blendDescIndex = globalBlendDetailCount;

	globalBlendDetailCount += numberOfDescs;

	return blendDescIndex;
}

int ApplicationLoop::AllocateGPUBlendRanges(int numberOfRanges)
{
	int blendRangeReturn = -1;

	if (globalBlendRangeCount + numberOfRanges > globalBlendRangeMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, "Too many blend ranges allocated", 32);
		return blendRangeReturn;
	}

	blendRangeReturn = globalBlendRangeCount;

	globalBlendRangeCount += numberOfRanges;

	return blendRangeReturn;
}

void CreateBitTangentFromNormal(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator)
{

	Vector3f* totalTangents = (Vector3f*)tempAllocator->CAllocate(sizeof(Vector3f) * totalVertCount, 16);
	float* signBitTangents = (float*)tempAllocator->CAllocate(sizeof(float) * totalVertCount, 4);
	Vector3f* normals = (Vector3f*)tempAllocator->CAllocate(sizeof(Vector3f) * totalVertCount, 16);

	for (int lead = 0; lead < totalIndexCount - 2; lead++)
	{
		uint16_t index1 = indices[lead];
		uint16_t index2 = indices[lead + 1];
		uint16_t index3 = indices[lead + 2];

		Vector3f vert1 = Vector3f(pos[index1].x, pos[index1].y, pos[index1].z);
		Vector3f vert2 = Vector3f(pos[index2].x, pos[index2].y, pos[index2].z);
		Vector3f vert3 = Vector3f(pos[index3].x, pos[index3].y, pos[index3].z);


		Vector3f e1 = vert2 - vert1;
		Vector3f e2 = vert3 - vert1;

		Vector3f normal = Cross(e1, e2);

		float area2 = Dot(normal, normal);

		if (area2 <= 0.0f) {
			continue;
		}

		normals[index1] = normals[index1] + normal;
		normals[index2] = normals[index2] + normal;
		normals[index3] = normals[index3] + normal;

		Vector2f uv1 = uvs[index1];
		Vector2f uv2 = uvs[index2];
		Vector2f uv3 = uvs[index3];

		Vector2f duv1 = uv2 - uv1;
		Vector2f duv2 = uv3 - uv1;

		float f_det = (duv1.x * duv2.y - duv2.x * duv1.y);



		float f = 1.0f / f_det;

		Vector3f tangent;
		tangent = Normalize((e1 * duv2.y - e2 * duv1.y) * f);

		Vector3f bitangent = Normalize((e2 * duv1.x - e1 * duv2.x) * f);


		Vector3f n = Normalize(normal);
		float sign = (Dot(Cross(n, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

		totalTangents[index1] = totalTangents[index1] + tangent;
		totalTangents[index2] = totalTangents[index2] + tangent;
		totalTangents[index3] = totalTangents[index3] + tangent;

		signBitTangents[index1] += sign;
		signBitTangents[index2] += sign;
		signBitTangents[index3] += sign;
	}


	for (int lead = 0; lead < totalVertCount; lead++)
	{
		Vector3f normal = normals[lead];

		normal = Normalize(normal);

		Vector3f tangent = totalTangents[lead];

		tangent = Normalize(tangent - (normal * Dot(normal, tangent)));

		float handedness = signBitTangents[lead] < 0.0f ? -1.0f : 1.0f;

		Vector4f outTangent = Vector4f(tangent.x, tangent.y, tangent.z, handedness);

		tangents[lead] = outTangent;

		outNormals[lead] = normal;
	}
}