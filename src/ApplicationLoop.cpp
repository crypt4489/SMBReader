#include "ApplicationLoop.h"
#include "RenderInstance.h"
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif
#include "Camera.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

ApplicationLoop* loop;

std::array<std::string, 2> commandsStrings =
{
	"end",
	"load"
};

static int computeMemory;

static float x = 0.0f;

static uint32_t computeObjIndex = 0;

static bool imageVisible = true;

static bool justUpdatedObj = false;



static void* transientVertexData;
static int transientVertexDataPtr = 0;
static int transientVertexDataSize = 16 * MB;


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
	int meshInstanceMemoryCount;
	int meshInstanceMemoryStart;
	int meshDescriptor;
	int deviceIndices;
	int deviceVertices;
};

static int meshTextureHandlesAlloc = 0;
static int meshVertexDataAlloc = 0;
static int meshIndexDataAlloc = 0;
static int meshInstanceDataAlloc = 0;
static int geometryInstanceDataAlloc = 0;
//static int meshObjectMemoryDataAlloc = 0;
static int meshDeviceMemoryDataAlloc = 0;

static std::array<int, MAX_MESH_TEXTURES> meshTextureHandles;
static std::array<void*, MAX_MESHES> meshVertexData;
static std::array<void*, MAX_MESHES> meshIndexData;
//static std::array<int, MAX_MESHES * 2> meshObjectMemoryData;
static std::array<int, MAX_MESHES * 2> meshDeviceMemoryData;
static std::array<Mesh, MAX_MESHES> meshInstanceData;
static std::array<Geometry, MAX_GEOMETRY> geometryInstanceData;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

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


void ApplicationLoop::CreateGlobalStorageImage()
{

	auto rendInst = VKRenderer::gRenderInstance;

	instanceAlloc = rendInst->GetPageFromDeviceBuffer(sizeof(instanceMatrices), alignof(glm::mat4));

	storageBuffer = rendInst->CreateStorageImage(512, 512, 1, R8G8B8A8_UNORM, GetPoolIndexByFormat(R8G8B8A8_UNORM));

	int computeDesc = rendInst->AllocateShaderResourceSet(3, 0, rendInst->MAX_FRAMES_IN_FLIGHT);

	computeMemory = rendInst->GetPageFromUniformBuffer(64, 64);

	rendInst->descriptorManager.BindBufferToShaderResource(computeDesc, computeMemory, DIRECT, 0);
	rendInst->descriptorManager.BindSampledImageToShaderResource(computeDesc, loop->storageBuffer, 1);
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

	rendInst->UpdateAllocation(instanceMatrices.data(), instanceAlloc, sizeof(instanceMatrices), ABSOLUTE_ALLOCATION_OFFSET);
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

	x += 0.0001f;
	auto rendInst = VKRenderer::gRenderInstance;

	if (imageVisible)

	{
	//	rendInst->UpdateAllocation(&x, computeMemory, 4, ABSOLUTE_ALLOCATION_OFFSET);
	}

	if (what != imageVisible)
	{
		//rendInst->SetActiveComputePipeline(computeObjIndex, imageVisible);
		//what = imageVisible;
	}	
}

void ApplicationLoop::UpdateCameraMatrix()
{
	c.UpdateCamera();

	WriteCameraMatrix(RenderInstance::MAX_FRAMES_IN_FLIGHT);
}

