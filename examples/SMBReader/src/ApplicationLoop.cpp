#include "ApplicationLoop.h"
#include "allocator/AppAllocator.h"
#include "Camera.h"
#include "CommonRenderTypes.h"
#include "FileManager.h"
#include "logger/Logger.h"
#include "RenderInstance.h"
#include "SMBExporter.h"
#include "SMBFile.h"
#include "SMBTexture.h"
#include "StringUtils.h"
#include "TextureDictionary.h"
#include "ThreadManager.h"
#include "WindowManager.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include "UI.h"


#include <array>
#include <queue>

#define MAX_MESH_TEXTURES 8192
#define MAX_IMAGE_DIM 4096

#define MAX_GPU_MATERIALS 8
#define DEFAULT_GPU_ITEM_ARRAY_INDEX 0
#define MAX_JOINT_VISUALIZERS 10
#define MAX_SMB_ARENAS 10

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

enum ShaderResourceLayoutIdentifiers
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
	OUTLINE, 
	FULLSCREEN, 
	SHADOWMAPDRAW,
	SHADOWMAPCULL,
	JOINTVISUAL,
	UICULLING,
	UIDRAWING,
	UIDEPTHCOUNT,
	UICHILDDEPTHADD,
	UIINDEXASSIGNMENT, 
	UILAYOUTSIZES,
	UIABSOLUTEPOSITION
};

static std::array<StringView, 10> pds = {
	STRING_VIEW_FROM_LITERAL_INIT_LIST("GenericPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("TextPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("DebugPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("NormalDebugPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("SkyboxPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("OutlinePipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("FullscreenPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("ShadowMap.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("JointVisualPipeline.pld"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIPipeline.pld"),
};

static std::array<StringView, 27> layouts = {
	STRING_VIEW_FROM_LITERAL_INIT_LIST("3DTexturedLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("TextLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("InterpolateMeshLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("PolynomialLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("IndirectCull.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("DebugDraw.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("IndirectDebug.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("PrefixSum.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("PrefixSumAdd.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("WorldObjectDivison.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("MeshWorldAssignments.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("LightObjectDivision.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("LightWorldAssignment.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("NormalDebug.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("Skybox.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("OutlineLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("FullscreenLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("ShadowMapLayout.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("ShadowMapClipping.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("JointVisualSL.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UICulling.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIDrawing.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIDepthCount.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIChildDepthAdd.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIIndexAssignments.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UILayoutsSizes.sgr"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("UIAbsolutePositioning.sgr"),
};

static std::array<StringView, 3> mainLayoutAttachments =
{
	STRING_VIEW_FROM_LITERAL_INIT_LIST("MSAAPostProcess.adf"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("BasicShadowMapAtt.adf"),
	STRING_VIEW_FROM_LITERAL_INIT_LIST("ShadowAtlasUse.adf")
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
	uint32_t materialFlags;
	float shininess;
	uint32_t unused2;
	uint32_t unused3;
	Vector4f albedoColor;
	Vector4ui textureHandles; // y - normalMapCoordinates // x- albedo
	Vector4f emissiveColor;
	Vector4f specularColor;
};

struct GPUMeshRenderable
{
	uint32_t meshIndex;
	uint32_t lightCount;
	uint32_t instanceCount;
	uint32_t geomIndex;
	uint32_t lightIndices[4];
	uint32_t materialStart;
	uint32_t blendLayersStart;
	uint32_t materialCount;
	uint32_t pad2;
	Matrix4f transform;
};

struct GPUMeshDetails
{
	uint32_t vertexFlags;
	uint32_t stride;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexByteOffset;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	Sphere sphere;
};

struct GPUGeometryDetails
{
	AxisBox minMaxBox;
};

struct GPUGeometryRenderable
{
	uint32_t geomDescIndex;
	uint32_t renderableStart;
	uint32_t renderableCount;
	uint32_t pad1;
	Matrix4f transform;
};

enum BlendMaterialType : uint32_t
{
	ConstantAlpha = 1,
	BlendMap = 2,
};

struct GPUBlendDetails
{
	BlendMaterialType type;
	union {
		float alphaBlend;
		uint32_t alphaMapHandle;
	};
};

struct IndirectDrawData
{
	int commandBufferAlloc;
	int commandBufferCount;
	int commandBufferSize;
	int commandBufferCountAlloc;
	ShaderResourceSetHandle indirectDrawDescriptor;
	int indirectDrawPipeline;
	ShaderResourceSetHandle indirectCullDescriptor;
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
	ShaderResourceSetHandle prefixSumDescriptors;
	int prefixSumPipeline;
	ShaderResourceSetHandle sumAfterDescriptors;
	int sumAfterPipeline;
	ShaderResourceSetHandle sumAppliedToBinDescriptors;
	int sumAppliedToBinPipeline;
	
	ShaderResourceSetHandle preWorldSpaceDivisionDescriptor; //for getting all the counts
	int preWorldSpaceDivisionPipeline;

	ShaderResourceSetHandle postWorldSpaceDivisionDescriptor; //for assigning all the slots
	int postWorldSpaceDivisionPipeline;
	int worldSpaceDivisionAlloc; //where all the assignments go 
};

enum class LightType : uint32_t
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

enum class DebugDrawType : uint32_t
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

	ShaderResourceSetHandle shadowClippingDescriptor1;
	ShaderResourceSetHandle shadowClippingDescriptor2;
	int shadowClippingPipeline;
};

struct ShadowMapDebugPipelineData
{
	int fullScreenPipeline;
	ShaderResourceSetHandle fullScreenDescriptorSet;
	int shadowMapPipeline;
	ShaderResourceSetHandle shadowMapDescriptorSet;
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
	int cpuVertices;
	int cpuIndices;
	int meshDeviceIndex;
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

static int mainGPU = -1;
static int mainLogicalDevice = -1;
static int mainPresentationSwapChain = -1;
static int mainPresentationWindow = -1;
static int mainHostBuffer = -1;
static int mainDeviceBuffer = -1;
static int mainDescriptorManagerIndex = -1;
static int mainCommandStreamIndex = -1;

static DeviceSlabAllocator mainHostAllocator, mainDeviceAllocator;

static IndirectDrawData mainIndirectDrawData;
static IndirectDrawData debugIndirectDrawData;

static WorldSpaceGPUPartition worldSpaceAssignment;
static WorldSpaceGPUPartition lightAssignment;

static int globalIndexBuffer = 0;
static int globalIndexBufferSize = 1024 * KiB;
static int globalVertexBuffer = 0;
static int globalVertexBufferSize = 1024 * KiB;

static int globalLightBufferSize = 16 * KiB;
static int globalLightMax = globalLightBufferSize / sizeof(GPULightSource);
static int globalLightTypesBufferSize = globalLightMax * sizeof(LightType);

static int globalLightTypesBuffer = 0;
static int globalLightBuffer = 0;
static int globalLightCount = 0;

static int normalDebugAlloc;

static int globalDebugStructAlloc = 0;
static const int globalDebugStructAllocSize = 10 * KiB;
static int globalDebugTypesAlloc = 0;

static int globalBufferLocation;
static ShaderResourceSetHandle globalBufferDescriptor;
static ShaderResourceSetHandle globalTexturesDescriptor;

static int globalMeshLocation;
static const int globalMeshSize = 24 * KiB;

static int globalGeometryDescriptionsLocation;
static const int globalGeometryDescriptionsSize = 1 * KiB;

static int globalGeometryRenderableLocation;
static const int globalGeometryRenderableSize = 1 * KiB;

static int globalRenderableLocation;
static const int globalRenderableSize = 24 * KiB;

static int globalMaterialIndicesLocation;
static const int globalMaterialIndicesSize = 4 * KiB;

static int globalMaterialsLocation; 
static const int globalMaterialsSize = 4 * KiB;

static int globalBlendDetailsLocation;
static int globalBlendRangesLocation;
static const int globalBlendDetailsSize = 1 * KiB;
static const int globalBlendRangesSize = 1 * KiB;

static std::atomic<int> globalRenderableCount = 0;
static std::atomic<int> globalBlendDetailCount = 0;
static std::atomic<int> globalBlendRangeCount = 0;
static std::atomic<int> globalMaterialsIndex = 0;
static std::atomic<int> globalGeometryRenderableCount = 0;
static std::atomic<int> globalGeometryDescriptionsCount = 0;
static std::atomic<int> globalMeshCount = 0;
static std::atomic<int> globalDebugStructCount = 0;
static std::atomic<int> globalMaterialRangeCount = 0;

static const int globalMaterialsMax = globalMaterialsSize / sizeof(GPUMaterial);
static const int globalBlendDetailMax = globalBlendDetailsSize / sizeof(GPUBlendDetails);
static const int globalBlendRangeMax = globalBlendRangesSize / sizeof(uint32_t);
static const int globalMaterialRangeMax = globalMaterialIndicesSize / sizeof(uint32_t);
static const int globalRenderableMax = globalRenderableSize / sizeof(GPUMeshRenderable);
static const int globalDebugStructMaxCount = globalDebugStructAllocSize / sizeof(GPUDebugRenderable);
static const int globalMeshCountMax = globalMeshSize / sizeof(GPUMeshDetails);
static const int globalGeometryDescriptionsCountMax = globalGeometryDescriptionsSize / sizeof(GPUGeometryDetails);
static const int globalGeometryRenderableCountMax = globalGeometryRenderableSize / sizeof(GPUGeometryRenderable);

static int shadowMapIndex = 0;

static DeviceSlabAllocator indexBufferAlloc(globalIndexBufferSize);

static DeviceSlabAllocator vertexBufferAlloc(globalVertexBufferSize);

static TextureDictionary mainDictionary;

static OSThreadHandle threadHandle;

static int currentFrameGraphIndex = 4;
static int mainComputeQueueIndex = 0;
static int mainFullScreenPipeline = 0;

static int jointMeshPipelines[MAX_JOINT_VISUALIZERS];
static uint32_t jointMeshStaringLocations[MAX_JOINT_VISUALIZERS];
static int jointMeshPipelinesCount = 0;

static int jointMeshVertexAlloc = 0;
static int jointMeshIndexAlloc = 0;

static int jointMeshWorldMatrixMaxCount = 164;
static int jointMeshWorldMatrixCount = 0;

static int jointMeshWorldMatrix = -1;
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

static PoolAllocator<MeshCPUData> meshCPUData{};
static PoolAllocator<GeometryCPUData> geometryCPUData{};
static PoolAllocator<RenderableGeomCPUData> renderablesGeomObjects{};
static PoolAllocator<RenderableMeshCPUData> renderablesMeshObjects{};

static UniformGrid mainGrid = {	
	.max = Vector4f(100.0f, 50.0f, 100.0f, 0.0),
	.min = Vector4f(-100.0f, -50.0f, -100.0f, 0.0),
	.numberOfDivision = 5,
};

static int skyboxPipeline = 0;
static int skyboxCubeImage = -1;

static char DebugAllocQueueMemory[512];
static char RenderableAllocQueueMemory[512];

static CircularMessageQueueMPSC DebugAllocQueue{ DebugAllocQueueMemory, sizeof(DebugAllocQueueMemory) };
static CircularMessageQueueMPSC RenderableAllocQueue{ RenderableAllocQueueMemory, sizeof(RenderableAllocQueueMemory) };

static Logger mainAppLogger{};

static int mainLinearSampler = -1;

static uint32_t swcImageIndex = 0;

static int mainRTVIndex;
static int mainDSVIndex;

static DeviceSlabAllocator mainRTVSlab, mainDSVSlab;

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

std::array<bool, MAXDIRS> camMovements;

static Semaphore queueSema;
static std::queue<int> wordCounts;

static Camera c;

static char RenderInstanceMemoryPool[256 * MiB];
static char RenderInstanceTemporaryPool[64 * KiB];
static char AppInstanceTempMemory[64 * KiB];
static char GlobalInputScratchMemory[128 * MiB];
static char PollingThreadSharedCmdMemory[4 * KiB];
static char PollingThreadStringViewMemory[4 * KiB];
static char LoggerMessageMemory[64 * KiB];

static TLSFAllocator RenderInstanceMemoryAllocator{ RenderInstanceMemoryPool, sizeof(RenderInstanceMemoryPool) };
static RingAllocator RenderInstanceTemporaryAllocator{ RenderInstanceTemporaryPool, sizeof(RenderInstanceTemporaryPool) };
static RingAllocator AppInstanceTempAllocator{ AppInstanceTempMemory, sizeof(AppInstanceTempMemory) };
static SlabAllocator GlobaScratchAllocator{ GlobalInputScratchMemory, sizeof(GlobalInputScratchMemory) };
static MessageQueue ThreadSharedMessageQueue{ PollingThreadSharedCmdMemory, sizeof(PollingThreadSharedCmdMemory) };
static RingAllocator ThreadSharedStringViewAllocator{ PollingThreadStringViewMemory, sizeof(PollingThreadStringViewMemory) };

static char SMBThreadedFileInputMemory[48 * MiB];
static char SMBThreadedFileScratchMemory[60 * KiB];

static const int SMBArenasSize[MAX_SMB_ARENAS] =
{
	1 * MiB,
	1 * MiB,
	1 * MiB,
	1 * MiB,
	4 * MiB,
	4 * MiB,
	4 * MiB,
	4 * MiB,
	12 * MiB,
	16 * MiB
};

static const int SMBArenasOffset[MAX_SMB_ARENAS] =
{
	0,
	1 * MiB,
	2 * MiB,
	3 * MiB,
	4 * MiB,
	8 * MiB,
	12 * MiB,
	16 * MiB,
	20 * MiB,
	32 * MiB
};

static SlabAllocator SMBThreadedFileInputAllocators[MAX_SMB_ARENAS] =
{
	{SMBThreadedFileInputMemory					    , SMBArenasSize[0] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[1], SMBArenasSize[1] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[2], SMBArenasSize[2] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[3], SMBArenasSize[3] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[4], SMBArenasSize[4] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[5], SMBArenasSize[5] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[6], SMBArenasSize[6] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[7], SMBArenasSize[7] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[8], SMBArenasSize[8] },
	{SMBThreadedFileInputMemory + SMBArenasOffset[9], SMBArenasSize[9] },
};

static SlabAllocator SMBThreadedFileScratchAllocators[MAX_SMB_ARENAS] =
{
	{SMBThreadedFileScratchMemory + (6 * KiB * 0), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 1), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 2), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 3), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 4), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 5), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 6), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 7), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 8), 6 * KiB  },
	{SMBThreadedFileScratchMemory + (6 * KiB * 9), 6 * KiB  },
};

static std::atomic_bool arenasUsed[MAX_SMB_ARENAS];

static char vertexAndIndicesMemory[16 * MiB];
static char geometryObjectSpecificMemory[4 * KiB];
static char mainTextureCacheMemory[256 * MiB];
static char mainOSDataManagement[KiB];

static SlabAllocator osAllocator(mainOSDataManagement, sizeof(mainOSDataManagement));
static SlabAllocator vertexAndIndicesAlloc(vertexAndIndicesMemory, sizeof(vertexAndIndicesMemory));
static SlabAllocator geometryObjectSpecificAlloc(geometryObjectSpecificMemory, sizeof(geometryObjectSpecificMemory));

static ShaderResourceSetHandle mainFullScreen;

static int globalUIContainerData = -1;
static int globalUIElementsOffsets = -1;
static int globalUIElementsData = -1;
static int globalUIElementsIndirectCountBuffer = -1;
static int globalUIElementsIndirectBuffer = -1;
static int globalUICullPipelineIndex = -1;
static int globalUIDrawingPipelineIndex = -1;
static int globalDepthCounts = -1;
static int globalDepthOffsets = -1;
static int globalChildrenPrefixSumCount = -1;
static int globalChildrenOffsets = -1;
static int globalUIIndirectionHandleBuffer = -1;
static int globalUIIndirectionPositionalHandleBuffer = -1;
static int globalUICountPipeline = -1;
static int globalUIPrefixSumPipeline = -1;
static int globalUIChildDepthAddPipeline = -1;
static int globalUIIndexAssignmentPipeline = -1;

static int globalChildSubtreeDimensions = -1;
static int globalContainerPositionCalculationPipeline[3] = { -1, -1, -1 };
static int globalContainerSizeCalculationPipeline[3] = { -1, -1, -1 };

static int globalUICount = 0;
static int globalUIMaxDepth = 2;

static std::array<int, DEPTH_MAX+1> depths = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

static UIContainer mainContainer =
{
	.bitfields = { MAKE_ORIENTATION(1) |MAKE_TYPE_SPECIFIC_DATA(INVISIBLE_CONTAINER) | MAKE_TYPE(0) | MAKE_DEPTH(0), 3, NO_PARENT, NOT_A_CHILD},
	.color = { 0.0, 0.0, 0.0, 0.0},
	.padding = {0, 0.0, 0.0, 0.0 },
	.relativeContainerSize = {1.0f, 1.0f},
	.absoluteSize = { 0.0, 0.0 },
	.anchorPoint = { 0.0, 0.0 },
};

static UIContainer mainLeftContainer =
{
	.bitfields = {MAKE_TYPE_SPECIFIC_DATA(ROUNDED_CORNERS | BORDERS) | MAKE_TYPE(0) | MAKE_DEPTH(1), 0, 0, 1},
	.color = { 1.0, 0.0, 0.0, 1.0},
	.padding = {10.0, 10.0, 10.0, 10.0 },
	.relativeContainerSize = {0.30f, .966f},
	.absoluteSize = { 0.0, 0.0 },
	.anchorPoint = { 0.0, 0.0 },
};

static UIContainer mainRightContainer =
{
	.bitfields = {MAKE_TYPE_SPECIFIC_DATA(ROUNDED_CORNERS | BORDERS) | MAKE_TYPE(0) | MAKE_DEPTH(1), 0, 0, 3},
	.color = { 0.0, 1.0, 0.0, 1.0},
	.padding = {10.0, 10.0, 10.0, 10.0 },
	.relativeContainerSize = {0.30f, .966f},
	.absoluteSize = { 0.0, 0.0 },
	.anchorPoint = { 0.0, 0.0 },
};

static UIContainer mainCenterContainer =
{
	.bitfields = {MAKE_TYPE_SPECIFIC_DATA(ROUNDED_CORNERS | BORDERS) | MAKE_TYPE(0) | MAKE_DEPTH(1), 0, 0, 2},
	.color = { 0.0, 0.0, 1.0, 1.0},
	.padding = {10.0, 10.0, 10.0, 10.0 },
	.relativeContainerSize = {0.30f, .966f},
	.absoluteSize = { 0.0, 0.0 },
	.anchorPoint = { 0.0, 0.0 },
};

struct WindowSize
{
	float width;
	float height;
	uint32_t totalUIContainerCount;
};

WindowSize windowSize = {
	.width = 800.0f,
	.height = 600.0f,
	.totalUIContainerCount = 1
};

static bool ExecuteCommands(const StringView& command, int argCount);
static void SetPositionOfGeometry(int geomIndex, const Vector3f& pos);
static void CreateCrateObject();
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
static void CreateBitTangentFromNormalTristrips(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator);
static int GetPoolIndexByFormat(ImageFormat format, DeviceSlabAllocator** allocator);
static int FindSMBArenaForUse(int requestedSize);
static int ReturnSMBArena(int arenaIndex);
static void LoadObject(const StringView& file);
static void LoadThreadedWrapper(StringView& file);
static int FindWords(const char* words, int charCount);
static void CreateTexturePools();
static void ProcessSMBFile(SMBFile* file, int arenaIndex);
static void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile* file, int textureBase, int arenaIndex);
static bool ProcessCommands();
static void AddCommandTS(int wordCount);
static int ReadDeferredMessageQueue(CircularMessageQueueMPSC* queue);
static void ProcessKeys(GenericKeyAction keyActions[KC_COUNT]);

static int CreateMaterial(
	int gpuMaterialID,
	int flags,
	int texturesIDs,
	int textureCount,
	const Vector4f& color
);

static int CreateMaterial(
	int gpuMaterialID,
	int flags,
	int texturesIDs,
	int textureCount,
	const Vector4f& diffuseColor,
	const Vector4f& specularColor,
	float shininess,
	const Vector4f& emissiveColor
);

static int CreateSphereDebugStruct(const Sphere& sphere, uint32_t count, const Vector4f& scale, const Vector4f& color);
static int CreateSphereDebugStruct(const Vector4f& minExtent, const Vector4f& maxExtent, uint32_t count, const Vector4f& scale, const Vector4f& color);
static int CreateSphereDebugStruct(const AxisBox& box, uint32_t count, const Vector4f& scale, const Vector4f& color);

static int CreateSphereDebugStruct(const Vector3f& center, float r, uint32_t count, const Vector4f& scale, const Vector4f& color);

static int CreateAABBDebugStruct(const AxisBox& box, const Vector4f& scale, const Vector4f& color);
static int CreateAABBDebugStruct(const Vector4f& boxMin, const Vector4f& boxMax, const Vector4f& scale, const Vector4f& color);
static int CreateAABBDebugStruct(const Vector3f& center, const Vector4f& halfExtents, const Vector4f& scale, const Vector4f& color);

static int CreateMaterialRange(int gpuMaterialRangeID, int count, int* ids);
static int CreateRenderable(int meshCPURenderableIndex, int meshGPURenderableIndex, const Matrix4f& mat, int geomIndex, int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount);

static void CreateCornerWall(float width, float height, float xDiv, float yDiv);

static int CreateSkyBox();

static int CreateMSAAPostFullScreen();

static void RecreateFrameGraphAttachments(uint32_t width, uint32_t height);

static int CreateMeshHandle(
	int meshCPUDataIndex, int meshGPUDataIndex,
	int vertexFlags, int vertexCount, int vertexStride,
	int indexStride, int indexCount,
	Sphere& sphere,
	int gpuVertexAlloc, int gpuIndexAlloc,
	int cpuVertexAlloc, int cpuIndexAlloc
);

static int CreateGPUGeometryDetails(int geometryDetailsIndex, const AxisBox& minMaxBox);
static int CreateGPUGeometryRenderable(int geomGPURenderableIndex, const Matrix4f& matrix, int geomDesc, int renderableStart, int renderableCount);
static int AllocateCPUGeometryDetails(int numberOfDetails);
static int AllocateGPUGeometryDetails(int numberOfDetails);
static int AllocateCPUMeshDetails(int numberOfDetails);
static int AllocateGPUMeshDetails(int numberOfDetails);
static int AllocateCPUMeshRenderable(int numberOfRenderables);
static int AllocateGPUMeshRenderable(int numberOfRenderables);
static int AllocateCPUGeometryInstances(int numberOfInstances);
static int AllocateGPUGeometryInstances(int numberOfInstances);
static int AllocateGPUMaterialData(int numberOfMaterials);
static int AllocateGPUMaterialRanges(int numberOfRanges);
static int AllocateGPUBlendDescriptions(int numberOfDescs);
static int AllocateGPUBlendRanges(int numberOfRanges);
static void CreateJointVisualData();
static void CreateJointVisualObject(int numberOfJoints, uint32_t startingLocation);
static void UpdateCameraMatrix();
static bool MoveCamera(double fps);
static void CreateGPUGenericObjects();

static void CreateUITools(int maxUIElements, int maxUIContainers);

ApplicationLoop::ApplicationLoop(ProgramArgs& _args) :
	args(_args),
	running(true),
	cleaned(true)
{
	Execute();
}

ApplicationLoop::~ApplicationLoop() { 
	if (!cleaned) 
	{ 
		CleanupRuntime(); 
	} 
	CloseAllFiles();
	CloseAllSyncObject();
	CloseAllThreads();
	CloseAllWindows();
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

	mainAppLogger.InitLogger(LoggerMessageMemory, sizeof(LoggerMessageMemory));

	OSGetSTDOutput(&mainAppLogger.fileHandle);

	StringView mainInputString = AppInstanceTempAllocator.AllocateFromNullStringCopy(args.inputFile.string().c_str());

	if (args.justexport)
	{
		SMBFile mainSMB{};

		int fileSize = mainSMB.OpenFile(mainInputString);

		if (fileSize <= 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot open SMB file for export"));
			mainAppLogger.ProcessMessage();
			return;
		}

		int arenaIndex = FindSMBArenaForUse(fileSize);

		int loadSMBRet = mainSMB.LoadFile(&SMBThreadedFileInputAllocators[arenaIndex], &mainAppLogger);

		if (loadSMBRet)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot read SMB file for export"));
			mainAppLogger.ProcessMessage();
			return;
		}

		StringView fileName{};

		char* strBuf = (char*)SMBThreadedFileScratchAllocators[arenaIndex].Allocate(mainInputString.charCount);

		FileManager::ExtractFileNameFromPath(&mainInputString, &fileName, strBuf);

		FileManager::SetFileCurrentDirectory(&fileName);

		ExportChunksFromFile(&mainSMB, &GlobaScratchAllocator);

		ReturnSMBArena(arenaIndex);
	}
	else
	{
		InitializeRuntime();

		meshCPUData.Create(&GlobaScratchAllocator, globalMeshCountMax);

		geometryCPUData.Create(&GlobaScratchAllocator, globalGeometryDescriptionsCountMax);

		renderablesGeomObjects.Create(&GlobaScratchAllocator, globalGeometryRenderableCount);

		renderablesMeshObjects.Create(&GlobaScratchAllocator, globalRenderableMax);

		cleaned = false;

		CreateGPUGenericObjects();

		CreateCrateObject();

		CreateCornerWall(10.0f, 10.0f, 2.0f, 1.0f);

		CreateJointVisualData();

		LoadObject(mainInputString);

		CreateUniformGrid();

		LARGE_INTEGER startTime;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER frequency;

		uint64_t frameCounter = 0;
		double FPS = 60.0f;

		char windowText[32];

		StringView view { windowText, 0 };

		auto fps = [&frameCounter, &currentTime, &startTime, &frequency, &FPS, &windowText, &view, this]()
			{
				double elapsed;
				QueryPerformanceCounter(&currentTime);

				elapsed = static_cast<double>((currentTime.QuadPart - startTime.QuadPart)) / frequency.QuadPart;

				if (elapsed >= 1.0) {
					FPS = static_cast<double>(frameCounter) / elapsed;
			
					view.charCount = snprintf(windowText, sizeof(windowText), "FPS : %.2f", FPS);

					mainWindow.SetWindowTitle(view);
				
					frameCounter = 0;
					QueryPerformanceCounter(&startTime);
					return 1;
				}

				return 0;
			};

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startTime);

		uint32_t framesInFlight = GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT;

		int updateMainDrawCommand = 0, updateMainDebugCommand = 0, updateLights = framesInFlight, updateShadowMap = 0, updateUI = 0;

		int mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;

		int mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;

		int previousGlobalLightCount = globalLightCount;

		int localUICount = 0;

		GlobalRenderer::gRenderInstance.AddCommandQueue(mainCommandStreamIndex, mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance.AddCommandQueue(mainCommandStreamIndex, currentFrameGraphIndex, ATTACHMENT_COMMANDS);

		DeviceHandleArrayUpdate samplerUpdate;

		samplerUpdate.updateType = DeviceHandleArrayUpdateType::SAMPLER_UPDATE;
		samplerUpdate.resourceCount = 1;
		samplerUpdate.resourceDstBegin = 0;
		samplerUpdate.resourceHandles = &mainLinearSampler;

		GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 0, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

		while (running)
		{
			mainWindow.PollEvents();

			if (mainWindow.ShouldCloseWindow()) break;

			ProcessKeys(mainWindow.windowData.info.actions);

			if (mainWindow.windowData.info.HandleResizeRequested())
			{
				int width = 0, height = 0;

				mainWindow.GetWindowSize(&width, &height);

				updateUI = framesInFlight;

				windowSize.height = height ? (float)height : 1.0;
				windowSize.width = width ? (float)width : 1.0;

				GlobalRenderer::gRenderInstance.RecreateSwapChain(mainLogicalDevice, mainPresentationSwapChain, (uint32_t)width, (uint32_t)height);
				c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));
				UpdateCameraMatrix();
				RecreateFrameGraphAttachments(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain), GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain));
				continue;
			}

			mainIndirectDrawData.commandBufferCount += ReadDeferredMessageQueue(&RenderableAllocQueue);

			debugIndirectDrawData.commandBufferCount += ReadDeferredMessageQueue(&DebugAllocQueue);

			bool cameraMove = MoveCamera(FPS);

			if (previousGlobalLightCount != globalLightCount || updateLights)
			{
				
				if (!updateLights)
				{
					updateLights = framesInFlight;
				}

				if (previousGlobalLightCount != globalLightCount)
				{
					GlobalRenderer::gRenderInstance.InsertTransferCommand(lightAssignment.deviceCountsAlloc, lightAssignment.totalElementsCount * sizeof(uint32_t), 0, 0);
				
					previousGlobalLightCount = globalLightCount;
				}

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.preWorldSpaceDivisionPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.prefixSumPipeline);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, lightAssignment.postWorldSpaceDivisionPipeline);

				updateLights--;
			
			}

			if (mainDrawRenderableCount != mainIndirectDrawData.commandBufferCount || updateShadowMap)
			{
				
				if (!updateShadowMap)
				{
					updateShadowMap = framesInFlight;
					GlobalRenderer::gRenderInstance.InsertTransferCommand(mainShadowMapManager.shadowMapObjectCountAlloc, 8, 0, 0);
				}

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, mainShadowMapManager.shadowClippingPipeline);

				updateShadowMap--;
			}

			if (cameraMove || mainDrawRenderableCount != mainIndirectDrawData.commandBufferCount || updateMainDrawCommand)
			{

				if (!updateMainDrawCommand || cameraMove) 
				{
					updateMainDrawCommand = framesInFlight;
					GlobalRenderer::gRenderInstance.InsertTransferCommand(mainIndirectDrawData.commandBufferCountAlloc, 8, 0, 0);
				}

				if (mainDrawRenderableCount != mainIndirectDrawData.commandBufferCount)
				{
					if (updateMainDrawCommand == framesInFlight)
						GlobalRenderer::gRenderInstance.InsertTransferCommand(worldSpaceAssignment.deviceCountsAlloc, worldSpaceAssignment.totalElementsCount * sizeof(uint32_t), 0, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.preWorldSpaceDivisionPipeline);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.prefixSumPipeline);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, worldSpaceAssignment.postWorldSpaceDivisionPipeline);

					if (updateMainDrawCommand == 1)
						mainDrawRenderableCount = mainIndirectDrawData.commandBufferCount;
				}

				updateMainDrawCommand--;

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, mainIndirectDrawData.indirectCullPipeline);
			}

			if (cameraMove || debugIndirectDrawData.commandBufferCount != mainDebugDrawRenderableCount || updateMainDebugCommand)
			{
			
				if (!updateMainDebugCommand || cameraMove)
				{
					GlobalRenderer::gRenderInstance.InsertTransferCommand(debugIndirectDrawData.commandBufferCountAlloc, 8, 0, 0);
					updateMainDebugCommand = framesInFlight;
				}
				mainDebugDrawRenderableCount = debugIndirectDrawData.commandBufferCount;
				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, debugIndirectDrawData.indirectCullPipeline);
				updateMainDebugCommand--;
			}

			if (localUICount != globalUICount || updateUI > 0)
			{
				
				if (!updateUI)
					updateUI = framesInFlight;

				if (updateUI == framesInFlight)
					GlobalRenderer::gRenderInstance.InsertTransferCommand(globalUIElementsIndirectCountBuffer, sizeof(uint32_t), 0, 0);
				
				if (localUICount != globalUICount)
				{
					if (updateUI == framesInFlight)
					{
						GlobalRenderer::gRenderInstance.InsertTransferCommand(globalDepthCounts , DEPTH_MAX*sizeof(uint32_t), 0, 0);
						GlobalRenderer::gRenderInstance.InsertTransferCommand(globalDepthOffsets , DEPTH_MAX*sizeof(uint32_t), 0, 0);
					}

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalUICountPipeline);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalUIPrefixSumPipeline);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalUIChildDepthAddPipeline);

					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalUIIndexAssignmentPipeline);

					if (updateUI == 1)
					{
						localUICount = globalUICount;
					}
				}

				for (int i = 0; i < globalUIMaxDepth; i++)
				{
					GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalContainerSizeCalculationPipeline[i]);
				}

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalContainerPositionCalculationPipeline[1]);

				GlobalRenderer::gRenderInstance.AddPipelineToComputeQueue(mainComputeQueueIndex, globalUICullPipelineIndex);
				
				updateUI--;

			}

			swcImageIndex = GlobalRenderer::gRenderInstance.BeginFrame(mainLogicalDevice, mainPresentationSwapChain);

			

			if (swcImageIndex != ~0UL)
			{
				if (currentFrameGraphIndex == MSAAShadowMapping)
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(smdpd.shadowMapPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 1);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 1);

					for (int jointPipeline = 0; jointPipeline < jointMeshPipelinesCount; jointPipeline++)
					{
						GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(jointMeshPipelines[jointPipeline], currentFrameGraphIndex, 1);
					}

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(globalUIDrawingPipelineIndex, currentFrameGraphIndex, 1);
				}
				else if (currentFrameGraphIndex == BasicShadow)
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(smdpd.fullScreenPipeline, currentFrameGraphIndex, 0);
				}
				else 
				{
					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(skyboxPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(debugIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 0);

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainIndirectDrawData.indirectDrawPipeline, currentFrameGraphIndex, 0);

					for (int jointPipeline = 0; jointPipeline < jointMeshPipelinesCount; jointPipeline++)
					{
						GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(jointMeshPipelines[jointPipeline], currentFrameGraphIndex, 0);
					}

					GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(globalUIDrawingPipelineIndex, currentFrameGraphIndex, 0);

					if (currentFrameGraphIndex == MSAAPost)
						GlobalRenderer::gRenderInstance.AddPipelineToRPGraphicsQueue(mainFullScreenPipeline, currentFrameGraphIndex, 1);
				}

				GlobalRenderer::gRenderInstance.DrawScene(mainLogicalDevice, mainCommandStreamIndex, swcImageIndex);

				GlobalRenderer::gRenderInstance.SubmitFrame(mainLogicalDevice, mainPresentationSwapChain, swcImageIndex);

				GlobalRenderer::gRenderInstance.EndFrame(mainCommandStreamIndex, mainLogicalDevice);
		
				running = ProcessCommands();

			}
			else
			{
				mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Missed image handle during drawing, closing app"));
				running = false;
			}			

			fps();

			frameCounter++;

			mainAppLogger.ProcessMessage();
		}
	}
}

