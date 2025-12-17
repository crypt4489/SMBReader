#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"


#include "SMBTexture.h"
#include "AppAllocator.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

ApplicationLoop* loop;

std::array<std::string, 3> commandsStrings =
{
	"end",
	"load",
	"position"
};

struct Handles
{
	int vertexFlags;
	int numHandles;
	int stride;
	int pad;
	int handles[12];
	glm::mat4 m;
	AxisBox minMaxBox;
};

static int computeMemory;
static int computeObjIndex;

static int globalBufferLocation;
static int globalBufferDescriptor;
static int globalTexturesDescriptor;

static int globalMeshLocation;
static int globalMeshAllocator;
static int globalMeshSize = 24 * KB;

static bool imageVisible = true;

static char vertexAndIndicesMemory[16 * MB];
static char meshObjectSpecificMemory[2 * MB];
static char geometryObjectSpecificMemory[1 * MB];
static char mainTextureCacheMemory[16 * MB];


SlabAllocator vertexAndIndicesAlloc(vertexAndIndicesMemory, sizeof(vertexAndIndicesMemory));
SlabAllocator meshObjectSpecificAlloc(meshObjectSpecificMemory, sizeof(meshObjectSpecificMemory));
SlabAllocator vgeometryObjectSpecificAlloc(geometryObjectSpecificMemory, sizeof(geometryObjectSpecificMemory));


static TextureDictionary mainDictionary;


#define MAX_GEOMETRY 2048
#define MAX_MESHES 4096
#define MAX_MESH_TEXTURES 8192