void ApplicationLoop::WriteCameraMatrix(uint32_t frame)
{
	for (uint32_t i = 0; i<frame; i++) {
		VKRenderer::gRenderInstance->UpdateAllocation(&c.View, globalBufferLocation, sizeof(glm::mat4) * 2, sizeof(glm::mat4) * 2 * i);
	}
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

char* AllocTransientVertexData(int size)
{
	char* head = (char*)transientVertexData;

	int out = transientVertexDataPtr;

	transientVertexDataPtr += size;

	return (head + out);

}

void ApplicationLoop::SMBGeometricalObject(SMBGeoChunk* geoDef, SMBFile& file)
{
	auto rendInst = VKRenderer::gRenderInstance;
	uint32_t frames = rendInst->MAX_FRAMES_IN_FLIGHT;
	int objMemory = rendInst->GetPageFromUniformBuffer(sizeof(glm::mat4) * frames, alignof(glm::mat4));
	int count = geoDef->numRenderables;

	glm::mat4 hierarchialMatrix = glm::scale(glm::identity<glm::mat4>(), glm::vec3(10.0f, 10.0f, 10.0f));

	glm::mat4 rotation = LTM::CreateRotationMatrixMat4(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(90.0f));

	hierarchialMatrix *= rotation;

	rendInst->UpdateAllocation(&hierarchialMatrix, objMemory, 64, 0);
	rendInst->UpdateAllocation(&hierarchialMatrix, objMemory, 64, 64);
	rendInst->UpdateAllocation(&hierarchialMatrix, objMemory, 64, 128);

	Geometry* geom = &geometryInstanceData[geometryInstanceDataAlloc++];

	geom->meshStart = meshInstanceDataAlloc;

	for (int i = 0; i<count; i++)
	{

		if (geoDef->renderablesTypes[i] == VBRENDERABLE) continue;

		Mesh* mesh = &meshInstanceData[meshInstanceDataAlloc++];

		geom->meshCount++;

		int indexCount = geoDef->indicesCount[i];

		int vertexCount = geoDef->verticesCount[i];

		mesh->indicesCount = indexCount;

		mesh->verticesCount = vertexCount;

		mesh->indexId = meshIndexDataAlloc;

		mesh->vertexId = meshVertexDataAlloc;

		SMBVertexTypes type = geoDef->vertexTypes[i];

		uint16_t* indices = (uint16_t*)AllocTransientVertexData(sizeof(uint16_t) * indexCount);

		meshIndexData[meshIndexDataAlloc++] = indices;
		
		size_t vertexSize = 0;

		void* vertexData;

		switch (type)
		{
		case PosPack6_CNorm_C16Tex1_Bone2:
			vertexSize = sizeof(Vertex_PosPack6_CNorm_C16Tex1_Bone2);
			break;
		case PosPack6_C16Tex2_Bone2:
			vertexSize = sizeof(Vertex_PosPack6_C16Tex2_Bone2);
			break;
		case PosPack6_C16Tex1_Bone2:
			vertexSize = sizeof(Vertex_PosPack6_C16Tex1_Bone2);
			break;
		}

		mesh->vertexSize = (int)vertexSize;

		mesh->indexSize = 2;

		vertexData = (void*)AllocTransientVertexData(vertexSize * vertexCount);

		meshVertexData[meshVertexDataAlloc++] = vertexData;
		
		SMBCopyVertexData(geoDef, i, file, vertexData);

		SMBCopyIndices(geoDef, i, file, indices);

		std::vector<BasicVertex> transfer(vertexCount);

		switch (type)
		{
		case PosPack6_CNorm_C16Tex1_Bone2:
		{
			Vertex_PosPack6_CNorm_C16Tex1_Bone2* verts = (Vertex_PosPack6_CNorm_C16Tex1_Bone2*)vertexData;
			for (int ii = 0; ii < vertexCount; ii++)
			{
				transfer[ii].TEXTURE = glm::vec4(verts[ii].TEXTURE, 0.0f, 0.0f);
				transfer[ii].POSITION = verts[ii].POSITION;
			}
			break;
		}
		case PosPack6_C16Tex2_Bone2:
		{
			Vertex_PosPack6_C16Tex2_Bone2* verts = (Vertex_PosPack6_C16Tex2_Bone2*)vertexData;
			for (int ii = 0; ii < vertexCount; ii++)
			{
				transfer[ii].TEXTURE = glm::vec4(verts[ii].TEXTURE, 0.0f, 0.0f);
				transfer[ii].POSITION = verts[ii].POSITION;
			}
			break;
		}
		case PosPack6_C16Tex1_Bone2:
		{
			Vertex_PosPack6_C16Tex1_Bone2* verts = (Vertex_PosPack6_C16Tex1_Bone2*)vertexData;
			for (int ii = 0; ii < vertexCount; ii++)
			{
				transfer[ii].TEXTURE = glm::vec4(verts[ii].TEXTURE, 0.0f, 0.0f);
				transfer[ii].POSITION = verts[ii].POSITION;
			}
			break;
		}
		}

		


		int graphicDesc = rendInst->AllocateShaderResourceSet(0, 2, frames);

		mesh->meshDescriptor = graphicDesc;
		
		int vertexMemory = rendInst->GetPageFromDeviceBuffer(sizeof(BasicVertex) * vertexCount, alignof(glm::vec4));
		int indexMemory = rendInst->GetPageFromDeviceBuffer(sizeof(uint32_t) * indexCount, alignof(uint32_t));

		mesh->deviceIndices = indexMemory;
		mesh->deviceVertices = vertexMemory;
		mesh->meshInstanceMemoryStart = meshDeviceMemoryDataAlloc;
		mesh->meshInstanceMemoryCount = 1;

		int textureHandles = rendInst->GetPageFromUniformBuffer(64 * frames, alignof(glm::mat4));

		meshDeviceMemoryData[meshDeviceMemoryDataAlloc++] = textureHandles;
		
		struct Handles
		{
			int numHandles;
			int handles[15];
		};

		

		Handles handles;

		memset(&handles, 0, sizeof(Handles));

		handles.numHandles = geoDef->materialsCount[i];

		int base = geoDef->materialStart[i];

		mesh->texuresCount = meshTextureHandlesAlloc;
		mesh->texuresCount = handles.numHandles;

		for (int ii = 0; ii < handles.numHandles; ii++)
		{
			handles.handles[ii] = geoDef->materialsId[base+ii];
			meshTextureHandles[meshTextureHandlesAlloc++] = handles.handles[ii];
		}
		

		rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, objMemory, REPEAT, 0);

		rendInst->descriptorManager.BindBufferToShaderResource(graphicDesc, textureHandles, REPEAT, 1);


		rendInst->UpdateAllocation(transfer.data(), vertexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

		rendInst->UpdateAllocation(indices, indexMemory, FULL_ALLOCATION_SIZE, ABSOLUTE_ALLOCATION_OFFSET);

		rendInst->UpdateAllocation(&handles, textureHandles, sizeof(Handles), ABSOLUTE_ALLOCATION_OFFSET);
		rendInst->UpdateAllocation(&handles, textureHandles, sizeof(Handles), sizeof(Handles) * 1);
		rendInst->UpdateAllocation(&handles, textureHandles, sizeof(Handles), sizeof(Handles) * 2);


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

	int previousLevel = mainDictionary.allocationIndex;

	int totalTextureCount = 0;

	std::vector<uint32_t> textureIds;

	int alloc = 0;

	auto& chunk = file.chunks;

	SMBGeoChunk* geoDef = nullptr;

	for (size_t i = 0; i<chunk.size(); i++)
	{
		switch (chunk[i].chunkType)
		{
		case GEO:
		{

			FileHandle* handle = FileManager::GetFile(file.id);

			auto& geoChunk = handle->streamHandle;

			size_t seekpos = chunk[i].offsetInHeader;

			geoChunk.seekg(seekpos);

			std::vector<char> geomHeader(chunk[i].headerSize);

			geoChunk.read(geomHeader.data(), chunk[i].headerSize);

			geoDef = ProcessGeometryClass(geomHeader.data(), totalTextureCount);

			geoDef->vertexAndIndicesInfo = chunk[i].contigOffset + file.fileOffset;

			seekpos = chunk[i + 1].contigOffset + file.fileOffset;

			geoDef->verticesandIndexCompressedSize = seekpos - geoDef->vertexAndIndicesInfo;

			break;
		}
		case TEXTURE:
		{
			SMBTexture texture(file, chunk[i]);
			alloc = mainDictionary.AllocateTextureData(
				(char*)texture.data,
				texture.cumulativeSize,
				texture.type,
				texture.width,
				texture.height,
				texture.miplevels);
			mainDictionary.textureHandles[alloc] =
				VKRenderer::gRenderInstance->CreateImage(
					(char*)texture.data,
					texture.imageSizes,
					texture.cumulativeSize,
					texture.width,
					texture.height,
					texture.miplevels,
					texture.type,
					GetPoolIndexByFormat(texture.type));
			textureIds.push_back(chunk[i].chunkId);
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
	
	if (geoDef)
	{
		
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
					if (id == textureIds[gg])
					{
						geoDef->materialsId[hh + base] = previousLevel + gg;
						break;
					}
				}

				if (gg == totalTextureCount)
				{
					throw std::runtime_error("Invalid texture ID");
				}

			}

			geoDef->materialStart[jj] = base;

			base += count;
		} 

		SMBGeometricalObject(geoDef, file);

		delete geoDef;
	}

	

	VKRenderer::gRenderInstance->UpdateSamplerBinding(globalTexturesDescriptor, 0, mainDictionary.textureHandles.data(), previousLevel, mainDictionary.allocationIndex);
}


void ApplicationLoop::InitializeRuntime()
{
	ThreadManager::LaunchBackgroundThread(
			std::bind(std::mem_fn(&ApplicationLoop::ScanSTDIN),
				this, std::placeholders::_1));

	transientVertexData = malloc(transientVertexDataSize);

	mainDictionary.textureCache = (uintptr_t)malloc(16 * MB);
	
	mainDictionary.textureSize = 16 * MB;


	VKRenderer::gRenderInstance = new RenderInstance();

	mainWindow = new WindowManager();

	mainWindow->CreateWindowInstance();

	glfwSetKeyCallback(mainWindow->GetWindow(), key_callback);

	VKRenderer::gRenderInstance->CreateVulkanRenderer(mainWindow);

	CreateTexturePools();

	//CreateGlobalStorageImage();

	globalBufferLocation = VKRenderer::gRenderInstance->GetPageFromUniformBuffer(sizeof(glm::mat4) * 2 * VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT,64);
	globalBufferDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 0, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);
	globalTexturesDescriptor = VKRenderer::gRenderInstance->AllocateShaderResourceSet(0, 1, VKRenderer::gRenderInstance->MAX_FRAMES_IN_FLIGHT);

	VKRenderer::gRenderInstance->descriptorManager.BindBufferToShaderResource(globalBufferDescriptor, globalBufferLocation, REPEAT, 0);

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

	free(transientVertexData);

	free((void*)mainDictionary.textureCache);

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
	ExecuteCommands(*mapFunc, { com.begin() + 1, com.end() });
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

void ApplicationLoop::LoadObjectThreaded(std::shared_ptr<std::atomic<bool>> flag, const std::string& file)
{

	//std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	this->LoadObject(file);

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
			j++;
			i = j;
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