void CreateTexturePools()
{
	std::array<ImageFormat, 4> formats = {
		ImageFormat::DXT1,
		ImageFormat::DXT3,
		ImageFormat::B8G8R8A8,
		ImageFormat::B8G8R8A8_UNORM
	};

	auto& rendInst = GlobalRenderer::gRenderInstance;

	size_t requestedSize = 128 * MiB;

	for (int i = 0; i < 4; i++)
	{
		int texturePoolHandle = 
			rendInst.CreateImagePool(mainLogicalDevice,
			requestedSize,
			formats[i], MAX_IMAGE_DIM, MAX_IMAGE_DIM, 
				ImageUsageFlagBits::TRANSFER_SRC | ImageUsageFlagBits::TRANSFER_DEST
				| ImageUsageFlagBits::SAMPLED | (i > 2 ? ImageUsageFlagBits::STORAGE : 0),
				MemoryTypeBits::DEVICE_MEMORY_TYPE
		);

		if (texturePoolHandle < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Failed to create a texture image pool"));
		}

		mainDictionary.texturePoolsFormat[i] = formats[i];

		mainDictionary.CreatePoolAllocator(i, requestedSize);

		mainDictionary.texturePoolHandle[i] = texturePoolHandle;
	}
}

bool MoveCamera(double fps)
{
	bool moved = false;

	float stepfactor = 15.0f / static_cast<float>(fps);

	double angleFactor = 15.0 / fps;

	if (camMovements[RIGHT])
	{
		c.ltm.MoveRight(stepfactor);
		moved = true;
	}

	if (camMovements[LEFT])
	{
		c.ltm.MoveRight(-stepfactor);
		moved = true;
	}

	if (camMovements[FORWARD])
	{
		c.ltm.MoveForward(-stepfactor);
		moved = true;
	}

	if (camMovements[BACK])
	{
		c.ltm.MoveForward(stepfactor);
		moved = true;
	}

	if (camMovements[PITCHDOWN])
	{
		c.ltm.PitchLTM(-angleFactor);
		moved = true;
	}

	if (camMovements[PITCHUP])
	{
		c.ltm.PitchLTM(angleFactor);
		moved = true;
	}

	if (camMovements[ROTATEYLEFT])
	{
		c.ltm.RotateAroundUp(angleFactor);
		moved = true;
	}

	if (camMovements[ROTATEYRIGHT])
	{
		c.ltm.RotateAroundUp(-angleFactor);
		moved = true;
	}

	if (moved) UpdateCameraMatrix();

	return moved;
}

void UpdateCameraMatrix()
{
	c.UpdateCamera();

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&c.View, globalBufferLocation, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 0, TransferType::MEMORY);
}

int GetPoolIndexByFormat(ImageFormat format, DeviceSlabAllocator** allocator)
{
	int ret = -1;
	switch (format)
	{
	case ImageFormat::DXT1:
		ret = mainDictionary.texturePoolHandle[0];
		*allocator = &mainDictionary.texturePoolAllocators[0];
		break;
	case ImageFormat::DXT3:
		ret = mainDictionary.texturePoolHandle[1];
		*allocator = &mainDictionary.texturePoolAllocators[1];
		break;
	case ImageFormat::B8G8R8A8:
		ret = mainDictionary.texturePoolHandle[2];
		*allocator = &mainDictionary.texturePoolAllocators[2];
		break;
	case ImageFormat::B8G8R8A8_UNORM:
		ret = mainDictionary.texturePoolHandle[3];
		*allocator = &mainDictionary.texturePoolAllocators[3];
		break;
	default:
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("unhandled texture format"));
		break;
	}
	return ret;
}

void CreateCornerWall(float width, float height, float xDiv, float yDiv)
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

	xDiv = MAX(xDiv, 1.0f);
	yDiv = MAX(yDiv, 1.0f);

	int vertexCount = (int)(xDiv+1.0f) * (int)(yDiv+1.0f);

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

	Matrix4f sideFrontPanel = CreateRotationMatrixMat4(Vector3f(0.0f, 0.0f, 1.0), DegToRad(-90.0f));

	sideFrontPanel.axes.translate = Vector4f(10.0f, 3.0, -5.0, 1.0);

	Matrix4f backPanel = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0), DegToRad(-90.0f));

	backPanel.axes.translate = Vector4f(5.0f, 3.0, -10.0, 1.0);

	Matrix4f floorPanel = Identity4f();

	floorPanel.axes.translate = { 5.0f, -2.0, -5.0, 1.0 };

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

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * vertexCount, 16);

	int indexAlloc = indexBufferAlloc.Allocate(indexCount * sizeof(uint16_t), 16);

	if (geomDetailsIndex < 0 || 
		meshGPUIndex < 0 || 
		meshCPUIndex < 0 || 
		geomRenderableCPUDataIndex < 0 || 
		gpuGeomRenderable < 0 || 
		gpuMeshRendrables < 0 || 
		cpuMeshRenderable < 0 ||
		materialHandleIndex < 0 ||
		materialRangeIndex < 0 ||
		indexAlloc < 0 ||
		vertexAlloc < 0
	)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot create corner object"));
		mainAppLogger.ProcessMessage();
		return;
	}

	CreateGPUGeometryDetails(geomDetailsIndex, box);

	CreateMeshHandle(
		meshCPUIndex, meshGPUIndex, 
		vertexFlags, vertexCount, compressedSize, 
		2, indexCount, 
		sphere, 
		vertexAlloc, indexAlloc,
		-1, -1
	);

	rendInst.UpdateDriverMemory(compPoses, globalVertexBuffer, vertexCount * compressedSize, vertexAlloc, TransferType::CACHED);
	rendInst.UpdateDriverMemory(indices, globalIndexBuffer, indexCount * 2, indexAlloc, TransferType::CACHED);

	CreateMaterial(materialHandleIndex, VERTEXNORMAL, -1, 0, Vector4f(1.0, 0.0, 0.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0));

	CreateMaterialRange(materialRangeIndex, 1, &materialHandleIndex);

	RenderableGeomCPUData* geomRendCPUData = renderablesGeomObjects.Get(geomRenderableCPUDataIndex);

	geomRendCPUData->renderableMeshStart = gpuMeshRendrables;

	geomRendCPUData->renderableMeshCount = 3;

	geomRendCPUData->geomIndex = gpuGeomRenderable;

	CreateGPUGeometryRenderable(gpuGeomRenderable, Identity4f(), geomDetailsIndex, geomRendCPUData->renderableMeshStart, geomRendCPUData->renderableMeshCount);

	CreateRenderable(cpuMeshRenderable, gpuMeshRendrables, floorPanel, gpuGeomRenderable, materialRangeIndex, 1, DEFAULT_GPU_ITEM_ARRAY_INDEX, meshGPUIndex, 1);

	CreateRenderable(cpuMeshRenderable + 1, gpuMeshRendrables + 1, sideFrontPanel, gpuGeomRenderable, materialRangeIndex, 1, DEFAULT_GPU_ITEM_ARRAY_INDEX, meshGPUIndex, 1);
	
	CreateRenderable(cpuMeshRenderable + 2, gpuMeshRendrables +2, backPanel, gpuGeomRenderable, materialRangeIndex, 1, DEFAULT_GPU_ITEM_ARRAY_INDEX, meshGPUIndex, 1);

	mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Created Corner Object"));

	return;
}