struct Geometry
{
	int meshCount;
	int meshStart;
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

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

static SMBImageFormat ConvertAppImageFormatToSMBFormat(ImageFormat format)
{
	switch (format)
	{
	case X8L8U8V8:         return SMB_X8L8U8V8;
	case DXT1:             return SMB_DXT1;
	case DXT3:             return SMB_DXT3;
	case R8G8B8A8:         return SMB_R8G8B8A8;

		// Formats that have no SMB equivalent:
	case B8G8R8A8:
	case D24UNORMS8STENCIL:
	case D32FLOATS8STENCIL:
	case D32FLOAT:
	case R8G8B8A8_UNORM:
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
	case SMB_R8G8B8A8:     return R8G8B8A8;

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
	delete mainWindow; 
}

void ApplicationLoop::ExecuteCommands(const std::string& command, const std::vector<std::any>& args)
{

	if (command == "load")
	{
		LoadThreadedWrapper(std::any_cast<std::string>(args.at(0)));
	}
	else if (command == "end")
	{
		SetRunning(false);
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

		ExecuteCommands("load", { args.inputFile.string() });

		//ExecuteCommands("load", { std::string("C:\\Users\\dflet\\Documents\\Visual Studio Projects\\SMBReader\\strangernew.smb") });

		int i = 0, j = 1;

		LARGE_INTEGER startTime;
		LARGE_INTEGER currentTime;
		LARGE_INTEGER frequency;

		uint64_t frameCounter = 0;
		double FPS = 60.0f;

		auto fps = [&frameCounter, &currentTime, &startTime, &frequency, &FPS]()
			{
				double elapsed;
				QueryPerformanceCounter(&currentTime);

				elapsed = static_cast<double>((currentTime.QuadPart - startTime.QuadPart)) / frequency.QuadPart;

				if (elapsed >= 1.0) {
					FPS = static_cast<double>(frameCounter) / elapsed;
					//std::cout << FPS << "\n";
					frameCounter = 0;
					QueryPerformanceCounter(&startTime);
				}
			};

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&startTime);

		while (running)
		{
			//std::string base = std::string("FPS : ");
			//std::string newstring = base + std::to_string(FPS);
			//size_t stringLoc = base.size() - 1;
			//text1->UpdateText(newstring);
			//TextManager::UpdateVertexBuffer(text1, stringLoc);

		

			if (mainWindow->ShouldCloseWindow()) break;

			glfwPollEvents();

			MoveCamera(FPS);

			//UpdateRenderables();

			auto index = VKRenderer::gRenderInstance->BeginFrame();

			

			if (index != ~0ui32) {
				VKRenderer::gRenderInstance->SubmitFrame(index);
			}
			

			ProcessCommands();

			ThreadManager::ASyncThreadsDone();

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
		ImageFormat::R8G8B8A8_UNORM
	};

	auto rendInst = VKRenderer::gRenderInstance;

	for (int i = 0; i < 4; i++)
	{
		mainDictionary.texturePoolsFormat[i] = formats[i];
		mainDictionary.texturePoolsSize[i] = 128 * MB;
		mainDictionary.texturePoolsAllocatedSize[i] = 0;
		mainDictionary.texturePoolHandle[i] = rendInst->CreateImagePool(
			mainDictionary.texturePoolsSize[i],
			formats[i], MAX_IMAGE_DIM, MAX_IMAGE_DIM, false
		);
	}
}

int instanceAlloc;
std::array<glm::mat4, 64 * 64> instanceMatrices;

static EntryHandle storageBuffer;


void ApplicationLoop::CreateGlobalStorageImage()
{

	auto rendInst = VKRenderer::gRenderInstance;

	instanceAlloc = rendInst->GetAllocFromDeviceBuffer(sizeof(instanceMatrices), alignof(glm::mat4), STATIC);

	storageBuffer = rendInst->CreateStorageImage(512, 512, 1, R8G8B8A8_UNORM, GetPoolIndexByFormat(R8G8B8A8_UNORM));

	int computeDesc = rendInst->AllocateShaderResourceSet(3, 0, rendInst->MAX_FRAMES_IN_FLIGHT);

	computeMemory = rendInst->GetAllocFromUniformBuffer(64, 64, STATIC);

	rendInst->descriptorManager.BindBufferToShaderResource(computeDesc, computeMemory, 0);
	rendInst->descriptorManager.BindSampledImageToShaderResource(computeDesc, storageBuffer, 1);
	rendInst->descriptorManager.BindImageBarrier(computeDesc, 1, 0, BEGINNING_OF_PIPE, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, false);
	rendInst->descriptorManager.BindImageBarrier(computeDesc, 1, 1, FRAGMENT_BARRIER, READ_SHADER_RESOURCE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);

	ShaderComputeLayout* layout = rendInst->GetComputeLayout(3);

	std::array computeDescriptors = { computeDesc };

	ComputeIntermediaryPipelineInfo create2 = {
			.x = 512 / layout->x,
			.y = 512 / layout->y,
			.z = 1,
			.pipelinename = POLY,
			.descCount = 1,
			.descriptorsetid = computeDescriptors.data(),
			.barrierCount = 2,
			.pushRangeCount = 0
	};

	//computeObjIndex = rendInst->CreateComputeVulkanPipelineObject(&create2);


	float offsetX = 4 * -15.0f;
	float offsetY = 4 * -15.0f;
	float offsetZ = 0.0f;

	for (int i = 0; i < 4096; i++)
	{
		instanceMatrices[i] = glm::identity<glm::mat4>();
		instanceMatrices[i][3].y = offsetY;
		instanceMatrices[i][3].x = offsetX;
		instanceMatrices[i][3].z = offsetZ;
		offsetX += 15.0f;
		if ((i & 7) == 7)
		{
			offsetX = 4 * -15.0f;
			offsetY += 15.0f;
		}

		if ((i & 63) == 63)
		{
			offsetZ -= 15.0f;
			offsetX = 4 * -15.0f;
			offsetY = 4 * -15.0f;
		}

	}

	rendInst->UpdateAllocation(instanceMatrices.data(), instanceAlloc, sizeof(instanceMatrices), ABSOLUTE_ALLOCATION_OFFSET, 0, 1);
}

void ApplicationLoop::MoveCamera(double fps)
{
	bool moved = false;

	float stepfactor = 10.0 / fps;

	double angleFactor = 10.0 / fps;

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
}
static bool what = imageVisible;
void ApplicationLoop::UpdateRenderables()
{
}

void ApplicationLoop::UpdateCameraMatrix()
{
	c.UpdateCamera();

	WriteCameraMatrix(RenderInstance::MAX_FRAMES_IN_FLIGHT);
}

void ApplicationLoop::WriteCameraMatrix(uint32_t frame)
{
	
	VKRenderer::gRenderInstance->UpdateAllocation(&c.View, globalBufferLocation, sizeof(glm::mat4) * 2, 0, 0, frame);
	
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
	case ImageFormat::R8G8B8A8_UNORM:
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
	
	//int objMemory = rendInst->GetAllocFromUniformBuffer(sizeof(glm::mat4) + sizeof(geoDef->axialBox), alignof(glm::mat4), PERFRAME);
	int count = geoDef->numRenderables;

	float xLoc = UpdateAtomic(geoX, 5.0f, 0.0f);

	glm::mat4 hierarchialMatrix = glm::translate(glm::scale(glm::identity<glm::mat4>(), glm::vec3(10.0f, 10.0f, 10.0f)), glm::vec3(xLoc, 0.f, 0.f));;

	

	glm::mat4 rotation = CreateRotationMatrixMat4(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(90.0f));

	hierarchialMatrix *= rotation;

//	rendInst->UpdateAllocation(&hierarchialMatrix, objMemory, 64, 0, 0, rendInst->MAX_FRAMES_IN_FLIGHT);

	//rendInst->UpdateAllocation(&geoDef->axialBox, objMemory, sizeof(geoDef->axialBox), 64, 0, rendInst->MAX_FRAMES_IN_FLIGHT);

	Geometry* geom = nullptr;

	std::tie(std::ignore, geom) = geometryInstanceData.Allocate();

	geom->meshStart = meshInstanceData.allocatorPtr;

	for (int i = 0; i<count; i++)
	{

		if (geoDef->renderablesTypes[i] == VBRENDERABLE) continue;

		SMBVertexTypes type = geoDef->vertexTypes[i];

		Mesh* mesh = nullptr;
		
		std::tie(std::ignore, mesh) = meshInstanceData.Allocate();

		geom->meshCount++;

		int indexCount = geoDef->indicesCount[i];

		int vertexCount = geoDef->verticesCount[i];

		mesh->indicesCount = indexCount;

		mesh->verticesCount = vertexCount;

		uint16_t* indices = (uint16_t*)vertexAndIndicesAlloc.Allocate(sizeof(uint16_t) * indexCount);

		mesh->indexId = meshIndexData.Allocate(indices);
		
		int vertexSize = 0;

		void* vertexData;

		bool decompressed = true;

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

		mesh->indexSize = 2;

		vertexData = (void*)vertexAndIndicesAlloc.Allocate(vertexSize * vertexCount);

		mesh->vertexId = meshVertexData.Allocate(vertexData);
		
		SMBCopyVertexData(geoDef, i, file, vertexData, decompressed);

		SMBCopyIndices(geoDef, i, file, indices);


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
			vertexFlags =  POSITION | TEXTURES1 | TEXTURES2 | BONES2;
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

		


		int graphicDesc = rendInst->AllocateShaderResourceSet(0, 2, frames);

		mesh->meshDescriptor = graphicDesc;
		
		int vertexMemory = rendInst->GetAllocFromDeviceStorageBuffer(vertexSize * vertexCount, 16, STATIC);
		int indexMemory = rendInst->GetAllocFromDeviceBuffer(sizeof(uint16_t) * indexCount, 64, STATIC);

		mesh->deviceIndices = indexMemory;
		mesh->deviceVertices = vertexMemory;
		 
		//mesh->meshInstanceMemoryCount = 1;

		Handles handles{ 0 };

		//int textureHandles = rendInst->GetAllocFromUniformBuffer(sizeof(Handles), alignof(glm::mat4), PERFRAME);

		//mesh->meshInstanceMemoryStart = meshDeviceMemoryData.Allocate(textureHandles);

		handles.numHandles = geoDef->materialsCount[i];

		int base = geoDef->materialStart[i];

		mesh->texturesStart = meshTextureHandles.AllocateN(handles.numHandles);
		mesh->texuresCount = handles.numHandles;

		handles.vertexFlags = vertexFlags;
		handles.stride = vertexSize;

		for (int ii = 0; ii < handles.numHandles; ii++)
		{
			handles.handles[ii] = geoDef->materialsId[base+ii];
			if (handles.handles[ii] == -1) handles.handles[ii] = 0;
			meshTextureHandles.Update(mesh->texturesStart+ii, handles.handles[ii]);
		}

		memcpy(&handles.minMaxBox, &geoDef->axialBox, sizeof(AxisBox));
		memcpy(&handles.m, &hierarchialMatrix, sizeof(glm::mat4));
		
		rendInst->UpdateAllocation(&handles, globalMeshLocation, sizeof(Handles), globalMeshAllocator, 0, 3);

		uint32_t index = (globalMeshAllocator / sizeof(Handles));

		globalMeshAllocator += sizeof(Handles);

		int smallSliceOfHeaven = rendInst->GetAllocFromUniformBuffer(sizeof(uint32_t), alignof(glm::mat4), PERFRAME);

		rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, smallSliceOfHeaven, 0);

		rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, globalMeshLocation, 1);

		rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, vertexMemory, 2);