void CreateJointVisualData()
{
	uint16_t Indices[24] = {
		// bottom pyramid (root to base)
		0, 1, 2,
		0, 2, 3,
		0, 3, 4,
		0, 4, 1,
		// top pyramid (base to tip)
		5, 2, 1,
		5, 3, 2,
		5, 4, 3,
		5, 1, 4,
	};

	struct MyVertex
	{
		Vector4f pos;
		Vector4f color;
	};

	MyVertex Verts[6] =
	{
		{ Vector4f(0.0f, 0.0f, 0.0f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
		{ Vector4f(0.1f, 0.2f, 0.1f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
		{ Vector4f(-0.1f, 0.2f, 0.1f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
		{ Vector4f(-0.1f, 0.2f, -0.1f, 1.0f), Vector4f(1.0f, 0.0f, 1.0f, 1.0f) },
		{ Vector4f(0.1f, 0.2f, -0.1f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
		{ Vector4f(0.0f, 1.0f,  0.0f, 1.0f), Vector4f(1.0f, 1.0f, 0.0f, 1.0f) },
	};


	auto& rendInst = GlobalRenderer::gRenderInstance;

	int vertexAlloc = jointMeshVertexAlloc = rendInst.CreateSuballocation(mainLogicalDevice, globalVertexBuffer, sizeof(Verts), 1, 64, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &vertexBufferAlloc);
	int indexAlloc = jointMeshIndexAlloc = rendInst.CreateSuballocation(mainLogicalDevice, globalIndexBuffer, sizeof(Indices), 1, 64, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &indexBufferAlloc);

	/*
	if (indexAlloc < 0 ||
		vertexAlloc < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot create joint object"));
		mainAppLogger.ProcessMessage();
		return;
	}

	*/

	rendInst.UpdateDriverMemory(Verts, jointMeshVertexAlloc, sizeof(Verts), 0, TransferType::CACHED);
	rendInst.UpdateDriverMemory(Indices, jointMeshIndexAlloc, sizeof(Indices), 0, TransferType::CACHED);

	mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Created Joint Object"));
}

void CreateJointVisualObject(int numberOfJoints, uint32_t startingLocation)
{
	if (jointMeshPipelinesCount >= MAX_JOINT_VISUALIZERS)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many joint visualizer objects"));
		return;
	}

	ShaderResourceSetContext lContext{ &mainAppLogger, false };

	int jointIndex = jointMeshPipelinesCount++;

	jointMeshStaringLocations[jointIndex] = startingLocation;

	ShaderResourceSetBuilder camJointData = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, JOINTVISUAL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	ShaderResourceSetBuilder JointDesc = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, JOINTVISUAL, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	camJointData.BindBufferToShaderResource(&lContext, &globalBufferLocation, 0, 1, 0);
	JointDesc.UploadConstant(&lContext, &jointMeshStaringLocations[jointIndex], 0);
	JointDesc.BindBufferToShaderResource(&lContext,  &jointMeshWorldMatrix, 0, 1, 0);
	JointDesc.BindBufferView(&lContext, &jointMeshParentIndices, 0, 1, 1);

	if (lContext.contextFailed)
	{
		lContext.contextLogger->ProcessMessage();
		return;
	}

	std::array<ShaderResourceSetHandle, 2> Descs = { camJointData(),
													JointDesc() };

	GraphicsIntermediaryPipelineInfo jointInfo = {
		.drawType = 0,
		.vertexBufferHandle = jointMeshVertexAlloc,
		.vertexCount = 6,
		.pipelinename = JOINTVISUAL,
		.descCount = 2,
		.descriptorsetid = Descs.data(),
		.indexBufferHandle = jointMeshIndexAlloc,
		.indexCount = 32,
		.instanceCount = (uint32_t)numberOfJoints,
		.indexSize = 2,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	jointMeshPipelines[jointIndex] = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &jointInfo);
}


void CreateCrateObject()
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

	CreateBitTangentFromNormalTristrips(BoxVerts, texturesCoordinate, BoxIndices, 52, 24, tangents, totalNormals, &AppInstanceTempAllocator);

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

	crateMatrix.axes.translate = Vector4f(-10.0, 5.0, 0.0f, 1.0f);

	Sphere sphere;

	sphere.sphere = Vector4f(0.0, 0.0, 0.0, 1.5f);

	StringView blendname = STRING_VIEW_FROM_LITERAL("blendmap.bmp");

	StringView skyname = STRING_VIEW_FROM_LITERAL("sky.bmp");

	StringView normalmapname = STRING_VIEW_FROM_LITERAL("WNN2.bmp");

	StringView albedomapname = STRING_VIEW_FROM_LITERAL("WNN.bmp");

	int albedoMapped = Read2DImage(&albedomapname, 1, BMP);
	int normalMapped = Read2DImage(&normalmapname, 1, BMP);
	int blendMapped = Read2DImage(&blendname, 1, BMP);
	int skymapped = Read2DImage(&skyname, 1, BMP);

	if (skymapped < 0 || albedoMapped < 0 || normalMapped < 0 || blendMapped < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot create crate object"));
		mainAppLogger.ProcessMessage();
		return;
	}

	int materialRangeCount = 2;

	int geomDetailsIndex = -1, meshGPUIndex = -1, meshCPUIndex = -1;

	geomDetailsIndex = AllocateGPUGeometryDetails(1);

	meshGPUIndex = AllocateGPUMeshDetails(1);

	meshCPUIndex = AllocateCPUMeshDetails(1);

	int cpuGeomRenderable = AllocateCPUGeometryInstances(1);

	int gpuGeomRenderable = AllocateGPUGeometryInstances(1);

	int gpuMeshRenderable = AllocateGPUMeshRenderable(1);

	int cpuMeshRenderable = AllocateCPUMeshRenderable(1);

	int materialHandleIndex = AllocateGPUMaterialData(materialRangeCount);

	int materialRangeIndex = AllocateGPUMaterialRanges(materialRangeCount);

	int blendDetailsIndex = AllocateGPUBlendDescriptions(2);

	int blendRangesIndex = AllocateGPUBlendRanges(2);

	int vertexAlloc = vertexBufferAlloc.Allocate(compressedSize * 24, 16);
	int indexAlloc = indexBufferAlloc.Allocate(sizeof(BoxIndices), 64);

	if (geomDetailsIndex < 0 || 
		meshGPUIndex < 0 || 
		meshCPUIndex < 0 || 
		cpuGeomRenderable < 0 || 
		gpuGeomRenderable < 0 || 
		gpuMeshRenderable < 0 || 
		cpuMeshRenderable < 0 || 
		materialHandleIndex < 0 ||
		materialRangeIndex < 0 ||
		blendDetailsIndex < 0 ||
		blendRangesIndex < 0 ||
		vertexAlloc < 0 ||
		indexAlloc < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot create crate object"));
		mainAppLogger.ProcessMessage();
		return;
	}

	CreateBlendDetails(blendDetailsIndex, BlendMaterialType::ConstantAlpha, 1.0f);
	CreateBlendDetails(blendDetailsIndex + 1, BlendMaterialType::BlendMap, blendMapped);

	std::array blendIndices = { blendDetailsIndex, blendDetailsIndex + 1 };

	CreateBlendRange(blendRangesIndex, blendIndices.data(), 2);

	std::array textureHandles = { albedoMapped, normalMapped, skymapped };

	CreateMaterial(materialHandleIndex, ALBEDOMAPPED | TANGENTNORMALMAPPED, albedoMapped, 2, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.25, .25, .25, 1.0), 3.0, Vector4f(.04, .06, 0.08, 1.0));

	CreateMaterial(materialHandleIndex + 1, ALBEDOMAPPED, skymapped, 1, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(.80, .80, .80, 1.0), 256.0, Vector4f(.04, .06, 0.08, 1.0));

	std::array<int, 2> materialIDs = { materialHandleIndex, materialHandleIndex + 1 };

	CreateMaterialRange(materialRangeIndex, materialRangeCount, materialIDs.data());

	CreateGPUGeometryDetails(geomDetailsIndex, BOX);
	
	CreateMeshHandle(
		meshCPUIndex, meshGPUIndex,
		vertexFlags, 24, 
		compressedSize, 
		2, 52, 
		sphere, 
		vertexAlloc, indexAlloc,
		-1, -1
	);

	RenderableGeomCPUData* rendGeomCPUData = renderablesGeomObjects.Get(cpuGeomRenderable);

	rendGeomCPUData->renderableMeshStart = gpuMeshRenderable;

	rendGeomCPUData->renderableMeshCount = 1;

	rendGeomCPUData->geomIndex = gpuGeomRenderable;

	CreateGPUGeometryRenderable(gpuGeomRenderable, crateMatrix, geomDetailsIndex, rendGeomCPUData->renderableMeshStart, rendGeomCPUData->renderableMeshCount);

	CreateRenderable(cpuMeshRenderable, gpuMeshRenderable, Identity4f(), gpuGeomRenderable, materialRangeIndex, materialRangeCount, blendRangesIndex, meshGPUIndex, 1);
	
	rendInst.UpdateDriverMemory(compressedVertexData, globalVertexBuffer, compressedSize * 24,  vertexAlloc, TransferType::CACHED);
	rendInst.UpdateDriverMemory(BoxIndices, globalIndexBuffer,  sizeof(BoxIndices),  indexAlloc, TransferType::CACHED);

	mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Created Crate Object"));
}

void SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile* file, int textureBase, int arenaIndex)
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

	if (geomDetailsIndex < 0 || 
		meshGPUIndex < 0 || 
		meshCPUIndex < 0 || 
		cpuGeomRenderable < 0 || 
		gpuGeomRenderable < 0 || 
		gpuMeshRenderable < 0 || 
		cpuMeshRenderable < 0 ||
		materialHandleIndex < 0 ||
		materialRangeIndex < 0 ||
		blendDetailsIndex < 0 ||
		blendRangesIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot smb object"));
		mainAppLogger.ProcessMessage();
		return;
	}

	CreateBlendDetails(blendDetailsIndex, BlendMaterialType::ConstantAlpha, 1.0f);

	CreateBlendRange(blendRangesIndex, &blendDetailsIndex, 1);

	CreateGPUGeometryDetails(geomDetailsIndex, geoDef->axialBox);

	RenderableGeomCPUData* rendGeomCPUData = renderablesGeomObjects.Get(cpuGeomRenderable);

	rendGeomCPUData->renderableMeshStart = gpuMeshRenderable;
	
	rendGeomCPUData->renderableMeshCount = meshCount;

	rendGeomCPUData->geomIndex = gpuGeomRenderable;

	int base = textureBase;

	Matrix4f geomSpecificData;

	geomSpecificData = Identity4f();

	geomSpecificData.axes.translate = Vector4f(0.f, 0.f, 0.f, 1.0f);

	Matrix4f rotation = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0f), DegToRad(90.0f));

	geomSpecificData = geomSpecificData * rotation;

	CreateGPUGeometryRenderable(gpuGeomRenderable, geomSpecificData, geomDetailsIndex, gpuMeshRenderable, meshCount);

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
		default:
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Unhandled vertex type"));
			break;
		}

		vertexData = (void*)vertexAndIndicesAlloc.Allocate(vertexSize * vertexCount);

		SMBCopyVertexData(geoDef, i, file, vertexData, decompressed, &SMBThreadedFileInputAllocators[arenaIndex]);

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
		
		int textureStart = base;
		int textureCount = geoDef->materialsCount[i];

		base += textureCount;

		CreateMaterial(
			materialHandleIndex + i,
			ALBEDOMAPPED | ((textureCount > 1) ? WORLDNORMALMAPPED : type == PosPack6_C16Tex1_Bone2 ? 0 : VERTEXNORMAL), 
			textureStart, 
			textureCount, 
			Vector4f(1.0, 1.0, 1.0, 1.0), 
			Vector4f(.1, .1, .1, 1.0), 1.5, 
			Vector4f(0.0, 0.0, 0.0, 1.0)
		);

		int outMaterialHandle = materialHandleIndex + i;
		int materialRangeCount = 1;
		
		CreateMaterialRange(materialRangeIndex + i, materialRangeCount, &outMaterialHandle);
		
		CreateMeshHandle(
			meshCPUIndex + i, meshGPUIndex + i,
			vertexFlags, vertexCount, 
			vertexSize, 2, 
			indexCount,
			geoDef->spheres[i], 
			vertexAlloc, indexAlloc,
			vertexAndIndicesAlloc.OffsetInAllocator(vertexData), vertexAndIndicesAlloc.OffsetInAllocator(indices)
		);

		CreateRenderable(cpuMeshRenderable + i, gpuMeshRenderable + i, Identity4f(), gpuGeomRenderable, materialRangeIndex + i, materialRangeCount, blendRangesIndex, meshGPUIndex + i, 1);
	}

	CreateSphereDebugStruct(geoDef->axialBox, 24, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(0.0, 1.0, 0.0, 0.0));

	CreateAABBDebugStruct(geoDef->axialBox, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.5, 0.5, 1.0, 0.0));

	mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Created SMB Object"));
}


int CreateSphereDebugStruct(const Sphere& sphere, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	return CreateSphereDebugStruct(Vector3f(sphere.sphere.x, sphere.sphere.y, sphere.sphere.z), sphere.sphere.w, count, scale, color);
}

int CreateSphereDebugStruct(const AxisBox& box, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	return CreateSphereDebugStruct(box.min, box.max, count, scale, color);
}

int CreateSphereDebugStruct(const Vector4f& minExtent, const Vector4f& maxExtent, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	Vector4f center = ((maxExtent - minExtent) * 0.5 + (minExtent));

	Vector4f half = ((maxExtent - minExtent) * 0.5);
	Vector3f half3 = Vector3f(half.x, half.y, half.z);
	float r = Length(half3);

	return CreateSphereDebugStruct(Vector3f(center.x, center.y, center.z), r, count, scale, color);
}

int CreateSphereDebugStruct(const Vector3f& center, float r, uint32_t count, const Vector4f& scale, const Vector4f& color)
{
	GPUDebugRenderable drawStruct;
	DebugDrawType type = DebugDrawType::DEBUGSPHERE;

	int debugStructLocation = -1;

	if (((debugStructLocation = globalDebugStructCount.fetch_add(1)) + 1) >= globalDebugStructMaxCount)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many debug structs created"));
		return -1;
	}

	int* allocQueueWrite = (int*)DebugAllocQueue.AcquireWrite(sizeof(int));

	*allocQueueWrite = 1;

	drawStruct.center = Vector4f(center.x, center.y, center.z, 1.0f);

	drawStruct.scale = scale;
	drawStruct.color = color;

	drawStruct.halfExtents = Vector4f(r, std::bit_cast<float, uint32_t>(count), 1.0, 1.0);

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&type,  globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

	return debugStructLocation;
}

int CreateAABBDebugStruct(const AxisBox& box, const Vector4f& scale, const Vector4f& color)
{
	Vector4f half = ((box.max - box.min) * 0.5);

	Vector4f center = ((box.max - box.min) * 0.5 + (box.min));

	return CreateAABBDebugStruct(Vector3f(center.x, center.y, center.z), half, scale, color);
}

int CreateAABBDebugStruct(const Vector4f& boxMin, const Vector4f& boxMax, const Vector4f& scale, const Vector4f& color)
{
	Vector4f half = ((boxMax - boxMin) * 0.5);;

	Vector4f center = ((boxMax - boxMin) * 0.5 + (boxMin));
	
	return CreateAABBDebugStruct(Vector3f(center.x, center.y, center.z), half, scale, color);
}

int CreateAABBDebugStruct(const Vector3f& center, const Vector4f& halfExtents, const Vector4f& scale, const Vector4f& color)
{
	DebugDrawType type = DebugDrawType::DEBUGBOX;
	GPUDebugRenderable drawStruct;

	int debugStructLocation = -1;

	if (((debugStructLocation = globalDebugStructCount.fetch_add(1)) + 1) >= globalDebugStructMaxCount)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many debug structs created"));
		return -1;
	}


	int* allocQueueWrite = (int*)DebugAllocQueue.AcquireWrite(sizeof(int));

	*allocQueueWrite = 1;
	
	drawStruct.center = Vector4f(center.x , center.y, center.z, 1.0f);
	drawStruct.scale = scale;
	drawStruct.color = color;
	drawStruct.halfExtents = halfExtents;

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&drawStruct, globalDebugStructAlloc, sizeof(GPUDebugRenderable), sizeof(GPUDebugRenderable) * debugStructLocation, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&type, globalDebugTypesAlloc, sizeof(DebugDrawType), sizeof(DebugDrawType) * debugStructLocation, TransferType::CACHED);

	return debugStructLocation;
}

void ProcessSMBFile(SMBFile *file, int arenaIndex)
{
	uint64_t readCount = 0;

	const int MAX_GEO_FILES = 2;
	const int MAX_TEXTURES = 10;

	int totalTextureCount = 0, totalMeshCount = 0, totalSkeletonCount = 0;

	auto& chunk = file->chunks;

	SMBTexture* textures = (SMBTexture*)SMBThreadedFileInputAllocators[arenaIndex].CAllocate(sizeof(SMBTexture) * MAX_TEXTURES);

	SMBGeoChunk* geoDefs = (SMBGeoChunk*)SMBThreadedFileInputAllocators[arenaIndex].Allocate(sizeof(SMBGeoChunk) * MAX_GEO_FILES);

	SMBSkeleton* skels = (SMBSkeleton*)SMBThreadedFileInputAllocators[arenaIndex].Allocate(sizeof(SMBSkeleton) * MAX_GEO_FILES);;

	for (size_t i = 0; i<file->numResources; i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{
			SMBGeoChunk* geoDef = &geoDefs[totalMeshCount];

			size_t seekpos = chunk[i].offsetInHeader;

			OSSeekFile(&file->fileHandle, seekpos, BEGIN);

			char* geomHeader = (char*)SMBThreadedFileInputAllocators[arenaIndex].Allocate(chunk[i].headerSize);

			OSReadFile(&file->fileHandle, chunk[i].headerSize, geomHeader, &readCount);

		    int geometryPorcessRet = ProcessGeometryClass(geomHeader, totalTextureCount, geoDef, chunk[i].contigOffset + file->fileOffset, chunk[i].fileOffset + file->numContiguousBytes + file->fileOffset, &mainAppLogger, &SMBThreadedFileInputAllocators[arenaIndex]);

			if (geometryPorcessRet < 0)
			{
				mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, STRING_VIEW_FROM_LITERAL("Geometry failed from SMB failed loading"));
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
				mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, STRING_VIEW_FROM_LITERAL("Texture from SMB failed loading"));
				break;
			}
			totalTextureCount++;
			break;
		}
		
		case Joints:
		{
			SMBSkeleton* skel = &skels[totalSkeletonCount];

			char* jointStrData = GetJointNames(&SMBThreadedFileInputAllocators[arenaIndex], file, &chunk[i]);

			int skelCode = GetBoneData(&SMBThreadedFileInputAllocators[arenaIndex], skel, file);

			int successStringOffset = GetStringOffset(file, skel);

			int startingJointLocation = jointMeshWorldMatrixCount;

			Matrix4f* worldMats = (Matrix4f*)SMBThreadedFileInputAllocators[arenaIndex].Allocate(sizeof(Matrix4f) * skel->jointCount);

			Matrix4f geomRotation = CreateRotationMatrixMat4(Vector3f(1.0f, 0.0f, 0.0f), DegToRad(90.0f));
			
			for (int joint = 0; joint < skel->jointCount; joint++)
			{
				Matrix4f jointRot = CreateRotationMatFromQuat(skel->joints[joint].granny_orientation);
				
				Matrix4f scale = CreateScaleMatrix(skel->joints[joint].granny_scale);

				Matrix4f translation = Identity4f();

				translation.axes.translate.x = skel->joints[joint].granny_position.x;
				translation.axes.translate.y = skel->joints[joint].granny_position.y;
				translation.axes.translate.z = skel->joints[joint].granny_position.z;

				uint32_t parentIdx = skel->joints[joint].granny_parentIndex;

				Matrix4f worldMat;

				if (parentIdx == 0xFFFFFFFF) {
					worldMat = geomRotation * translation *  jointRot * scale; // root joint

					worldMats[joint] = worldMat;
				}
				else 
				{
					worldMat = worldMats[parentIdx] * translation * jointRot * scale;

					worldMats[joint] = worldMat;
					
					Vector3f parentPos = Vector3f(worldMats[parentIdx].axes.translate.x, worldMats[parentIdx].axes.translate.y, worldMats[parentIdx].axes.translate.z);

					Vector3f childPos = Vector3f(worldMat.axes.translate.x, worldMat.axes.translate.y, worldMat.axes.translate.z);

					Vector3f boneDir =  parentPos - childPos;

					float boneScale = Length(boneDir);

					boneDir = Normalize(boneDir);

					Vector3f from = { 0.0f, 1.0f, 0.0f };
					Vector3f axis = Cross(from, boneDir);
					float axisLen = Length(axis);
					float angle = atan2f(axisLen, Dot(from, boneDir));

					if (axisLen < 0.0001f && Dot(from, boneDir) > 0.0f)
					{
						worldMat = Identity4f();
					}
					else if (Dot(from, boneDir) < -0.9999f)
					{
						worldMat = CreateRotationMatrixMat4({ 1,0,0 }, PI);
					}
					else
					{
						worldMat = CreateRotationMatrixMat4(Normalize(axis), angle);
					}

					worldMat.axes.right.x *= boneScale;
					worldMat.axes.right.y *= boneScale;
					worldMat.axes.right.z *= boneScale;
					worldMat.axes.up.x *= boneScale;
					worldMat.axes.up.y *= boneScale;
					worldMat.axes.up.z *= boneScale;
					worldMat.axes.forward.x *= boneScale;
					worldMat.axes.forward.y *= boneScale;
					worldMat.axes.forward.z *= boneScale;

					worldMat.axes.translate = Vector4f(parentPos.x, parentPos.y, parentPos.z, 1.0f);
					
				}

				GlobalRenderer::gRenderInstance.UpdateDriverMemory(&worldMat, jointMeshWorldMatrix, sizeof(Matrix4f), (startingJointLocation + joint) * sizeof(Matrix4f), TransferType::CACHED);
				GlobalRenderer::gRenderInstance.UpdateDriverMemory(&skel->joints[joint].granny_parentIndex, jointMeshParentIndices, sizeof(uint32_t), (startingJointLocation + joint) * sizeof(uint32_t), TransferType::CACHED);
			}


			jointMeshWorldMatrixCount += skel->jointCount;

			CreateJointVisualObject(skel->jointCount, startingJointLocation);

			totalSkeletonCount++;

			break;
		}
		case GR2:
		default:
			mainAppLogger.AddLogMessage(LogMessageType::LOGINFO, STRING_VIEW_FROM_LITERAL("Unhandled SMB Chunk"));
			break;
		}
	}

	int globalTextureStartIndex = 0;


	for (int i = 0; i < totalMeshCount; i++) 
	{
		SMBGeoChunk* geoDef = &geoDefs[i];

		int* flattenMatHandles = (int*)SMBThreadedFileScratchAllocators[arenaIndex].Allocate(sizeof(int) * geoDef->numMaterials);

		int uploadedTextureCount = 0;

		for (int jj = 0; jj < geoDef->numRenderables; jj++)
		{
			int count = geoDef->materialsCount[jj];
			for (int hh = 0; hh < count; hh++)
			{
				uint32_t id = geoDef->materialsId[hh + uploadedTextureCount];
				int gg = 0;
				for (; gg < totalTextureCount; gg++)
				{
					if (id == textures[gg].id)
					{
						flattenMatHandles[hh + uploadedTextureCount] = gg;
						break;
					}
				}

				if (gg == totalTextureCount)
				{
					flattenMatHandles[hh + uploadedTextureCount] = 0;
					mainAppLogger.AddLogMessage(LogMessageType::LOGERROR, STRING_VIEW_FROM_LITERAL("Unhandled Mesh Material Image"));
				}
			}
			uploadedTextureCount += count;
		}


		if (uploadedTextureCount)
		{
			TextureDetails** details = (TextureDetails**)SMBThreadedFileScratchAllocators[arenaIndex].Allocate(sizeof(TextureDetails*) * uploadedTextureCount);

			globalTextureStartIndex = mainDictionary.AllocateNTextureHandles(uploadedTextureCount, details);

			for (int ii = 0; ii < uploadedTextureCount; ii++)
			{
				SMBTexture& texture = textures[flattenMatHandles[ii]];

				ImageFormat format = ConvertSMBImageToAppImage(texture.type);

				texture.data = (char*)mainDictionary.AllocateImageCache(texture.cumulativeSize);

				texture.ReadTextureData(file);

				DeviceSlabAllocator* perFormatAllocator = nullptr;

				int poolIndex = GetPoolIndexByFormat(format, &perFormatAllocator);

				size_t actualMemorySize = 0, actualMemoryAlignment = 0, actualMemoryAddress = 0;

				GlobalRenderer::gRenderInstance.GetGPURequestedImageSizeAndAlignment(mainLogicalDevice,
					texture.width, texture.height, texture.miplevels, 1, format, ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST, &actualMemorySize, &actualMemoryAlignment
				);

				actualMemoryAddress = perFormatAllocator->Allocate(actualMemorySize, actualMemoryAlignment);

				int smbImageIndex = mainDictionary.textureHandles[ii + globalTextureStartIndex] =
					GlobalRenderer::gRenderInstance.CreateImageHandle(mainLogicalDevice,
						actualMemoryAddress,
						texture.width,
						texture.height,
						texture.miplevels,
						1,
						format,
						ImageType::IMAGE_2D,
						ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST,
						poolIndex
					);

				int viewIndex = GlobalRenderer::gRenderInstance.CreateImageView(mainLogicalDevice, smbImageIndex, 0, IMAGE_VIEW_ALL_MIPS, 0, IMAGE_VIEW_ALL_LAYERS, COLOR_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

				GlobalRenderer::gRenderInstance.UpdateImageMemory(
					texture.data,
					mainDictionary.textureHandles[ii + globalTextureStartIndex],
					texture.cumulativeSize,
					texture.width,
					texture.height,
					texture.miplevels,
					0,
					1,
					0,
					COLOR_IMAGE_ASPECT
				);
			}

			DeviceHandleArrayUpdateTextureView* texViews = (DeviceHandleArrayUpdateTextureView*)SMBThreadedFileScratchAllocators[arenaIndex].Allocate(sizeof(DeviceHandleArrayUpdateTextureView) * uploadedTextureCount);

			for (int imageIndex = 0; imageIndex < uploadedTextureCount; imageIndex++)
			{
				texViews[imageIndex].imageHandle = mainDictionary.textureHandles[globalTextureStartIndex + imageIndex];
				texViews[imageIndex].viewIndex = 0;
			}

			DeviceHandleArrayUpdate update;

			update.updateType = DeviceHandleArrayUpdateType::TEXTURE_VIEW_UPDATE;
			update.resourceCount = uploadedTextureCount;
			update.resourceDstBegin = globalTextureStartIndex;
			update.resourceHandles = texViews;

			GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);
		}

		SMBGeometricalObject(geoDef, file, globalTextureStartIndex, arenaIndex);
	}

	SMBThreadedFileInputAllocators[arenaIndex].Reset();
	SMBThreadedFileScratchAllocators[arenaIndex].Reset();

	mainAppLogger.ProcessMessage();
}

int CreateDebugCommandBuffers(int count)
{
	debugIndirectDrawData.commandBufferCount = 0;
	debugIndirectDrawData.commandBufferSize = count;

	ShaderResourceSetContext debugRSContext{ &mainAppLogger, false };

	debugIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(VkDrawIndirectCommand),  debugIndirectDrawData.commandBufferSize, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	debugIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), debugIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);

	debugIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	ShaderResourceSetBuilder indirectCullBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, DEBUGCULL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	debugIndirectDrawData.indirectCullDescriptor = indirectCullBuilder();
	 
	indirectCullBuilder.BindBufferToShaderResource(&debugRSContext, &debugIndirectDrawData.commandBufferAlloc, 0, 1, 0);

	indirectCullBuilder.BindBufferView(&debugRSContext, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	indirectCullBuilder.BindBufferView(&debugRSContext, &globalDebugTypesAlloc, 0, 1, 2);

	indirectCullBuilder.BindBufferToShaderResource(&debugRSContext, &globalBufferLocation,  0, 1, 3);

	indirectCullBuilder.BindBufferToShaderResource(&debugRSContext, &globalDebugStructAlloc, 0, 1, 4);

	indirectCullBuilder.BindBufferToShaderResource(&debugRSContext, &debugIndirectDrawData.commandBufferCountAlloc, 0, 1, 5);

	indirectCullBuilder.UploadConstant(&debugRSContext,  &debugIndirectDrawData.commandBufferCount, 0);


	if (debugRSContext.contextFailed)
	{
		debugRSContext.contextLogger->ProcessMessage();
		return -1;
	}


	std::array<ShaderResourceSetHandle, 1> debugCullDescriptors = { debugIndirectDrawData.indirectCullDescriptor };

	ShaderComputeLayout* debugCullDescriptorLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(DEBUGCULL);

	ComputeIntermediaryPipelineInfo debugCullPipelineCreate = {
			.x = count / debugCullDescriptorLayout->x,
			.y = 1,
			.z = 1,
			.pipelinename = DEBUGCULL,
			.descCount = 1,
			.descriptorsetid = debugCullDescriptors.data()
	};


	debugIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &debugCullPipelineCreate);

	if (debugIndirectDrawData.indirectCullPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Indirect cull pipeline failed creation"));
		return -1;
	}

	ShaderResourceSetBuilder indirectDrawBuilder =  GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, DEBUGDRAW, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	debugIndirectDrawData.indirectDrawDescriptor = indirectDrawBuilder();

	indirectDrawBuilder.BindBufferToShaderResource(&debugRSContext, &globalDebugStructAlloc, 0, 1, 0);

	indirectDrawBuilder.BindBufferView(&debugRSContext, &debugIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 1);

	indirectDrawBuilder.BindBufferView(&debugRSContext, &globalDebugTypesAlloc, 0, 1, 2);

	std::array<ShaderResourceSetHandle, 2> indirectDebugDrawDescriptors = {
		globalBufferDescriptor,
		debugIndirectDrawData.indirectDrawDescriptor
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
		.indirectAllocation = debugIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = debugIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = debugIndirectDrawData.commandBufferCountAlloc
	};


	debugIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &indirectDebugDrawPipelineCreate);

	if (debugIndirectDrawData.indirectDrawPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("indirect draw pipeline failed creation"));
		return -1;
	}

	return 0;
}