		rendInst->UpdateAllocation(&index, smallSliceOfHeaven, 4, 0, 0, rendInst->MAX_FRAMES_IN_FLIGHT);

		rendInst->UpdateAllocation(vertexData, vertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET, 0, 1);

		rendInst->UpdateAllocation(indices, indexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET, 0, 1);

		


		GraphicsIntermediaryPipelineInfo create = {
			.drawType = 0,
			.vertexBufferIndex = vertexMemory,
			.vertexCount = (uint32_t)vertexCount,
			.pipelinename = GENERIC,
			.descCount = 1,
			.descriptorsetid = &graphicDesc,
			.indexBufferHandle = indexMemory,
			.indexCount = (uint32_t)indexCount,
			.pushRangeCount = 0,
			.instanceCount = 1,
			.indexSize = 2
		};

		mesh->drawableIndex = (int)rendInst->CreateGraphicsVulkanPipelineObject(&create);
	}
}



void ApplicationLoop::LoadSMBFile(SMBFile &file)
{

	int totalTextureCount = 0, totalMeshCount = 0;

	std::vector<SMBTexture> textures;

	auto& chunk = file.chunks;

	std::array<SMBGeoChunk*, 10> geoDefs{};

	for (size_t i = 0; i<chunk.size(); i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{
			
			SMBGeoChunk** geoDef = &geoDefs[totalMeshCount++];

			FileHandle* handle = FileManager::GetFile(file.id);

			auto& geoChunk = handle->streamHandle;

			size_t seekpos = chunk[i].offsetInHeader;

			geoChunk.seekg(seekpos);

			std::vector<char> geomHeader(chunk[i].headerSize);

			geoChunk.read(geomHeader.data(), chunk[i].headerSize);

			*geoDef = ProcessGeometryClass(geomHeader.data(), totalTextureCount);

			(*geoDef)->vertexAndIndicesInfo = chunk[i].contigOffset + file.fileOffset;
		

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

	VKRenderer::gRenderInstance->UpdateSamplerBinding(globalTexturesDescriptor, 0, mainDictionary.textureHandles.data() + index, index, totalTextureCount);
	
	for (int i = 0; i < totalMeshCount; i++)
	{
		SMBGeoChunk* geoDef = geoDefs[i];

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

		delete geoDef;
	}
	

	
}


void ApplicationLoop::InitializeRuntime()
{
	ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN),
				this, std::placeholders::_1));

	mainDictionary.textureCache = (uintptr_t)mainTextureCacheMemory;
	
	mainDictionary.textureSize = sizeof(mainTextureCacheMemory);


	VKRenderer::gRenderInstance = new RenderInstance();

	mainWindow = new WindowManager();

	mainWindow->CreateWindowInstance();

	glfwSetKeyCallback(mainWindow->GetWindow(), key_callback);

	VKRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow);

	CreateTexturePools();

	globalBufferLocation = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(sizeof(glm::mat4) * 2, alignof(glm::mat4), PERFRAME);
	globalBufferDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	globalMeshLocation = VKRenderer::gRenderInstance->GetAllocFromUniformBuffer(globalMeshSize, alignof(glm::mat4), PERFRAME);;
	globalMeshAllocator = 0;

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalBufferDescriptor, globalBufferLocation, 0);

	std::array arr = { globalBufferDescriptor, globalTexturesDescriptor };

	VKRenderer::gRenderInstance->CreateRenderTargetData(arr.data(), 2);

	
	c.CamLookAt(glm::vec3(0.0f, 0.0f, 55.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	c.UpdateCamera();

	c.CreateProjectionMatrix(VKRenderer::gRenderInstance->GetSwapChainWidth() / (float)VKRenderer::gRenderInstance->GetSwapChainHeight(), 0.1f, 10000.0f, glm::radians(45.0f));
	
	WriteCameraMatrix(VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
}


static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

	switch (key)
	{
	case GLFW_KEY_D:
		loop->camMovements[ApplicationLoop::RIGHT] = pressed;
		break;

	case GLFW_KEY_A:
		loop->camMovements[ApplicationLoop::LEFT] = pressed;
		break;

	case GLFW_KEY_W:
		loop->camMovements[ApplicationLoop::FORWARD] = pressed;
		break;

	case GLFW_KEY_S:
		loop->camMovements[ApplicationLoop::BACK] = pressed;
		break;

	case GLFW_KEY_UP:
		loop->camMovements[ApplicationLoop::PITCHUP] = pressed;
		break;

	case GLFW_KEY_DOWN:
		loop->camMovements[ApplicationLoop::PITCHDOWN] = pressed;
		break;

	case GLFW_KEY_RIGHT:
		loop->camMovements[ApplicationLoop::ROTATEYRIGHT] = pressed;
		break;

	case GLFW_KEY_LEFT:
		loop->camMovements[ApplicationLoop::ROTATEYLEFT] = pressed;
		break;

	case GLFW_KEY_2:
		if (pressed && action == GLFW_PRESS)
			VKRenderer::gRenderInstance->IncreaseMSAA();
		break;

	case GLFW_KEY_1:
		if (pressed && action == GLFW_PRESS)
			VKRenderer::gRenderInstance->DecreaseMSAA();
		break;

	case GLFW_KEY_9:
		if (pressed && action == GLFW_PRESS)
			imageVisible = !imageVisible;
		break;
	}
}

void ApplicationLoop::CleanupRuntime()
{
	VKRenderer::gRenderInstance->WaitOnRender();

	for (int i = 0; i < mainDictionary.allocationIndex; i++)
	{
		VKRenderer::gRenderInstance->DestoryTexture(mainDictionary.textureHandles[i]);
	}

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
	std::vector<std::any> com = std::move(commands.front());
	commands.pop();
	queueSema.Notify();
	if (!com.size()) { std::cerr << "what are you doing\n"; return; }
	auto mapFunc = std::find(commandsStrings.begin(), commandsStrings.end(), std::any_cast<std::string>(com[0]));
	if (mapFunc == std::end(commandsStrings)) return;
	if (com.size() > 1)
		ExecuteCommands(*mapFunc, { com[1] });
	else 
		ExecuteCommands(*mapFunc, { });
}



void ApplicationLoop::AddCommandTS(std::vector<std::any>& com)
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

	for (uint32_t i = 0; i < VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT; i++)
		VKRenderer::gRenderInstance->InvalidateRecordBuffer(i);

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

void ApplicationLoop::FindWords(std::string words, std::vector<std::any>& out)
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

		std::vector<std::any> comandargs{};

		FindWords(output, comandargs);

		this->AddCommandTS(comandargs);

		if (output == "end") break;

		std::osyncstream(std::cout) << "Hit enter and then write command > ";
	}
}