int CreateGenericMeshCommandBuffers(int count)
{
	mainIndirectDrawData.commandBufferSize = count;
	mainIndirectDrawData.commandBufferCount = 0;

	ShaderResourceSetContext genericMeshRSContext{ &mainAppLogger, false };

	mainIndirectDrawData.commandBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), count, alignof(VkDrawIndexedIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	mainIndirectDrawData.commandBufferCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	ShaderResourceSetBuilder indirectCullBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, RENDEROBJCULL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	
	mainIndirectDrawData.indirectCullDescriptor = indirectCullBuilder();

	mainIndirectDrawData.indirectGlobalIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), mainIndirectDrawData.commandBufferSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);


	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &mainIndirectDrawData.commandBufferAlloc, 0, 1, 0);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalMeshLocation, 0, 1, 1);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalBufferLocation, 0, 1, 2);
	indirectCullBuilder.BindBufferView(&genericMeshRSContext, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 3);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &mainIndirectDrawData.commandBufferCountAlloc, 0, 1, 4);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalRenderableLocation, 0, 1, 5);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryDescriptionsLocation, 0, 1, 6);
	indirectCullBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryRenderableLocation, 0, 1, 7);

	indirectCullBuilder.UploadConstant(&genericMeshRSContext, &mainGrid, 0);
	indirectCullBuilder.UploadConstant(&genericMeshRSContext, &mainIndirectDrawData.commandBufferCount, 1);

	ShaderResourceSetBuilder indirectDrawBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, GENERIC, 2, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	
	mainIndirectDrawData.indirectDrawDescriptor = indirectDrawBuilder();
	
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalMeshLocation, 0, 1, 0);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalVertexBuffer, 0, 1, 1);
	indirectDrawBuilder.BindBufferView(&genericMeshRSContext, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalLightBuffer, 0, 1, 3);
	indirectDrawBuilder.BindBufferView(&genericMeshRSContext, &globalLightTypesBuffer,  0, 1, 4);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalMaterialsLocation, 0, 1, 5);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalRenderableLocation, 0, 1, 6);
	indirectDrawBuilder.BindBufferView(&genericMeshRSContext, &globalMaterialIndicesLocation, 0, 1, 7);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalBlendDetailsLocation, 0, 1, 8);
	indirectDrawBuilder.BindBufferView(&genericMeshRSContext, &globalBlendRangesLocation, 0, 1, 9);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryDescriptionsLocation, 0, 1, 10);
	indirectDrawBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryRenderableLocation, 0, 1, 11);

	std::array<ShaderResourceSetHandle, 3> indirectDrawDescriptors = {
		globalBufferDescriptor,
		globalTexturesDescriptor,
		mainIndirectDrawData.indirectDrawDescriptor,
	};

	shadowMapIndex = mainDictionary.AllocateNTextureHandles(GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, nullptr);

	int viewIndex = GlobalRenderer::gRenderInstance.CreateAttachmentImageView(mainLogicalDevice, MSAAShadowMapping, 1, 0, 1, 0, 1, DEPTH_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, viewIndex, globalTexturesDescriptor, 3, shadowMapIndex);

	indirectDrawBuilder.UploadConstant(&genericMeshRSContext, &shadowMapIndex, 0);
	indirectDrawBuilder.UploadConstant(&genericMeshRSContext, &GlobalRenderer::gRenderInstance.currentFrame, 1);

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
		.indirectAllocation = mainIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = mainIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = mainIndirectDrawData.commandBufferCountAlloc
	};

	mainIndirectDrawData.indirectDrawPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &indirectDrawCreate);

	if (mainIndirectDrawData.indirectDrawPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("indirect draw pipeline failed creation"));
		return -1;
	}

	ShaderResourceSetBuilder cullLightDescriptorBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, RENDEROBJCULL, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);


	cullLightDescriptorBuilder.BindBufferView(&genericMeshRSContext, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	cullLightDescriptorBuilder.BindBufferView(&genericMeshRSContext, &lightAssignment.deviceOffsetsAlloc, 0, 1, 0);
	cullLightDescriptorBuilder.BindBufferView(&genericMeshRSContext, &lightAssignment.deviceCountsAlloc, 0, 1, 1);
	cullLightDescriptorBuilder.BindBufferToShaderResource(&genericMeshRSContext, &globalLightBuffer, 0, 1, 3);
	cullLightDescriptorBuilder.BindBufferView(&genericMeshRSContext, &globalLightTypesBuffer, 0, 1, 4 );

	if (genericMeshRSContext.contextFailed)
	{
		genericMeshRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ShaderComputeLayout* layout = GlobalRenderer::gRenderInstance.GetComputeLayout(RENDEROBJCULL);

	std::array<ShaderResourceSetHandle, 2> computeDescriptors = { mainIndirectDrawData.indirectCullDescriptor, cullLightDescriptorBuilder() };

	ComputeIntermediaryPipelineInfo mainCullComputeSetup = {
			.x = count / layout->x,
			.y = 1,
			.z = 1,
			.pipelinename = RENDEROBJCULL,
			.descCount = 2,
			.descriptorsetid = computeDescriptors.data()
	};

	mainIndirectDrawData.indirectCullPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &mainCullComputeSetup);

	ShaderResourceSetBuilder outlineDescriptor = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, OUTLINE, 2, 3);

	outlineDescriptor.BindBufferToShaderResource(&genericMeshRSContext, &globalMeshLocation, 0, 1, 0);

	outlineDescriptor.BindBufferToShaderResource(&genericMeshRSContext, &globalVertexBuffer, 0, 1, 1);

	outlineDescriptor.BindBufferView(&genericMeshRSContext, &mainIndirectDrawData.indirectGlobalIDsAlloc, 0, 1, 2);
	outlineDescriptor.BindBufferToShaderResource(&genericMeshRSContext, &globalRenderableLocation, 0, 1, 3);
	outlineDescriptor.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryDescriptionsLocation, 0, 1, 4);
	outlineDescriptor.BindBufferToShaderResource(&genericMeshRSContext, &globalGeometryRenderableLocation, 0, 1, 5);

	static Vector4f black = { 0.0, 0.0, 0.0, 1.0 };

	static float outlineLength = 1.025;

	outlineDescriptor.UploadConstant(&genericMeshRSContext, &black, 0);

	outlineDescriptor.UploadConstant(&genericMeshRSContext, &outlineLength, 1);

	std::array<ShaderResourceSetHandle, 1> indirectOutline = { outlineDescriptor()};

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
		.indirectAllocation = mainIndirectDrawData.commandBufferAlloc,
		.indirectDrawCount = mainIndirectDrawData.commandBufferSize,
		.indirectCountAllocation = mainIndirectDrawData.commandBufferCountAlloc
	};

	//int outlinePipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(&outlineDrawCreate, true);

	/*
	
	if (outlinePipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Outline pipeline failed creation"));
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

	ShaderResourceSetBuilder prefixSumBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	worldSpaceAssignment.prefixSumDescriptors = prefixSumBuilder();
	 
	worldSpaceAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	worldSpaceAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	if (worldSpaceAssignment.totalSumsNeeded)
	{
		worldSpaceAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
		
		prefixSumBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceSumsAlloc, 0, 1, 2);
	}
	else 
	{
		prefixSumBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, nullptr, 0, 0, 2);
	}

	prefixSumBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceCountsAlloc, 0, 1, 0);
	prefixSumBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceOffsetsAlloc, 0, 1, 1);

	prefixSumBuilder.UploadConstant(&genericMeshWorldRSContext, &worldSpaceAssignment.totalElementsCount, 0);

	std::array<ShaderResourceSetHandle, 1> prefixSumDescriptor = { worldSpaceAssignment.prefixSumDescriptors };

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

	worldSpaceAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &worldAssignmentPrefix);

	if (worldSpaceAssignment.prefixSumPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Space sum after failed creation"));
		return -1;
	}
	
	if (worldSpaceAssignment.totalSumsNeeded)
	{

		ShaderResourceSetBuilder sumAfterBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		
		worldSpaceAssignment.sumAfterDescriptors = sumAfterBuilder();

		sumAfterBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceSumsAlloc, 0, 1, 0);
		sumAfterBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceSumsAlloc, 0, 1, 1);
		sumAfterBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceSumsAlloc, 0, 1, 2);
		sumAfterBuilder.UploadConstant(&genericMeshWorldRSContext, &worldSpaceAssignment.totalSumsNeeded, 0);

		std::array<ShaderResourceSetHandle, 1> prefixSumOverflowDescriptor = { worldSpaceAssignment.sumAfterDescriptors, };

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

		worldSpaceAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &prefixSumComputePipeline);

		if (worldSpaceAssignment.sumAfterPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Space sum after failed creation"));
			return -1;
		}

		ShaderResourceSetBuilder sumAppliedToBin = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXADD, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		worldSpaceAssignment.sumAppliedToBinDescriptors = sumAppliedToBin();

		sumAppliedToBin.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceOffsetsAlloc, 0, 1, 0);
		sumAppliedToBin.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceSumsAlloc, 0, 1, 1);
		sumAppliedToBin.UploadConstant(&genericMeshWorldRSContext, &worldSpaceAssignment.totalElementsCount, 0);

		std::array<ShaderResourceSetHandle, 1> incrementSumsDescriptor = { worldSpaceAssignment.sumAppliedToBinDescriptors, };

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

		worldSpaceAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &incrementSumsComputePipeline);

		if (worldSpaceAssignment.sumAppliedToBinPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Space sum applied failed creation"));
			return -1;
		}
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(WORLDORGANIZE);

	uint32_t assignmentGroupCount = (uint32_t)ceil(worldSpaceAssignment.totalElementsCount / (float)assignmentLayout->x);

	ShaderResourceSetBuilder worldSpaceDivisionBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, WORLDORGANIZE, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	worldSpaceAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), worldSpaceAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);

	worldSpaceAssignment.preWorldSpaceDivisionDescriptor = worldSpaceDivisionBuilder();

	worldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceCountsAlloc, 0, 1, 0);
	worldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalMeshLocation, 0, 1, 1);
	worldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalRenderableLocation, 0, 1, 2);
	worldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalGeometryDescriptionsLocation, 0, 1, 3);
	worldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalGeometryRenderableLocation, 0, 1, 4);
	worldSpaceDivisionBuilder.UploadConstant(&genericMeshWorldRSContext, &mainGrid, 0);
	worldSpaceDivisionBuilder.UploadConstant(&genericMeshWorldRSContext, &mainIndirectDrawData.commandBufferCount, 1);

	std::array<ShaderResourceSetHandle, 1> preWorldDivDescriptor = { worldSpaceAssignment.preWorldSpaceDivisionDescriptor };

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

	worldSpaceAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &preWorldDivComputePipeline);

	if (worldSpaceAssignment.preWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Space pre division failed creation"));
		return -1;
	}

	ShaderResourceSetBuilder postWorldSpaceDivisionBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, WORLDASSIGN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	
	worldSpaceAssignment.postWorldSpaceDivisionDescriptor = postWorldSpaceDivisionBuilder();

	postWorldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &worldSpaceAssignment.deviceOffsetsAlloc, 0, 1, 0);
	postWorldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalMeshLocation, 0, 1, 1);
	postWorldSpaceDivisionBuilder.BindBufferView(&genericMeshWorldRSContext, &worldSpaceAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	postWorldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalRenderableLocation, 0, 1, 3);
	postWorldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalGeometryDescriptionsLocation, 0, 1, 4);
	postWorldSpaceDivisionBuilder.BindBufferToShaderResource(&genericMeshWorldRSContext, &globalGeometryRenderableLocation, 0, 1, 5);
	postWorldSpaceDivisionBuilder.UploadConstant(&genericMeshWorldRSContext, &mainGrid, 0);
	postWorldSpaceDivisionBuilder.UploadConstant(&genericMeshWorldRSContext, &mainIndirectDrawData.commandBufferCount, 1);

	std::array<ShaderResourceSetHandle, 1> postWorldDivDescriptor = {  worldSpaceAssignment.postWorldSpaceDivisionDescriptor, };

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

	worldSpaceAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &postWorldDivComputePipeline);

	if (worldSpaceAssignment.postWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Space assign failed creation"));
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

	ShaderResourceSetBuilder prefixSumBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	lightAssignment.prefixSumDescriptors = prefixSumBuilder();

	lightAssignment.deviceOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	lightAssignment.deviceCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	prefixSumBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceCountsAlloc, 0, 1, 0);
	prefixSumBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceOffsetsAlloc, 0, 1, 1);
	prefixSumBuilder.UploadConstant(&genericLightWorldRSContext, &lightAssignment.totalElementsCount, 0);

	if (lightAssignment.totalSumsNeeded)
	{
		lightAssignment.deviceSumsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalSumsNeeded, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);

		prefixSumBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceSumsAlloc, 0, 1, 2);

	}
	else
	{
		prefixSumBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, nullptr, 0, 0, 2);
	}

	std::array<ShaderResourceSetHandle, 1> prefixSumDescriptor = { lightAssignment.prefixSumDescriptors };

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

	lightAssignment.prefixSumPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &worldAssignmentPrefix);

	if (lightAssignment.prefixSumPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Prefix Sum After failed creation"));
		return -1;
	}

	if (lightAssignment.totalSumsNeeded)
	{

		ShaderResourceSetBuilder sumAfterBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

		lightAssignment.sumAfterDescriptors = sumAfterBuilder();
		sumAfterBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceSumsAlloc, 0, 1, 0);
		sumAfterBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceSumsAlloc, 0, 1, 1);
		sumAfterBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceSumsAlloc, 0, 1, 2);
		sumAfterBuilder.UploadConstant(&genericLightWorldRSContext, &lightAssignment.totalSumsNeeded, 0);

		std::array<ShaderResourceSetHandle, 1> prefixSumOverflowDescriptor = { lightAssignment.sumAfterDescriptors };

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

		lightAssignment.sumAfterPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &prefixSumComputePipeline);

		if (lightAssignment.sumAfterPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Sum After failed creation"));
			return -1;
		}


		ShaderResourceSetBuilder sumAppliedToBinBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXADD, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		lightAssignment.sumAppliedToBinDescriptors = sumAppliedToBinBuilder();

		sumAppliedToBinBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceOffsetsAlloc, 0, 1, 0);
		sumAppliedToBinBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceSumsAlloc, 0, 1, 1);
		sumAppliedToBinBuilder.UploadConstant(&genericLightWorldRSContext, &lightAssignment.totalElementsCount, 0);

		std::array<ShaderResourceSetHandle, 1> incrementSumsDescriptor = { lightAssignment.sumAppliedToBinDescriptors, };

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


		lightAssignment.sumAppliedToBinPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &incrementSumsComputePipeline);

		if (lightAssignment.sumAppliedToBinPipeline < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("World Sum failed creation"));
			return -1;
		}
	}

	ShaderComputeLayout* assignmentLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(LIGHTASSIGN);

	uint32_t assignmentGroupCount = (uint32_t)ceil(lightAssignment.totalElementsCount / (float)assignmentLayout->x);

	lightAssignment.worldSpaceDivisionAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), lightAssignment.totalElementsCount * 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	ShaderResourceSetBuilder preWorldSpaceBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, LIGHTORGANIZE, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	lightAssignment.preWorldSpaceDivisionDescriptor = preWorldSpaceBuilder();

	preWorldSpaceBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceCountsAlloc, 0, 1, 0);
	preWorldSpaceBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &globalLightBuffer, 0, 1, 1);
	preWorldSpaceBuilder.BindBufferView(&genericLightWorldRSContext, &globalLightTypesBuffer, 0, 1, 2);
	preWorldSpaceBuilder.UploadConstant(&genericLightWorldRSContext, &mainGrid, 0);
	preWorldSpaceBuilder.UploadConstant(&genericLightWorldRSContext, &globalLightCount, 1);

	std::array<ShaderResourceSetHandle, 1> preWorldDivDescriptor = { lightAssignment.preWorldSpaceDivisionDescriptor, };

	if (genericLightWorldRSContext.contextFailed)
	{
		genericLightWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo preWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTORGANIZE,
			.descCount = 1,
			.descriptorsetid = preWorldDivDescriptor.data()
	};

	lightAssignment.preWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &preWorldDivComputePipeline);

	if (lightAssignment.preWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Pre World Division Pipeline failed creation"));
		return -1;
	}

	ShaderResourceSetBuilder postWorldBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, LIGHTASSIGN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	
	lightAssignment.postWorldSpaceDivisionDescriptor = postWorldBuilder();

	postWorldBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &lightAssignment.deviceOffsetsAlloc, 0, 1, 0);
	postWorldBuilder.BindBufferToShaderResource(&genericLightWorldRSContext, &globalLightBuffer, 0, 1, 1);
	postWorldBuilder.BindBufferView(&genericLightWorldRSContext, &globalLightTypesBuffer, 0, 1, 3);
	postWorldBuilder.BindBufferView(&genericLightWorldRSContext, &lightAssignment.worldSpaceDivisionAlloc, 0, 1, 2);
	postWorldBuilder.UploadConstant(&genericLightWorldRSContext, &mainGrid, 0);
	postWorldBuilder.UploadConstant(&genericLightWorldRSContext, &globalLightCount, 1);

	std::array<ShaderResourceSetHandle, 1> postWorldDivDescriptor = { lightAssignment.postWorldSpaceDivisionDescriptor, };

	if (genericLightWorldRSContext.contextFailed)
	{
		genericLightWorldRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ComputeIntermediaryPipelineInfo postWorldDivComputePipeline = {
			.x = assignmentGroupCount,
			.y = 1,
			.z = 1,
			.pipelinename = LIGHTASSIGN,
			.descCount = 1,
			.descriptorsetid = postWorldDivDescriptor.data()
	};

	lightAssignment.postWorldSpaceDivisionPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &postWorldDivComputePipeline);

	if (lightAssignment.postWorldSpaceDivisionPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Post World Division failed creation"));
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
	mainShadowMapManager.shadowMapCountsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapCountsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	mainShadowMapManager.shadowMapOffsetsAllocSize =  maxObjCount;
	mainShadowMapManager.shadowMapOffsetsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapOffsetsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	mainShadowMapManager.shadowMapAssignmentsAllocSize = maxShadowMapAssignment * maxObjCount;
	mainShadowMapManager.shadowMapAssignmentsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapAssignmentsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);

	mainShadowMapManager.shadowMapObjectIDsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapObjectIDsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), mainShadowMapManager.shadowMapObjectIDsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	mainShadowMapManager.shadowMapObjectCountAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	mainShadowMapManager.shadowMapIndirectBufferAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapIndirectBufferAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, sizeof(VkDrawIndexedIndirectCommand), mainShadowMapManager.shadowMapIndirectBufferAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator); ;
	

	mainShadowMapManager.shadowMapViewProjAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapViewProjAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(Matrix4f) * 2, mainShadowMapManager.shadowMapViewProjAllocSize, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	mainShadowMapManager.shadowMapAtlasViewsAllocSize = maxObjCount;
	mainShadowMapManager.shadowMapAtlasViewsAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(ShadowMapView), mainShadowMapManager.shadowMapAtlasViewsAllocSize, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT, &mainHostAllocator);


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

	
	ShaderResourceSetBuilder shadowClippingDescriptorB1 = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, SHADOWMAPCULL, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	ShaderResourceSetBuilder shadowClippingDescriptorB2 = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, SHADOWMAPCULL, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	

	mainShadowMapManager.shadowClippingDescriptor1 = shadowClippingDescriptorB1();
	mainShadowMapManager.shadowClippingDescriptor2 = shadowClippingDescriptorB2();

	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapIndirectBufferAlloc, 0, 1, 0);
	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalMeshLocation, 0, 1, 1);
	shadowClippingDescriptorB1.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapObjectCountAlloc, 0, 1, 3);
	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalRenderableLocation, 0, 1, 4);
	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalGeometryDescriptionsLocation, 0, 1, 5);
	shadowClippingDescriptorB1.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalGeometryRenderableLocation, 0, 1, 6);

	shadowClippingDescriptorB2.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalLightBuffer, 0, 1, 0);
	shadowClippingDescriptorB2.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 1);
	shadowClippingDescriptorB2.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapCountsAlloc, 0, 1, 2);
	shadowClippingDescriptorB2.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 3);
	shadowClippingDescriptorB2.BindBufferView(&genericMainShadowMapRSContext, &globalLightTypesBuffer, 0, 1, 4);

	shadowClippingDescriptorB1.UploadConstant(&genericMainShadowMapRSContext, &mainIndirectDrawData.commandBufferCount, 0);
	shadowClippingDescriptorB1.UploadConstant(&genericMainShadowMapRSContext, &globalLightCount, 1);

	std::array<ShaderResourceSetHandle, 2> shadowClipDesc = { mainShadowMapManager.shadowClippingDescriptor1, mainShadowMapManager.shadowClippingDescriptor2 };

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	ShaderComputeLayout* shadowMapCullLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(SHADOWMAPCULL);

	uint32_t maxCullInvocations = (uint32_t)ceil(maxObjCount / (float)shadowMapCullLayout->x);

	ComputeIntermediaryPipelineInfo shadowClipPipelineInfo = {
			.x = maxCullInvocations,
			.y = 1,
			.z = 1,
			.pipelinename = SHADOWMAPCULL,
			.descCount = 2,
			.descriptorsetid = shadowClipDesc.data()
	};
	
	mainShadowMapManager.shadowClippingPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &shadowClipPipelineInfo);

	if (mainShadowMapManager.shadowClippingPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Shadow Clipping Pipeline failed creation"));
		return -1;
	}

	ShaderResourceSetBuilder shadowMapDescBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, SHADOWMAPDRAW, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	smdpd.shadowMapDescriptorSet = shadowMapDescBuilder();

	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalMeshLocation, 0, 1, 0);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalVertexBuffer, 0, 1, 1);
	shadowMapDescBuilder.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapObjectIDsAlloc, 0, 1, 2);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalRenderableLocation, 0, 1, 3);
	shadowMapDescBuilder.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapOffsetsAlloc, 0, 1, 4);
	shadowMapDescBuilder.BindBufferView(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapAssignmentsAlloc, 0, 1, 5);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapViewProjAlloc, 0, 1, 6);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &mainShadowMapManager.shadowMapAtlasViewsAlloc, 0, 1, 7);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalGeometryDescriptionsLocation, 0, 1, 8);
	shadowMapDescBuilder.BindBufferToShaderResource(&genericMainShadowMapRSContext, &globalGeometryRenderableLocation, 0, 1, 9);

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	std::array<ShaderResourceSetHandle, 1> indirectShadowMapDraw = { smdpd.shadowMapDescriptorSet };
	
	GraphicsIntermediaryPipelineInfo indirectShadowDrawCreate = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 0,
		.pipelinename = SHADOWMAPDRAW,
		.descCount = 1,
		.descriptorsetid = indirectShadowMapDraw.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexSize = 2,
		.indirectAllocation = mainShadowMapManager.shadowMapIndirectBufferAlloc,
		.indirectDrawCount = mainShadowMapManager.shadowMapIndirectBufferAllocSize,
		.indirectCountAllocation = mainShadowMapManager.shadowMapObjectCountAlloc
	};


	smdpd.shadowMapPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &indirectShadowDrawCreate);

	if (smdpd.shadowMapPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Shadow Map Pipeline failed creation"));
		return -1;
	}
	
	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.updateType = DeviceHandleArrayUpdateType::SAMPLER_UPDATE;
	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;

	ShaderResourceSetBuilder fullScreenBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, FULLSCREEN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	smdpd.fullScreenDescriptorSet = fullScreenBuilder();

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, 1, smdpd.fullScreenDescriptorSet, 0, 0);
	GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(smdpd.fullScreenDescriptorSet, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	fullScreenBuilder.UploadConstant(&genericMainShadowMapRSContext, &GlobalRenderer::gRenderInstance.currentFrame, 0);

	std::array<ShaderResourceSetHandle, 1> fullScreenDesc = { smdpd.fullScreenDescriptorSet };

	if (genericMainShadowMapRSContext.contextFailed)
	{
		genericMainShadowMapRSContext.contextLogger->ProcessMessage();
		return -1;
	}

	GraphicsIntermediaryPipelineInfo fullscreenInfo = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 4,
		.pipelinename = FULLSCREEN,
		.descCount = 1,
		.descriptorsetid = fullScreenDesc.data(),
		.indexBufferHandle = ~0,
		.indexCount = 0,
		.instanceCount = 1,
		.indexSize = 0,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	smdpd.fullScreenPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &fullscreenInfo);

	if (smdpd.fullScreenPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Shadow Full Screen Pipeline failed creation"));
		return -1;
	}

	return 0;
}

void RecreateFrameGraphAttachments(uint32_t width, uint32_t height)
{
	mainRTVSlab.dataAllocator = 0;
	mainDSVSlab.dataAllocator = 0;

	if (BasicShadow >= 0)
	{
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, BasicShadow, 0, nullptr, &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
	}

	if (MSAAPost >= 0)
	{
		GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(mainLogicalDevice, MSAAPost, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, width, height, nullptr, &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, MSAAPost, 1, nullptr, &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
		int viewIndex = GlobalRenderer::gRenderInstance.CreateAttachmentImageView(mainLogicalDevice, MSAAPost, 1, 0, 1, 0, 1, COLOR_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);
		GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAPost, 1, viewIndex, mainFullScreen, 0, 0);
	}
	
	if (MSAAShadowMapping >= 0)
	{
		GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(mainLogicalDevice, MSAAShadowMapping, 0, 3, 4096, 4096, nullptr, &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
		GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, MSAAShadowMapping, 1, nullptr, &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);

		int viewIndex = GlobalRenderer::gRenderInstance.CreateAttachmentImageView(mainLogicalDevice, MSAAShadowMapping, 1, 0, 1, 0, 1, DEPTH_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

		GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, viewIndex, globalTexturesDescriptor, 3, shadowMapIndex);
		GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAShadowMapping, 1, viewIndex, smdpd.fullScreenDescriptorSet, 0, 0);
	}
}

int CreateMSAAPostFullScreen()
{
	ShaderResourceSetContext genericMSAARSContext{ &mainAppLogger, false };

	DeviceHandleArrayUpdate samplerUpdate;

	samplerUpdate.updateType = DeviceHandleArrayUpdateType::SAMPLER_UPDATE;
	samplerUpdate.resourceCount = 1;
	samplerUpdate.resourceDstBegin = 0;
	samplerUpdate.resourceHandles = &mainLinearSampler;

	ShaderResourceSetBuilder mainFullScreenB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, FULLSCREEN, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	mainFullScreen = mainFullScreenB();

	int viewIndex = GlobalRenderer::gRenderInstance.CreateAttachmentImageView(mainLogicalDevice, MSAAPost, 1, 0, 1, 0, 1, COLOR_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

	GlobalRenderer::gRenderInstance.UploadFrameAttachmentResource(MSAAPost, 1, viewIndex, mainFullScreen, 0, 0);
	GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(mainFullScreen, 1, ShaderResourceType::SAMPLERSTATE, &samplerUpdate);

	mainFullScreenB.UploadConstant(&genericMSAARSContext, &GlobalRenderer::gRenderInstance.currentFrame, 0);

	if (genericMSAARSContext.contextFailed)
	{
		genericMSAARSContext.contextLogger->ProcessMessage();
		return -1;
	}

	std::array<ShaderResourceSetHandle, 1> mainFullScreenDesc = { mainFullScreen };

	GraphicsIntermediaryPipelineInfo fullscreenInfo = {
		.drawType = 0,
		.vertexBufferHandle = ~0,
		.vertexCount = 4,
		.pipelinename = FULLSCREEN,
		.descCount = 1,
		.descriptorsetid = mainFullScreenDesc.data(),
		.indexBufferHandle = ~0,
		.indexCount = 0,
		.instanceCount = 1,
		.indexSize = 0,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	mainFullScreenPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &fullscreenInfo);

	if (mainFullScreenPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Main Full Screen Pipeline failed creation"));
		return -1;
	}

	return 0;
}

int CreateSkyBox()
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


	int vertexAlloc = GlobalRenderer::gRenderInstance.CreateSuballocation(mainLogicalDevice, globalVertexBuffer, sizeof(BoxVerts), 1, 64, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &vertexBufferAlloc);
	int indexAlloc = GlobalRenderer::gRenderInstance.CreateSuballocation(mainLogicalDevice, globalIndexBuffer, sizeof(BoxIndices), 1, 64, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &indexBufferAlloc);


	GlobalRenderer::gRenderInstance.UpdateDriverMemory(BoxVerts, vertexAlloc, sizeof(BoxVerts), 0, TransferType::CACHED);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(BoxIndices, indexAlloc, sizeof(BoxIndices), 0, TransferType::CACHED);

	StringView names[6] = {
		STRING_VIEW_FROM_LITERAL("face4.bmp"),
		STRING_VIEW_FROM_LITERAL("face1.bmp"),
		STRING_VIEW_FROM_LITERAL("face5.bmp"),
		STRING_VIEW_FROM_LITERAL("face2.bmp"),
		STRING_VIEW_FROM_LITERAL("face6.bmp"),
		STRING_VIEW_FROM_LITERAL("face3.bmp"),
	};

	ShaderResourceSetContext genericSkyBoxRSontext{ &mainAppLogger, false };

	skyboxCubeImage = ReadCubeImage(names, 6, BMP);

	static Matrix4f matrix = Identity4f();

	matrix.axes.translate = Vector4f(-30.0, 0.0, 0.0, 1.0f);

	ShaderResourceSetBuilder camSkyboxData = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, SKYBOX, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	ShaderResourceSetBuilder skyboxDesc = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, SKYBOX, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	int viewIndex = 0;

	camSkyboxData.BindBufferToShaderResource(&genericSkyBoxRSontext, &globalBufferLocation, 0, 1, 0);
	skyboxDesc.BindSampledImageToShaderResource(&genericSkyBoxRSontext, &skyboxCubeImage, &viewIndex, &mainLinearSampler, 1, 0, 0);
	skyboxDesc.UploadConstant(&genericSkyBoxRSontext, &matrix, 0);

	if (genericSkyBoxRSontext.contextFailed)
	{
		genericSkyBoxRSontext.contextLogger->ProcessMessage();
		return -1;
	}

	std::array<ShaderResourceSetHandle, 2> skyboxDescs = {  camSkyboxData(), skyboxDesc() };

	GraphicsIntermediaryPipelineInfo skyboxInfo = {
		.drawType = 0,
		.vertexBufferHandle = vertexAlloc,
		.vertexCount = 24,
		.pipelinename = SKYBOX,
		.descCount = 2,
		.descriptorsetid = skyboxDescs.data(),
		.indexBufferHandle = globalIndexBuffer,
		.indexCount = 36,
		.instanceCount = 1,
		.indexSize = 2,
		.indirectAllocation = ~0,
		.indirectDrawCount = 0,
		.indirectCountAllocation = ~0
	};

	skyboxPipeline = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &skyboxInfo);

	if (skyboxPipeline < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Skybox Pipeline failed creation"));
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

	RenderInstanceCreateInfo riCreateInfo{};
	riCreateInfo.maxAttachmentGraphTemplates = mainLayoutAttachments.size();
	riCreateInfo.maxAttachmentGraphInstances = mainLayoutAttachments.size();
	riCreateInfo.maxImagePoolsCount = 10;
	riCreateInfo.maxBufferPoolsCount = 10;
	riCreateInfo.maxRenderTargets = 20;
	riCreateInfo.maxShaderGraphs = 50;
	riCreateInfo.maxShaderHandles = 60;
	riCreateInfo.maxDescriptorManagers = 1;
	riCreateInfo.maxShaderResourceTemplates = 60;
	riCreateInfo.maxComputeQueues = 10;
	riCreateInfo.maxRenderQueues = 10;
	riCreateInfo.maxPipelineTemplates = 15;
	riCreateInfo.maxPipelineInstances = 25;
	riCreateInfo.maxPipelineHandles = 25;
	riCreateInfo.maxAllocations = 50;
	riCreateInfo.maxSubAllocations = 30;
	riCreateInfo.maxGPUCommandsStreams = 1;
	riCreateInfo.maxTextureHandles = 100;
	riCreateInfo.maxSamplerHandles = 1;
	riCreateInfo.maxResourceStatuses = 125;
	riCreateInfo.commandBuffersSize = 16 * KiB;
	riCreateInfo.commandsCacheSize = 64 * KiB;
	riCreateInfo.internalLoggerRingSize = 8 * KiB;
	riCreateInfo.numberOfDriverHostAllocations = 800;
	riCreateInfo.numberOfTransferCommandAllocations = 20;
	riCreateInfo.numberOfResourceUpdateAllocations = 30;
	riCreateInfo.numberOfDriverDeviceAllocations = 60;
	riCreateInfo.numberOfImageMemoryAllocations = 30;
	riCreateInfo.maxWindows = 1;
	riCreateInfo.maxSwapChains = 1;
	riCreateInfo.maxGPUS = 1;
	riCreateInfo.maxLogicalDevices = 1;
	riCreateInfo.maxConcurrentRecordings = 1;

	OSGetSTDOutput(&riCreateInfo.internalRendererHandle);

	GPUFeatureRequest request{};

	request.desiredMaxImageWidth = 4096;
	request.desiredMaxImageHeight = 4096;
	request.deviceType = DISCRETE | INTEGRATED;
	request.requireDescriptorBindingPartiallyBound = true;
	request.requireDescriptorBindingSampledImageUpdateAfterBind = true;
	request.requireDescriptorBindingUpdateUnusedWhilePending = true;
	request.requireDescriptorBindingVariableDescriptorCount = true;
	request.requireShaderSampledImageArrayNonUniformIndexing = true;
	request.requireStorageBuffer8BitAccess = true;
	request.requireDrawIndirectCount = true;
	request.requireRuntimeDescriptorArray = true;
	request.requireGeometryShader = false;
	request.requireTextureCompressionBC = true;
	request.requireTessellationShader = false;
	request.requireSamplerAnisotropy = true;
	request.requireMultiDrawIndirect = true;
	request.requireWideLines = true;
	request.requireTimelineSemaphores = true;

	LogicalDeviceFeatures deviceFeatures{};

	deviceFeatures.useSPVDebugInfo = true;
	deviceFeatures.useSPVDrawParameters = true;
	deviceFeatures.useSwapChain = true;
	deviceFeatures.useSwapChainMaintenance = true;

	GlobalRenderer::gRenderInstance.CreateRenderInstance(&riCreateInfo, &RenderInstanceMemoryAllocator, &RenderInstanceTemporaryAllocator);

	GlobalRenderer::gRenderInstance.CreateHighLevelInstance(800 * KiB, 128 * KiB, 4 * KiB, 96 * KiB);

	OSWindowInternalData internalWindowData;
	
	mainWindow.GetInternalData(&internalWindowData);

	mainPresentationWindow = GlobalRenderer::gRenderInstance.CreateWindowedSurface(&internalWindowData);

	int foundPhysicalDevice = GlobalRenderer::gRenderInstance.OpenPhysicalDevicePicker();

	mainGPU = GlobalRenderer::gRenderInstance.CreatePhysicalDeviceAdapterWithQuerying(&request, &deviceFeatures);

	GlobalRenderer::gRenderInstance.ClosePhysicalDevicePicker();

	LogicalDeviceCreateInfo lDeviceCreateInfo{};

	lDeviceCreateInfo.maxQueries = 10;
	lDeviceCreateInfo.physicalDeviceIndex = mainGPU;
	lDeviceCreateInfo.surfaceIndexForPresent = mainPresentationWindow;
	lDeviceCreateInfo.requestedDeviceFeatures = &deviceFeatures;
	lDeviceCreateInfo.requestedPhysicalFeatures = &request;
	lDeviceCreateInfo.deviceInstCacheSize = 96 * KiB;
	lDeviceCreateInfo.deviceInstHandleSize = 16 * KiB;
	lDeviceCreateInfo.deviceInstPermanentSize = 32 * KiB;
	lDeviceCreateInfo.driverCacheSize = 16 * KiB;
	lDeviceCreateInfo.driverPermanentSize = 4 * MiB;

	mainLogicalDevice = GlobalRenderer::gRenderInstance.CreateLogicalDevice(&lDeviceCreateInfo);

	GlobalRenderer::gRenderInstance.CreatePerFrameStagingBuffers(mainLogicalDevice, 128 * MiB);

	std::array<DescriptorTypes, 4> descriptorTypes =
	{
		DescriptorTypes::UNIFORM_DESCRIPTOR,
		DescriptorTypes::UNORDERED_ACCESS_DESCRIPTOR,
		DescriptorTypes::SAMPLER_DESCRIPTOR,
		DescriptorTypes::SAMPLED_IMAGE_DESCRIPTOR,
	};

	std::array<uint32_t, 4> descriptorCounts =
	{
		50,
		50,
		10,
		50,
	};

	mainDescriptorManagerIndex = GlobalRenderer::gRenderInstance.CreateDescriptorHeap(mainLogicalDevice, descriptorTypes.data(), descriptorCounts.data(), 4, 100, 50);

	size_t mainHostSize = 128 * MiB;
	size_t mainDeviceSize = 64 * MiB;

	mainDeviceBuffer = GlobalRenderer::gRenderInstance.CreateUniversalBuffer(mainLogicalDevice, mainDeviceSize, MemoryTypeBits::DEVICE_MEMORY_TYPE);
	mainHostBuffer = GlobalRenderer::gRenderInstance.CreateUniversalBuffer(mainLogicalDevice, mainHostSize, MemoryTypeBits::HOST_MEMORY_COHERENT_TYPE);

	mainHostAllocator.dataAllocator = 0;
	mainHostAllocator.dataSize = mainHostSize;
	mainDeviceAllocator.dataSize = mainDeviceSize;
	mainDeviceAllocator.dataAllocator = 0;

	ImageFormat requestedColorFormats = ImageFormat::B8G8R8A8;

	ImageFormat mainColorFormat = GlobalRenderer::gRenderInstance.FindSupportedBackBufferColorFormat(mainGPU, mainPresentationWindow, &requestedColorFormats, 1);

	ImageFormat requestedDSVFormats = ImageFormat::D24UNORMS8STENCIL;

	ImageFormat mainDepthFormat = GlobalRenderer::gRenderInstance.FindSupportedDepthFormat(mainLogicalDevice, &requestedDSVFormats, 1);

	size_t mainRTVSize = 800 * MiB, mainDSVSize = 1024 * MiB;

	mainRTVIndex = GlobalRenderer::gRenderInstance.CreateImagePool(
		mainLogicalDevice, mainRTVSize, mainColorFormat, 4096, 4096,
		ImageUsageFlagBits::TRANSFER_SRC | ImageUsageFlagBits::TRANSFER_DEST
		| ImageUsageFlagBits::COLOR_ATTACHMENT,
		MemoryTypeBits::DEVICE_MEMORY_TYPE
		);

	mainDSVIndex = GlobalRenderer::gRenderInstance.CreateImagePool(
		mainLogicalDevice, mainDSVSize, mainDepthFormat, 4096, 4096,
		ImageUsageFlagBits::TRANSFER_SRC | ImageUsageFlagBits::TRANSFER_DEST|
		ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::DEPTH_ATTACHMENT | 
		ImageUsageFlagBits::STENCIL_ATTACHMENT,
		MemoryTypeBits::DEVICE_MEMORY_TYPE);

	mainRTVSlab.dataAllocator = 0;
	mainRTVSlab.dataSize = mainRTVSize;

	mainDSVSlab.dataAllocator = 0;
	mainDSVSlab.dataSize = mainDSVSize;

	MSAAPost = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(mainLogicalDevice, &mainLayoutAttachments[0], nullptr);
	BasicShadow = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(mainLogicalDevice, &mainLayoutAttachments[1], nullptr);
	MSAAShadowMapping = GlobalRenderer::gRenderInstance.CreateAttachmentGraph(mainLogicalDevice, &mainLayoutAttachments[2], nullptr);

	currentFrameGraphIndex = MSAAShadowMapping;

	frameGraphsCount = mainLayoutAttachments.size();

	mainPresentationSwapChain = GlobalRenderer::gRenderInstance.CreateSwapChainHandle(mainLogicalDevice, mainPresentationWindow, mainColorFormat, 800, 600);

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

	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, BasicShadow, 0, ShadowMapViewerClears.data(), &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, MSAAPost, 1, MSAAPostClears.data(), &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
	GlobalRenderer::gRenderInstance.CreateSwapChainAttachment(mainLogicalDevice, mainPresentationSwapChain, MSAAShadowMapping, 1, MSAAShadowMappingClears.data(), &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
	
	GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(mainLogicalDevice, MSAAPost, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, 800, 600, MSAAPostClears.data(), &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);
	GlobalRenderer::gRenderInstance.CreatePerFrameAttachment(mainLogicalDevice, MSAAShadowMapping, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT, mainShadowWidth, mainShadowHeight, MSAAShadowMappingClears.data(), &mainRTVSlab, &mainDSVSlab, mainRTVIndex, mainDSVIndex);

	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAPost, 0, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAPost, 1, 1);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAShadowMapping, 0, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(MSAAShadowMapping, 1, 10);
	GlobalRenderer::gRenderInstance.CreateGraphicsQueueForAttachments(BasicShadow, 0, 1);

	GlobalRenderer::gRenderInstance.CreatePipelines(pds.data(), pds.size());

	GlobalRenderer::gRenderInstance.CreateShaderGraphs(mainLogicalDevice, layouts.data(), layouts.size());

	mainLinearSampler = GlobalRenderer::gRenderInstance.CreateSampler(mainLogicalDevice, 7);

	std::array frameGraphs = { MSAAPost, MSAAShadowMapping, BasicShadow };
	std::array frameRenderPassSelection = { 0, 1, 0 };

	std::array fullScreenFrameGraphs = { MSAAPost, BasicShadow };

	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, GENERIC, 0, frameGraphs.data(), frameRenderPassSelection.data(),  2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, TEXT, 1, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, DEBUGDRAW, 2, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, NORMALDEBUGDRAW, 3, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, SKYBOX, 4, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, OUTLINE, 5, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, FULLSCREEN, 6, fullScreenFrameGraphs.data(), frameRenderPassSelection.data() + 1, 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, SHADOWMAPDRAW, 7, frameGraphs.data() + 1, frameRenderPassSelection.data(), 1);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, JOINTVISUAL, 8, frameGraphs.data(), frameRenderPassSelection.data(), 2);
	GlobalRenderer::gRenderInstance.CreateGraphicRenderStateObject(mainLogicalDevice, UIDRAWING, 9, frameGraphs.data(), frameRenderPassSelection.data(), 2);

	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, INTERPOLATE);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, POLYNOMIAL);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, RENDEROBJCULL);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, DEBUGCULL);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, PREFIXSUM);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, PREFIXADD);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, WORLDORGANIZE);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, WORLDASSIGN);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, LIGHTORGANIZE);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, LIGHTASSIGN);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, SHADOWMAPCULL);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UICULLING);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UIDEPTHCOUNT);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UICHILDDEPTHADD);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UIINDEXASSIGNMENT);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UILAYOUTSIZES);
	GlobalRenderer::gRenderInstance.CreateComputePipelineStateObject(mainLogicalDevice, UIABSOLUTEPOSITION);

	mainComputeQueueIndex = GlobalRenderer::gRenderInstance.CreateComputeQueue(15);

	CreateTexturePools();

	mainCommandStreamIndex = GlobalRenderer::gRenderInstance.CreateGPUCommandStream(10);

	globalBufferLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, (sizeof(Matrix4f) * 3) + sizeof(Frustum), 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::UNIFORM_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalIndexBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, globalIndexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	globalVertexBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainDeviceBuffer, globalVertexBufferSize, 1, 16, AllocationType::STATIC, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainDeviceAllocator);
	
	ShaderResourceSetBuilder globalBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, GENERIC, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
	ShaderResourceSetBuilder globalTexturesDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, GENERIC, 1, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);

	globalBufferDescriptor = globalBufferDescriptorB();
	globalTexturesDescriptor = globalTexturesDescriptorB();

	globalMeshLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalMeshSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalMaterialsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalMaterialsSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	globalLightBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalLightBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalLightTypesBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalLightTypesBufferSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);

	globalDebugStructAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalDebugStructAllocSize, 1, alignof(Matrix4f), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalDebugTypesAlloc = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), globalDebugStructMaxCount, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);

	globalMaterialIndicesLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalMaterialIndicesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalRenderableLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	globalBlendDetailsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalBlendDetailsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalBlendRangesLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalBlendRangesSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::NO_BUFFER_ALIGNMENT, &mainHostAllocator);

	globalGeometryDescriptionsLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalGeometryDescriptionsSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalGeometryRenderableLocation = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, globalGeometryRenderableSize, 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	jointMeshWorldMatrix = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(Matrix4f), 1, 64, AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	jointMeshParentIndices = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, jointMeshWorldMatrixMaxCount * sizeof(uint32_t), 1, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);

	ShaderResourceSetContext globalDescriptorBuilder{ &mainAppLogger, false };

	globalBufferDescriptorB.BindBufferToShaderResource(&globalDescriptorBuilder, &globalBufferLocation, 0, 1, 0);

	globalTexturesDescriptorB.SetVariableArrayCount(&globalDescriptorBuilder, 3, 512);

	int creationRetSB = CreateSkyBox();
	int creationRetDB = CreateDebugCommandBuffers(globalDebugStructMaxCount);
	int creationRetLW = CreateLightAssignments(globalLightMax);
	int creationRetMW = CreateMeshWorldAssignment(mainGrid.numberOfDivision * mainGrid.numberOfDivision * mainGrid.numberOfDivision);
	int creationRetGMB = CreateGenericMeshCommandBuffers(globalMeshCountMax);
	int creationRetSM = CreateShadowMapManager(4, globalMeshCountMax, 1024, 1024, mainShadowWidth, mainShadowHeight);
	int creationRetFS = CreateMSAAPostFullScreen();

	CreateUITools(25, 5);

	if (creationRetSB < 0 ||
		creationRetDB < 0 ||
		creationRetLW < 0 ||
		creationRetMW < 0 ||
		creationRetGMB < 0 ||
		creationRetSM < 0 ||
		creationRetFS < 0)
	{
		mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Shutting down environment, cannot create minimal sandbox"));
		return;
	}

	AddLight(mainSpotLight, LightType::SPOT);
	AddLight(mainDirectionalLight, LightType::DIRECTIONAL);
	
	c.CamLookAt(Vector3f(25.0 * -mainDirectionalLight.direction.x, 25.0 * -mainDirectionalLight.direction.y,  25.0 * -mainDirectionalLight.direction.z), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));

	c.CreateProjectionMatrix(GlobalRenderer::gRenderInstance.GetSwapChainWidth(mainPresentationSwapChain) / (float)GlobalRenderer::gRenderInstance.GetSwapChainHeight(mainPresentationSwapChain), 0.1f, 10000.0f, DegToRad(45.0f));

	UpdateCameraMatrix();

	
}

void ApplicationLoop::CleanupRuntime()
{
	stopThreadServer = true;

	GlobalRenderer::gRenderInstance.WaitOnRender(mainLogicalDevice);

	cleaned = true;
}

bool ProcessCommands()
{
	queueSema.Wait();
	if (!wordCounts.size()) {
		queueSema.Notify();
		return true;
	}
	int wordCount = wordCounts.front();

	wordCounts.pop();
	queueSema.Notify();

	StringView* commandType = (StringView*)(ThreadSharedMessageQueue.bufferLocation);

	return ExecuteCommands(*commandType, wordCount);
}

void AddCommandTS(int wordCount)
{
	SemaphoreGuard lock(queueSema);
	wordCounts.push(wordCount);
}

void LoadObject(const StringView& file)
{
	SMBFile SMB{};

	int fileSize = SMB.OpenFile(file);

	if (fileSize <= 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot open SMB file for viewing"));
		return;
	}

	int arenaIndex = FindSMBArenaForUse(fileSize);

	if (arenaIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGWARNING, STRING_VIEW_FROM_LITERAL("Cannot find arena for loading mesh"));
		return;
	}

	int loadSmbRet = SMB.LoadFile(&SMBThreadedFileInputAllocators[arenaIndex], &mainAppLogger);

	if (loadSmbRet)
	{	
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("SMB Load Failed"));
	}
	else 
	{
		ProcessSMBFile(&SMB, arenaIndex);
	}

	ReturnSMBArena(arenaIndex);
}

void LoadThreadedWrapper(StringView& file)
{
	ThreadManager::LaunchOSASyncThread(LoadObjectThreaded, &file);
}

int FindWords(const char* words, int charCount)
{
	int wordCount = 0;
	size_t size = charCount;
	int i = 0, j = 1;
	int wordSize = 0; 
	while (j < size) {

		if (words[j] == 0x20)
		{
			wordSize = j - i;
			if (wordSize > 1) {
				StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
				char* stringData = (char*)ThreadSharedStringViewAllocator.Allocate(wordSize);
				out->stringData = stringData;
				out->charCount = wordSize;
				memcpy(stringData, words + i, wordSize);
				wordCount++;
			}
			j++;
			i = j;
		}
		else if (words[j] == 0x22) //capture quote
		{
			j++;
			while (j <= size - 1 && words[j++] != 0x22);

			wordSize = j - i;
			StringView* out = (StringView*)ThreadSharedMessageQueue.AcquireWrite(sizeof(StringView));
			char* stringData = (char*)ThreadSharedStringViewAllocator.Allocate(wordSize);
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
		char* stringData = (char*)ThreadSharedStringViewAllocator.Allocate(wordSize);
		out->stringData = stringData;
		out->charCount = wordSize;
		memcpy(stringData, words + i, wordSize);
	}

	return wordCount;
}

int CreateMaterialRange(int gpuMaterialRangeID, int count, int* ids)
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(ids, globalMaterialIndicesLocation, sizeof(uint32_t) * count, sizeof(uint32_t) * gpuMaterialRangeID, TransferType::CACHED);

	return 0;
}

int CreateRenderable(int meshCPURenderableIndex, int meshGPURenderableIndex, const Matrix4f& mat, int geomIndex,  int materialStart, int materialCount, int blendStart, int meshIndex, int instanceCount)
{
	RenderableMeshCPUData* meshCpuRenderable = nullptr;

	meshCpuRenderable = renderablesMeshObjects.Get(meshCPURenderableIndex);

	if (materialStart < 0 ||
		materialCount < 0 || materialCount > MAX_GPU_MATERIALS ||
		blendStart < 0 ||
		meshIndex < 0 ||
		instanceCount < 1)
	{
		mainAppLogger.AddLogMessage(LOGWARNING, STRING_VIEW_FROM_LITERAL("Data validation for GPU Renderable incorrect"));
	}

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

int CreateMaterial(
	int gpuMaterialID,
	int flags,
	int texturesIDs,
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
		ptr[i] = texturesIDs + i;
	}

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * gpuMaterialID, TransferType::CACHED);

	return 0;
}

int CreateMaterial(
	int gpuMaterialID,
	int flags,
	int texturesIDs,
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
		ptr[i] = texturesIDs + i;
	}

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&material, globalMaterialsLocation, sizeof(GPUMaterial), sizeof(GPUMaterial) * gpuMaterialID, TransferType::CACHED);

	return 0;
}


int CreateMeshHandle(
	int meshCPUDataIndex, int meshGPUDataIndex,
	int vertexFlags, int vertexCount, int vertexStride,
	int indexStride, int indexCount,
	Sphere& sphere,
	int gpuVertexAlloc, int gpuIndexAlloc,
	int cpuVertexAlloc, int cpuIndexAlloc
)
{
	MeshCPUData* mesh = nullptr;

	mesh = meshCPUData.Get(meshCPUDataIndex);

	if (indexCount <= 0 || 
		vertexCount <= 0 || 
		vertexStride <= 0 || 
		vertexFlags == 0 || 
		indexStride <= 0 || 
		gpuIndexAlloc < 0 || 
		gpuVertexAlloc < 0
		)
	{
		mainAppLogger.AddLogMessage(LOGWARNING, STRING_VIEW_FROM_LITERAL("Data validation is incorrect for GPU Mesh Handle"));
	}

	mesh->indicesCount = indexCount;

	mesh->verticesCount = vertexCount;

	mesh->vertexSize = vertexStride;

	mesh->indexSize = indexStride;

	mesh->meshDeviceIndex = meshGPUDataIndex;

	mesh->vertexFlags = vertexFlags;

	mesh->deviceIndices = gpuIndexAlloc;

	mesh->deviceVertices = gpuVertexAlloc;

	mesh->cpuVertices = cpuVertexAlloc;

	mesh->cpuIndices = cpuIndexAlloc;

	GPUMeshDetails handles{};

	handles.vertexFlags = vertexFlags;
	handles.stride = vertexStride;

	handles.indexCount = mesh->indicesCount;
	handles.firstIndex = gpuIndexAlloc / mesh->indexSize;
	handles.vertexByteOffset = gpuVertexAlloc;

	memcpy(&handles.sphere, &sphere, sizeof(Vector4f));

	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&handles, globalMeshLocation, sizeof(GPUMeshDetails), meshGPUDataIndex * sizeof(GPUMeshDetails), TransferType::CACHED);

	return 0;
}

void SetPositionOfGeometry(int geomIndex, const Vector3f& pos)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;
	
	RenderableGeomCPUData* geom = nullptr;

	if (geomIndex >= renderablesGeomObjects.count)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Incorrect Geometry Location"));
		return;
	}

	geom = renderablesGeomObjects.Get(geomIndex);

	int geomRenderable = geom->geomIndex;

	Matrix4f transform = Identity4f();;

	transform.axes.translate = Vector4f(pos.x, pos.y, pos.z, 1.0f);

	rendInst.UpdateDriverMemory(&transform, globalGeometryRenderableLocation, sizeof(Matrix4f), (sizeof(GPUMeshRenderable) * geomRenderable) + 16, TransferType::CACHED);
}

bool ExecuteCommands(const StringView& command, int wordCount)
{
	bool stillRunning = true;

	StringView* args = (StringView*)(ThreadSharedMessageQueue.bufferLocation);
	if (strncmp(command.stringData, "load", 4) == 0)
	{
		for (int i = 1; i<wordCount; i++)
			LoadThreadedWrapper(args[i]);
	}
	else if (strncmp(command.stringData, "end", 4) == 0)
	{
		stillRunning = false;
	}
	else if (strncmp(command.stringData, "positiong", 9) == 0)
	{
		if (wordCount != 5)
			return stillRunning;

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
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Unprocessed command entered"));
	}

	ThreadSharedMessageQueue.Read();

	return stillRunning;
}

int CreateGPUGeometryDetails(int geometryDetailsIndex, const AxisBox& minMaxBox)
{
	auto& rendInst = GlobalRenderer::gRenderInstance;

	GPUGeometryDetails details{};

	details.minMaxBox = minMaxBox;

	rendInst.UpdateDriverMemory(&details, globalGeometryDescriptionsLocation, sizeof(GPUGeometryDetails), sizeof(GPUGeometryDetails) * geometryDetailsIndex, TransferType::CACHED);

	return 0;
}

int CreateGPUGeometryRenderable(int geomGPURenderableIndex, const Matrix4f& matrix, int geomDesc, int renderableStart, int renderableCount)
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

	OSFileHandle stdIn;

	OSGetSTDInput(&stdIn);
	uint64_t numberOfBytesRead;
	DWORD events, readReturn = 0;
	INPUT_RECORD record;

	char inputBuffer[1024];

	mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Ready for commands... \nHit enter and then write command > "));

	volatile bool* stopToken = (volatile bool*)data;

	while (!*stopToken)
	{
		int ret = OSPollFile(&stdIn, 500);

		if (ret < 0) continue;

		BOOL success = ReadConsoleInput(stdInHandle, &record, 1, &events);

		if (!success) {
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot get ReadConsoleInput\n"));
			break;
		}

		switch (record.EventType) {
		case KEY_EVENT:
			if (record.Event.KeyEvent.bKeyDown) {
				if (record.Event.KeyEvent.uChar.AsciiChar == VK_RETURN)
				{
					break;
				}
			}
		default:
			continue;
		}

		readReturn = OSReadFile(&stdIn, 1024, inputBuffer, &numberOfBytesRead);

		if (readReturn < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot get ReadFile from stdinput\n"));
			break;
		}

		if (numberOfBytesRead <= 2)
			continue;

		int wordCount = FindWords(inputBuffer, numberOfBytesRead - 2);

		AddCommandTS(wordCount);

		mainAppLogger.AddLogMessage(LOGINFO, STRING_VIEW_FROM_LITERAL("Hit enter and then write command > "));
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

				CreateAABBDebugStruct(center, half, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.0, 0.0, 0.0, 0.0));
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

	char fileName[125];

	memcpy(fileName, file->stringData + offset, charCount);

	StringView truncatedView{ fileName, charCount };

	LoadObject(truncatedView);
}

void ProcessKeys(GenericKeyAction keyActions[KC_COUNT])
{

	camMovements[RIGHT] = (keyActions[KC_D].state == HELD || keyActions[KC_D].state == PRESSED);

	camMovements[LEFT] = (keyActions[KC_A].state == HELD || keyActions[KC_A].state == PRESSED);

	camMovements[FORWARD] = (keyActions[KC_W].state == HELD || keyActions[KC_W].state == PRESSED);

	camMovements[BACK] = (keyActions[KC_S].state == HELD || keyActions[KC_S].state == PRESSED);

	camMovements[PITCHUP] = (keyActions[KC_UP].state == HELD || keyActions[KC_UP].state == PRESSED);

	camMovements[PITCHDOWN] = (keyActions[KC_DOWN].state == HELD || keyActions[KC_DOWN].state == PRESSED);

	camMovements[ROTATEYRIGHT] = (keyActions[KC_RIGHT].state == HELD || keyActions[KC_RIGHT].state == PRESSED);

	camMovements[ROTATEYLEFT] = (keyActions[KC_LEFT].state == HELD || keyActions[KC_LEFT].state == PRESSED);


	if (keyActions[KC_TWO].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance.IncreaseMSAA(currentFrameGraphIndex, 0);
	}

	if (keyActions[KC_ONE].state == PRESSED)
	{
		GlobalRenderer::gRenderInstance.DecreaseMSAA(currentFrameGraphIndex, 0);
	}

	static int dir = -1;

	static bool clamped = false;

	if (keyActions[KC_Q].state == PRESSED && !clamped)
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

		GlobalRenderer::gRenderInstance.ResetCommandList(mainCommandStreamIndex);
		GlobalRenderer::gRenderInstance.AddCommandQueue(mainCommandStreamIndex, mainComputeQueueIndex, COMPUTE_QUEUE_COMMANDS);
		GlobalRenderer::gRenderInstance.AddCommandQueue(mainCommandStreamIndex, currentFrameGraphIndex, ATTACHMENT_COMMANDS);

		clamped = true;
	}
	else if (keyActions[KC_Q].state == RELEASED && clamped)
	{
		clamped = false;
	}
}


void UpdateLight(GPULightSource& lightDesc, int lightIndex)
{
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&lightDesc, globalLightBuffer, sizeof(GPULightSource), sizeof(GPULightSource) * lightIndex, TransferType::CACHED);
}

int ReadCubeImage(StringView* name, int textureCount, TextureIOType ioType)
{
	TextureDetails details;

	OSFileHandle outHandle{};

	int nRet = OSOpenFile(name->stringData, name->charCount, READ, &outHandle);

	if (nRet)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot open cube image file"));
		return -1;
	}

	int size = outHandle.fileLength;

	void* fileData = GlobaScratchAllocator.Allocate(size);

	uint64_t readCount = 0;

	int nRead = OSReadFile(&outHandle, size, (char*)fileData, &readCount);

	OSCloseFile(&outHandle);

	if (nRead < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot read cube image file"));
		return -1;
	}


	int filePointer = ReadBMPDetails((char*)fileData, &details);

	details.data = (char*)mainDictionary.AllocateImageCache(details.dataSize);

	details.currPointer = details.data;

	for (int i = 1; i < textureCount; i++)
	{
		ReadBMPData((char*)fileData, filePointer, &details);

		nRet = OSOpenFile(name[i].stringData, name[i].charCount, READ, &outHandle);

		if (nRet)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot open cube image file"));
			return -1;
		}

		size = outHandle.fileLength;

		fileData = GlobaScratchAllocator.Allocate(size);

		nRead = OSReadFile(&outHandle, size, (char*)fileData, &readCount);

		OSCloseFile(&outHandle);

		if (nRead < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot read cube image file"));
			return -1;
		}

		TextureDetails stubDetails{};

		filePointer = ReadBMPDetails((char*)fileData, &stubDetails);

		if (stubDetails.width != details.width || stubDetails.height != details.height) {
			mainAppLogger.AddLogMessage(LOGWARNING, STRING_VIEW_FROM_LITERAL("Cube Image Dimension mismatch"));
		}

		details.currPointer = (char*)mainDictionary.AllocateImageCache(details.dataSize);
	}

	ReadBMPData((char*)fileData, filePointer, &details);

	details.arrayLayers = textureCount;

	DeviceSlabAllocator* perFormatAllocator = nullptr;

	int poolIndex = GetPoolIndexByFormat(details.type, &perFormatAllocator);

	size_t actualMemorySize = 0, actualMemoryAlignment = 0, actualMemoryAddress = 0;

	GlobalRenderer::gRenderInstance.GetGPURequestedImageSizeAndAlignment(mainLogicalDevice,
		details.width, details.height, details.miplevels, details.arrayLayers, details.type, ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST, &actualMemorySize, &actualMemoryAlignment
	);

	actualMemoryAddress = perFormatAllocator->Allocate(actualMemorySize, actualMemoryAlignment);

	int cubeImageIndex = 
		GlobalRenderer::gRenderInstance.CreateImageHandle(
			mainLogicalDevice,
			actualMemoryAddress,
			details.width,
			details.height,
			details.miplevels,
			6,
			details.type,
			ImageType::IMAGE_CUBE,
			ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST,
			poolIndex
		);


	if (cubeImageIndex >= 0)
	{
		int viewIndex = GlobalRenderer::gRenderInstance.CreateImageView(mainLogicalDevice, cubeImageIndex, 0, IMAGE_VIEW_ALL_MIPS, 0, IMAGE_VIEW_ALL_LAYERS, COLOR_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

		GlobalRenderer::gRenderInstance.UpdateImageMemory(
			details.data,
			cubeImageIndex,
			textureCount * details.dataSize,
			details.width,
			details.height,
			details.miplevels,
			0,
			details.arrayLayers,
			0,
			COLOR_IMAGE_ASPECT
		);
	}
	else
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Failed to register image to renderer"));
	}

	return cubeImageIndex;
}

int Read2DImage(StringView* name, int mipCounts, TextureIOType ioType)
{
	TextureDetails* details;

	int textureStart = mainDictionary.AllocateNTextureHandles(1, &details);

	int totalBlobSize = 0;

	int twoDimageIndex = -1;

	for (int i = 0; i < mipCounts; i++)
	{
		TextureDetails stubDetails{};

		OSFileHandle outHandle{};

		int nRet = OSOpenFile(name[i].stringData, name[i].charCount, READ, &outHandle);

		if (nRet)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot open 2d image file"));
			return -1;
		}

		int size = outHandle.fileLength;

		void* fileData = GlobaScratchAllocator.Allocate(size);
		
		uint64_t readCount = 0;

		int nRead = OSReadFile(&outHandle, size, (char*)fileData, &readCount);

		OSCloseFile(&outHandle);

		if (nRead < 0)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Cannot read 2d image file"));
			return -1;
		}

		TextureDetails* currDetailPtr = (!i) ? details : &stubDetails;

		int filePointer = ReadBMPDetails((char*)fileData, currDetailPtr);

		currDetailPtr->data = (char*)mainDictionary.AllocateImageCache(currDetailPtr->dataSize);

		currDetailPtr->currPointer = currDetailPtr->data;

		totalBlobSize += currDetailPtr->dataSize;

		ReadBMPData((char*)fileData, filePointer, currDetailPtr);
	}

	details->miplevels = mipCounts;
	details->arrayLayers = 1;

	DeviceSlabAllocator* perFormatAllocator = nullptr;

	int poolIndex = GetPoolIndexByFormat(details->type, &perFormatAllocator);

	size_t actualMemorySize = 0, actualMemoryAlignment = 0, actualMemoryAddress = 0;

	GlobalRenderer::gRenderInstance.GetGPURequestedImageSizeAndAlignment(mainLogicalDevice,
		details->width, details->height, details->miplevels, details->arrayLayers, details->type, ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST, &actualMemorySize, &actualMemoryAlignment
	);

	actualMemoryAddress = perFormatAllocator->Allocate(actualMemorySize, actualMemoryAlignment);

	twoDimageIndex = GlobalRenderer::gRenderInstance.CreateImageHandle(mainLogicalDevice,
			actualMemoryAddress,
			details->width,
			details->height,
			details->miplevels,
			details->arrayLayers,
			details->type,
			ImageType::IMAGE_2D,
			ImageUsageFlagBits::SAMPLED | ImageUsageFlagBits::TRANSFER_DEST,
			poolIndex
		);

	if (twoDimageIndex >= 0)
	{
		int viewIndex = GlobalRenderer::gRenderInstance.CreateImageView(mainLogicalDevice, twoDimageIndex, 0, IMAGE_VIEW_ALL_MIPS, 0, IMAGE_VIEW_ALL_LAYERS, COLOR_IMAGE_ASPECT, ImageLayout::SHADERREADABLE);

		GlobalRenderer::gRenderInstance.UpdateImageMemory(
			details->data,
			twoDimageIndex,
			totalBlobSize,
			details->width,
			details->height,
			details->miplevels,
			0,
			details->arrayLayers,
			0,
			COLOR_IMAGE_ASPECT
		);

		DeviceHandleArrayUpdateTextureView arrayUpdateStruct{};

		arrayUpdateStruct.imageHandle = twoDimageIndex;
		arrayUpdateStruct.viewIndex = viewIndex;

		mainDictionary.textureHandles[textureStart] = twoDimageIndex;

		DeviceHandleArrayUpdate update;

		update.updateType = DeviceHandleArrayUpdateType::TEXTURE_VIEW_UPDATE;
		update.resourceCount = 1;
		update.resourceDstBegin = textureStart;
		update.resourceHandles = &arrayUpdateStruct;

		GlobalRenderer::gRenderInstance.UpdateShaderResourceArray(globalTexturesDescriptor, 3, ShaderResourceType::IMAGE2D, &update);
	}
	else
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Failed to register 2d image to renderer"));

		return -1;
	}

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

	int actualSize = 0;

	//allocDetails = RenderInstanceMemoryAllocator.GetUsageAndCapacity();

	actualSize = snprintf(StringBuffer, 512, "Render Instance memory allocation %d/%d", allocDetails.first, allocDetails.second);

	mainAppLogger.AddLogMessage(LOGINFO, StringBuffer, actualSize);

	allocDetails = GlobaScratchAllocator.GetUsageAndCapacity();

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
}


int AllocateCPUGeometryDetails(int numberOfDetails) {
	mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("AllocateCPUGeometryDetails : unimplemented"));
	return -1;
}

int AllocateGPUGeometryDetails(int numberOfDetails) 
{
	int geomDescriptionsIndex = -1;

	if (((geomDescriptionsIndex = globalGeometryDescriptionsCount.fetch_add(numberOfDetails)) + numberOfDetails) > globalGeometryDescriptionsCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many geometry details created"));
		return -1;
	}

	return geomDescriptionsIndex;
}

int AllocateCPUMeshDetails(int numberOfDetails) 
{	
	int meshIndex = -1;

	meshIndex = meshCPUData.Allocate(numberOfDetails);

	if (meshIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many CPU meshes created"));
	}

	return meshIndex;
}

int AllocateGPUMeshDetails(int numberOfDetails) 
{	
	int meshIndex = -1;

	if (((meshIndex = globalMeshCount.fetch_add(numberOfDetails)) + numberOfDetails) > globalMeshCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many GPU meshes created"));
		return -1;
	}

	return meshIndex;
}

int AllocateCPUMeshRenderable(int numberOfRenderables) 
{
	int meshRenderableIndex = -1;

	meshRenderableIndex = renderablesMeshObjects.Allocate(numberOfRenderables);

	if (meshRenderableIndex < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many CPU Mesh renderables created"));
	}

	return meshRenderableIndex;
}

int AllocateGPUMeshRenderable(int numberOfRenderables) 
{
	int meshRenderableIndex = -1;

	if (((meshRenderableIndex = globalRenderableCount.fetch_add(numberOfRenderables)) + numberOfRenderables) > globalRenderableMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many GPU Mesh renderables created"));
		return -1;
	}

	int* allocQueueWrite = (int*)RenderableAllocQueue.AcquireWrite(sizeof(int));

	*allocQueueWrite = numberOfRenderables;

	return meshRenderableIndex;
}

int AllocateCPUGeometryInstances(int numberOfInstances) 
{	
	int geomRendCPUCode = 0;

	geomRendCPUCode = renderablesGeomObjects.Allocate(numberOfInstances);

	if (geomRendCPUCode < 0)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many CPU geometry renderables created"));
	}

	return geomRendCPUCode;
}

int AllocateGPUGeometryInstances(int numberOfInstances) 
{	
	int geomRenderableIndex = -1;

	if (((geomRenderableIndex = globalGeometryRenderableCount.fetch_add(numberOfInstances)) + numberOfInstances) > globalGeometryRenderableCountMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many GPU geometry renderables created"));
		return -1;
	}

	return geomRenderableIndex;
}

int AllocateGPUMaterialData(int numberOfMaterials)
{
	int materialIndexReturn = -1;

	if (((materialIndexReturn = globalMaterialsIndex.fetch_add(numberOfMaterials)) + numberOfMaterials) > globalMaterialsMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many materials created"));
		return -1;
	}

	return materialIndexReturn;
}

int AllocateGPUMaterialRanges(int numberOfRanges)
{
	int materialRangeIndex = -1;

	if (((materialRangeIndex = globalMaterialRangeCount.fetch_add(numberOfRanges)) + numberOfRanges) > globalMaterialRangeMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many material ranges created"));
		return -1;
	}

	return materialRangeIndex;
}

int AllocateGPUBlendDescriptions(int numberOfDescs)
{
	int blendDescIndex = -1;

	if (((blendDescIndex = globalBlendDetailCount.fetch_add(numberOfDescs)) + numberOfDescs) > globalBlendDetailMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many blend details allocated"));
		return -1;
	}

	return blendDescIndex;
}

int AllocateGPUBlendRanges(int numberOfRanges)
{
	int blendRangeReturn = -1;

	if (((blendRangeReturn = globalBlendRangeCount.fetch_add(numberOfRanges)) + numberOfRanges) > globalBlendRangeMax)
	{
		mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("Too many blend ranges allocated"));
		return -1;;
	}

	return blendRangeReturn;
}

void CreateBitTangentFromNormalTristrips(Vector4f* pos, Vector2f* uvs, uint16_t* indices, int totalIndexCount, int totalVertCount, Vector4f* tangents, Vector3f* outNormals, RingAllocator* tempAllocator)
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

		Vector2f uv1 = uvs[index1];
		Vector2f uv2 = uvs[index2];
		Vector2f uv3 = uvs[index3];

		Vector2f duv1 = uv2 - uv1;
		Vector2f duv2 = uv3 - uv1;

		float f_det = (duv1.x * duv2.y - duv2.x * duv1.y);

		if (f_det == 0.0f)
		{
			continue;
		}

		float f = 1.0f / f_det;

		Vector3f tangent;
		tangent = Normalize((e1 * duv2.y - e2 * duv1.y) * f);

		Vector3f bitangent = Normalize((e2 * duv1.x - e1 * duv2.x) * f);


		Vector3f n = Normalize(normal);
		float sign = (Dot(Cross(n, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

		totalTangents[index1] = totalTangents[index1] + tangent;
		totalTangents[index2] = totalTangents[index2] + tangent;
		totalTangents[index3] = totalTangents[index3] + tangent;

		normals[index1] = normals[index1] + normal;
		normals[index2] = normals[index2] + normal;
		normals[index3] = normals[index3] + normal;

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

int FindSMBArenaForUse(int requestedSize)
{
	int smbArenaIndex = -1;

	for (int i = 0; i < MAX_SMB_ARENAS; i++)
	{
		if (requestedSize > SMBArenasSize[i])
			continue;

		bool expectedCase = false;

		if (arenasUsed[i].compare_exchange_weak(expectedCase, true, std::memory_order_acquire, std::memory_order_relaxed))
		{
			smbArenaIndex = i;
			break;
		}
	}

	return smbArenaIndex;
}

int ReturnSMBArena(int arenaIndex)
{
	arenasUsed[arenaIndex].store(false, std::memory_order_release);
	return 0;
}

int ReadDeferredMessageQueue(CircularMessageQueueMPSC* queue)
{
	int intCount = 0;

	uint64_t totalInts = queue->BytesUnread() / sizeof(int);

	while (totalInts--)
	{
		int newCount = *(int*)queue->Pop(sizeof(int));
		intCount += newCount;
	}

	return intCount;
}

void CreateGPUGenericObjects()
{

#define MAX_OBJECT_DISTANCE 100000.0f

	int geomDetailsIndex = AllocateGPUGeometryDetails(1);

	int meshGPUIndex = AllocateGPUMeshDetails(1);

	int meshCPUIndex = AllocateCPUMeshDetails(1);

	int gpuGeomRenderable = AllocateGPUGeometryInstances(1);

	int gpuMeshRendrables = AllocateGPUMeshRenderable(1);

	int cpuMeshRenderable = AllocateCPUMeshRenderable(1);

	int materialHandleIndex = AllocateGPUMaterialData(1);

	int materialRangeIndex = AllocateGPUMaterialRanges(1);

	int blendDetailsIndex = AllocateGPUBlendDescriptions(1);

	int blendRangesIndex = AllocateGPUBlendRanges(1);
	
	AxisBox box{
		.min = Vector4f(MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, 1.0f),
		.max = Vector4f(MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, 1.0f),
	};

	Sphere sphere{ Vector4f(MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, MAX_OBJECT_DISTANCE, 0.0f) };

	CreateGPUGeometryDetails(geomDetailsIndex, box);

	CreateMeshHandle(
		meshCPUIndex, meshGPUIndex,
		0, 0, 0,
		1, 0,
		sphere,
		0, 0,
		-1, -1
	);

	CreateBlendDetails(blendDetailsIndex, BlendMaterialType::ConstantAlpha, 1.0f);

	CreateBlendRange(blendRangesIndex, &blendDetailsIndex, 1);

	CreateMaterial(materialHandleIndex, 0, -1, 0, Vector4f(1.0, 1.0, 1.0, 1.0), Vector4f(1.0, 1.0, 1.0, 1.0), 1.0, Vector4f(1.0, 1.0, 1.0, 1.0));

	CreateMaterialRange(materialRangeIndex, 1, &materialHandleIndex);

	CreateGPUGeometryRenderable(gpuGeomRenderable, Identity4f(), geomDetailsIndex, gpuMeshRendrables, 1);

	CreateRenderable(cpuMeshRenderable, gpuMeshRendrables, Identity4f(), gpuGeomRenderable, materialRangeIndex, 1, blendRangesIndex, meshGPUIndex, 1);
}




void CreateUITools(int maxUIElements, int maxUIContainers)
{
	globalUIContainerData = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(UIContainer), maxUIContainers, alignof(UIContainer), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalUIElementsIndirectBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(VkDrawIndirectCommand), maxUIContainers + maxUIElements, alignof(VkDrawIndirectCommand), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalUIElementsIndirectCountBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), 2, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalDepthCounts = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), DEPTH_MAX, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalDepthOffsets = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), DEPTH_MAX, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::NO_BUFFER_FORMAT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	//globalChildrenPrefixSumCount = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), maxUIContainers, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalChildrenOffsets = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), maxUIContainers, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalUIIndirectionHandleBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), maxUIContainers, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);
	globalUIIndirectionPositionalHandleBuffer = GlobalRenderer::gRenderInstance.GetAllocFromBuffer(mainLogicalDevice, mainHostBuffer, sizeof(uint32_t), maxUIContainers, alignof(uint32_t), AllocationType::PERFRAME, ComponentFormatType::R32_UINT, BufferAlignmentType::STORAGE_BUFFER_ALIGNMENT, &mainHostAllocator);


	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&mainContainer, globalUIContainerData, sizeof(UIContainer), sizeof(UIContainer) * globalUICount++, TransferType::MEMORY);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&mainRightContainer, globalUIContainerData, sizeof(UIContainer), sizeof(UIContainer) * globalUICount++, TransferType::MEMORY);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&mainCenterContainer, globalUIContainerData, sizeof(UIContainer), sizeof(UIContainer) * globalUICount++, TransferType::MEMORY);
	GlobalRenderer::gRenderInstance.UpdateDriverMemory(&mainLeftContainer, globalUIContainerData, sizeof(UIContainer), sizeof(UIContainer) * globalUICount++, TransferType::MEMORY);

	windowSize.totalUIContainerCount = globalUICount;

	{
		ShaderResourceSetBuilder uiCullingBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UICULLING, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uilDescriptorBuilder{ &mainAppLogger, false };

		uiCullingBufferDescriptorB.BindBufferToShaderResource(&uilDescriptorBuilder, &globalUIElementsIndirectCountBuffer, 0, 1, 1);
		uiCullingBufferDescriptorB.BindBufferToShaderResource(&uilDescriptorBuilder, &globalUIElementsIndirectBuffer, 0, 1, 0);
		uiCullingBufferDescriptorB.BindBufferToShaderResource(&uilDescriptorBuilder, &globalUIContainerData, 0, 1, 2);
		uiCullingBufferDescriptorB.BindBufferView(&uilDescriptorBuilder, &globalUIIndirectionHandleBuffer, 0, 1, 3);
		uiCullingBufferDescriptorB.UploadConstant(&uilDescriptorBuilder, &windowSize, 0);

		if (uilDescriptorBuilder.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> uiCullDescriptors = { uiCullingBufferDescriptorB() };

		ShaderComputeLayout* uiCullDescriptorLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(UICULLING);

		ComputeIntermediaryPipelineInfo uiCullPipelineCreate = {
				.x = (uint32_t)std::ceil((float)2 / (float)uiCullDescriptorLayout->x),
				.y = 1,
				.z = 1,
				.pipelinename = UICULLING,
				.descCount = 1,
				.descriptorsetid = uiCullDescriptors.data()
		};

		globalUICullPipelineIndex = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiCullPipelineCreate);
	}
	{
		ShaderResourceSetBuilder uiDrawingBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UIDRAWING, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uiDrawDescriptorContext{ &mainAppLogger, false };

		uiDrawingBufferDescriptorB.BindBufferToShaderResource(&uiDrawDescriptorContext, &globalUIContainerData, 0, 1, 0);
		uiDrawingBufferDescriptorB.BindBufferView(&uiDrawDescriptorContext, &globalUIIndirectionHandleBuffer, 0, 1, 1);
		uiDrawingBufferDescriptorB.UploadConstant(&uiDrawDescriptorContext, &windowSize, 0);

		if (uiDrawDescriptorContext.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> uiDrawDescriptors = { uiDrawingBufferDescriptorB() };

		GraphicsIntermediaryPipelineInfo uiDrawPipelineCreate = {
			.drawType = 0,
			.vertexBufferHandle = -1,
			.vertexCount = 0,
			.pipelinename = UIDRAWING,
			.descCount = 1,
			.descriptorsetid = uiDrawDescriptors.data(),
			.indexBufferHandle = -1,
			.indexSize = 2,
			.indirectAllocation = globalUIElementsIndirectBuffer,
			.indirectDrawCount = maxUIContainers + maxUIElements,
			.indirectCountAllocation = globalUIElementsIndirectCountBuffer
		};

		globalUIDrawingPipelineIndex = GlobalRenderer::gRenderInstance.CreateGraphicsPipelineObject(mainLogicalDevice, &uiDrawPipelineCreate);
	}

	{
		ShaderResourceSetBuilder uiDepCountBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UIDEPTHCOUNT, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uiDepCountDescriptorBuilder{ &mainAppLogger, false };

		uiDepCountBufferDescriptorB.BindBufferToShaderResource(&uiDepCountDescriptorBuilder, &globalUIContainerData, 0, 1, 0);
		uiDepCountBufferDescriptorB.BindBufferToShaderResource(&uiDepCountDescriptorBuilder, &globalDepthCounts, 0, 1, 1);
		uiDepCountBufferDescriptorB.UploadConstant(&uiDepCountDescriptorBuilder, &globalUICount, 0);

		if (uiDepCountDescriptorBuilder.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		ShaderComputeLayout* uiDepCountDescriptorLayout = GlobalRenderer::gRenderInstance.GetComputeLayout(UIDEPTHCOUNT);

		std::array<ShaderResourceSetHandle, 1> uiDepthDescriptors = { uiDepCountBufferDescriptorB() };

		ComputeIntermediaryPipelineInfo uiDepthCountPipelineCreate = {
			.x = (uint32_t)std::ceil((float)2 / (float)uiDepCountDescriptorLayout->x),
			.y = 1,
			.z = 1,
			.pipelinename = UIDEPTHCOUNT,
			.descCount = 1,
			.descriptorsetid = uiDepthDescriptors.data()
		};

		globalUICountPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiDepthCountPipelineCreate);

	}
	{
		ShaderResourceSetBuilder uiDepPrefixBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, PREFIXSUM, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uiDepPrefixDescriptorBuilder{ &mainAppLogger, false };

		uiDepPrefixBufferDescriptorB.BindBufferToShaderResource(&uiDepPrefixDescriptorBuilder, &globalDepthCounts, 0, 1, 0);
		uiDepPrefixBufferDescriptorB.BindBufferToShaderResource(&uiDepPrefixDescriptorBuilder, &globalDepthOffsets, 0, 1, 1);
		uiDepPrefixBufferDescriptorB.UploadConstant(&uiDepPrefixDescriptorBuilder, &depths[DEPTH_MAX], 0);

		if (uiDepPrefixDescriptorBuilder.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> uiPrefixDepthDescriptors = { uiDepPrefixBufferDescriptorB() };

		ComputeIntermediaryPipelineInfo uiDepthPrefixPipelineCreate = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = PREFIXSUM,
			.descCount = 1,
			.descriptorsetid = uiPrefixDepthDescriptors.data()
		};

		globalUIPrefixSumPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiDepthPrefixPipelineCreate);
	}

	{
		ShaderResourceSetBuilder uiChilPrefixBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UICHILDDEPTHADD, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uiChilPrefixDescriptorBuilder{ &mainAppLogger, false };

		uiChilPrefixBufferDescriptorB.BindBufferToShaderResource(&uiChilPrefixDescriptorBuilder, &globalUIContainerData, 0, 1, 0);
		uiChilPrefixBufferDescriptorB.BindBufferToShaderResource(&uiChilPrefixDescriptorBuilder, &globalDepthOffsets, 0, 1, 1);
		uiChilPrefixBufferDescriptorB.BindBufferToShaderResource(&uiChilPrefixDescriptorBuilder, &globalChildrenOffsets, 0, 1, 2);
		uiChilPrefixBufferDescriptorB.UploadConstant(&uiChilPrefixDescriptorBuilder, &globalUICount, 0);

		if (uiChilPrefixDescriptorBuilder.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> uiChildDepthDescriptors = { uiChilPrefixBufferDescriptorB() };

		ComputeIntermediaryPipelineInfo uiChildPrefixPipelineCreate = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = UICHILDDEPTHADD,
			.descCount = 1,
			.descriptorsetid = uiChildDepthDescriptors.data()
		};


		globalUIChildDepthAddPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiChildPrefixPipelineCreate);
	}

	{
		ShaderResourceSetBuilder uiUIIndexAssignBufferDescriptorB = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UIINDEXASSIGNMENT, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext uiUIIndexAssignDescriptorBuilder{ &mainAppLogger, false };

		uiUIIndexAssignBufferDescriptorB.BindBufferToShaderResource(&uiUIIndexAssignDescriptorBuilder, &globalUIContainerData, 0, 1, 0);
		uiUIIndexAssignBufferDescriptorB.BindBufferToShaderResource(&uiUIIndexAssignDescriptorBuilder, &globalChildrenOffsets, 0, 1, 1);
		uiUIIndexAssignBufferDescriptorB.BindBufferView(&uiUIIndexAssignDescriptorBuilder, &globalUIIndirectionPositionalHandleBuffer, 0, 1, 2);
		uiUIIndexAssignBufferDescriptorB.BindBufferToShaderResource(&uiUIIndexAssignDescriptorBuilder, &globalDepthOffsets, 0, 1, 3);
		uiUIIndexAssignBufferDescriptorB.UploadConstant(&uiUIIndexAssignDescriptorBuilder, &globalUICount, 0);

		if (uiUIIndexAssignDescriptorBuilder.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> uiIndexDescriptors = { uiUIIndexAssignBufferDescriptorB() };

		ComputeIntermediaryPipelineInfo uiChildPrefixPipelineCreate = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = UIINDEXASSIGNMENT,
			.descCount = 1,
			.descriptorsetid = uiIndexDescriptors.data()
		};


		globalUIIndexAssignmentPipeline = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiChildPrefixPipelineCreate);
	}

	for (int i = 0; i < 2; i++)
	{
		ShaderResourceSetBuilder descriptorBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UILAYOUTSIZES, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext descriptorContext{ &mainAppLogger, false };

		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalUIContainerData, 0, 1, 0);
		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalDepthCounts, 0, 1, 2);
		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalDepthOffsets, 0, 1, 1);
		descriptorBuilder.BindBufferView(&descriptorContext, &globalUIIndirectionPositionalHandleBuffer, 0, 1, 3);
		descriptorBuilder.UploadConstant(&descriptorContext, &windowSize, 0);
		descriptorBuilder.UploadConstant(&descriptorContext, &depths[i], 1);

		if (descriptorContext.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> descriptors = { descriptorBuilder() };

		ComputeIntermediaryPipelineInfo uiChildPrefixPipelineCreate = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = UILAYOUTSIZES,
			.descCount = 1,
			.descriptorsetid = descriptors.data()
		};

		globalContainerSizeCalculationPipeline[i] = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiChildPrefixPipelineCreate);
	}

	for (int i = 0; i < 1; i++)
	{
		ShaderResourceSetBuilder descriptorBuilder = GlobalRenderer::gRenderInstance.AllocateShaderResourceSet(mainDescriptorManagerIndex, UIABSOLUTEPOSITION, 0, GlobalRenderer::gRenderInstance.MAX_FRAMES_IN_FLIGHT);
		ShaderResourceSetContext descriptorContext{ &mainAppLogger, false };

		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalUIContainerData, 0, 1, 0);
		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalDepthCounts, 0, 1, 2);
		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalDepthOffsets, 0, 1, 1);
		descriptorBuilder.BindBufferToShaderResource(&descriptorContext, &globalChildrenOffsets, 0, 1, 3);
		descriptorBuilder.BindBufferView(&descriptorContext, &globalUIIndirectionPositionalHandleBuffer, 0, 1, 4);
		descriptorBuilder.UploadConstant(&descriptorContext, &windowSize, 0);
		descriptorBuilder.UploadConstant(&descriptorContext, &depths[i+1], 1);

		if (descriptorContext.contextFailed)
		{
			mainAppLogger.AddLogMessage(LOGERROR, STRING_VIEW_FROM_LITERAL("failed binding for UI descriptor"));
			mainAppLogger.ProcessMessage();
			return;
		}

		std::array<ShaderResourceSetHandle, 1> descriptors = { descriptorBuilder() };

		ComputeIntermediaryPipelineInfo uiChildPrefixPipelineCreate = {
			.x = 1,
			.y = 1,
			.z = 1,
			.pipelinename = UIABSOLUTEPOSITION,
			.descCount = 1,
			.descriptorsetid = descriptors.data()
		};

		globalContainerPositionCalculationPipeline[i+1] = GlobalRenderer::gRenderInstance.CreateComputePipelineObject(mainLogicalDevice, &uiChildPrefixPipelineCreate);
	}

}