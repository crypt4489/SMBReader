#pragma once
#include <array>
#include <functional>
#include <vulkan/vulkan.h>

#include "AppTypes.h"
#include "AppAllocator.h"
#include "IndexTypes.h"
#include "VKTypes.h"
#include "ResourceDependencies.h"
#include "ShaderResourceSet.h"
#include "ThreadManager.h"
#include "WindowManager.h"


namespace API {

	VkCompareOp ConvertDepthTestAppToVulkan(DepthTest testApp);

	VkFormat ConvertSMBToVkFormat(ImageFormat format);

	VkPrimitiveTopology ConvertTopology(PrimitiveType type);
}

struct GraphicsIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferHandle;
	uint32_t vertexCount;
	uint32_t pipelinename;
	uint32_t descCount;
	int *descriptorsetid;
	int indexBufferHandle;
	uint32_t indexCount;
	uint32_t instanceCount;
	uint32_t indexSize;
	uint32_t indexOffset;
	uint32_t vertexOffset;
	int indirectAllocation;
	int indirectDrawCount;
	int indirectCountAllocation;
};

struct ComputeIntermediaryPipelineInfo
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
};



#define FULL_ALLOCATION_SIZE 0
#define ABSOLUTE_ALLOCATION_OFFSET 0

enum AllocationType
{
	STATIC = 0,
	PERFRAME = 1,
	PERDRAW = 2
};

struct RenderAllocation
{
	EntryHandle memIndex;
	size_t offset;
	size_t deviceAllocSize;
	size_t requestedSize;
	size_t alignment;
	AllocationType allocType;
};

enum class TransferType
{
	CACHED = 0,
	MEMORY = 1,
};

struct HostTransferRegion
{
	TransferType type;
	int size; 
	int copyCount;
	int allocationIndex;
	int allocoffset;
	void* data;
};

struct TransferRegionLink
{
	HostTransferRegion* region;
	TransferRegionLink* next;
};

template<int T_MaxRegionCopy>
struct HostDriverTransferPool
{
	char stagingbuffer[32*1024];
	std::array<HostTransferRegion, 1000> transferRegions;
	std::array<TransferRegionLink, 1000> regionLinks;
	TransferRegionLink* linkHead = nullptr;
	TransferRegionLink** popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0; 
	std::atomic<int> currentStagingBufferWrite = 0;

	int Create(void* data, int size, int allocationIndex, int allocOffset, TransferType type)
	{
		TransferRegionLink* link = Find(allocationIndex, allocOffset);
		HostTransferRegion* region = nullptr;

		if (!link)
		{
			int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());
			
			region = &transferRegions[regionAlloc];

			link  = &regionLinks[regionAlloc];

			

			if (type == TransferType::CACHED)
			{
				int writeLoc = UpdateAtomic(currentStagingBufferWrite, size, (int)sizeof(stagingbuffer));
				memcpy(stagingbuffer + writeLoc, data, size);
				region->data = stagingbuffer + writeLoc;
			}
			else if (type == TransferType::MEMORY)
			{
				region->data = data;
			}

			

			link->region = region;
			link->next = nullptr;

			region->allocationIndex = allocationIndex;
			region->allocoffset = allocOffset;
			region->size = size;

			Insert(link);

			
			

		}
		else 
		{
			region = link->region;

			if (type == TransferType::CACHED)
			{
				if (size > region->size)
				{
					int writeLoc = UpdateAtomic(currentStagingBufferWrite, size, (int)sizeof(stagingbuffer));
					region->data = stagingbuffer + writeLoc;
				}
				memcpy(region->data, data, size);
			}
			else 
			{
				region->data = data;
			}

			region->allocoffset = allocOffset;
			region->size = size;
		}

		
		region->type = type;
		region->copyCount = T_MaxRegionCopy;

		return 0;
	}

	void Insert(TransferRegionLink* newlink)
	{
		TransferRegionLink** test = &linkHead;
		while (*test && ((*test)->region->allocationIndex <= newlink->region->allocationIndex))
		{
			test = &((*test)->next);
		}
		newlink->next = *test;
		*test = newlink;
		linkCount.fetch_add(1);
	}

	void Delete(TransferRegionLink* deletelink)
	{
		TransferRegionLink** link = &linkHead;
		while (*link != deletelink)
		{
			link = &((*link)->next);
		}

		*link = deletelink->next;

		HostTransferRegion* region = deletelink->region;
		region->allocationIndex = -1;
		region->size = 0;
		region->copyCount = -1;
		region->allocoffset = -1;
		region->data = nullptr;
	}

	TransferRegionLink* Find(int allocationIndex, int offset)
	{
		TransferRegionLink* link = linkHead;
		while (link && (link->region->allocationIndex != allocationIndex || link->region->allocoffset != offset))
		{
			link = link->next;
		}
		return link;
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	TransferRegionLink* PopLink(HostTransferRegion* outputRegion, TransferRegionLink* link)
	{
		if (!link) return nullptr;
		outputRegion->allocationIndex = link->region->allocationIndex;
		outputRegion->size = link->region->size;
		outputRegion->copyCount = link->region->copyCount;
		outputRegion->data = link->region->data;
		outputRegion->allocoffset = link->region->allocoffset;
		TransferRegionLink* linkRet = link->next;
		if (link->region->copyCount > 1)
		{
			link->region->copyCount--;
			popPrev = &link->next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount--;
			HostTransferRegion* region = link->region;
			region->allocationIndex = -1;
			region->size = 0;
			region->copyCount = -1;
			region->allocoffset = -1;
			region->data = nullptr;
		}
		return linkRet;
	}
};

/*enum class TransferCommandType
{
	TCFILL = 0,
	TCCLEAR = 1,
};*/

struct TransferCommand
{
	//TransferCommandType type;
	uint32_t fillVal;
	int size; 
	int offset;
	int allocationIndex;
	int copycount;
	BarrierStage dstStage;
	BarrierAction dstAction;
};

struct TransferCommandLink
{
	TransferCommandLink* next;
	TransferCommand* command;
};


template<int T_MaxRegionCopy>
struct TransferCommandsPool
{
	std::array<TransferCommand, 1000> transferRegions;
	std::array<TransferCommandLink, 1000> regionLinks;
	TransferCommandLink* linkHead = nullptr;
	TransferCommandLink** popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0;

	int Create(int size, int allocationIndex, int allocOffset,  uint32_t fillValue, BarrierStage stage, BarrierAction action)
	{
		TransferCommandLink* link = Find(allocationIndex, allocOffset);
		TransferCommand* region = nullptr;

		if (!link)
		{
			int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());

			region = &transferRegions[regionAlloc];

			link = &regionLinks[regionAlloc];

			
			region->fillVal = fillValue;
			

			link->command = region;
			link->next = nullptr;

			region->allocationIndex = allocationIndex;
			region->offset = allocOffset;
			region->size = size;

			Insert(link);
		}
		else
		{
			region = link->command;

			
			region->fillVal = fillValue;
			

			region->offset = allocOffset;
			region->size = size;
		}

		//region->type = type;
		region->copycount = T_MaxRegionCopy;
		region->dstStage = stage;
		region->dstAction = action;

		return 0;
	}

	void Insert(TransferCommandLink* newlink)
	{
		TransferCommandLink** test = &linkHead;
		while (*test && ((*test)->command->allocationIndex <= newlink->command->allocationIndex))
		{
			test = &((*test)->next);
		}
		newlink->next = *test;
		*test = newlink;
		linkCount.fetch_add(1);
	}

	void Delete(TransferCommandLink* deletelink)
	{
		TransferCommandLink** link = &linkHead;
		while (*link != deletelink)
		{
			link = &((*link)->next);
		}

		*link = deletelink->next;

		TransferCommand* region = deletelink->command;
		region->allocationIndex = -1;
		region->size = 0;
		region->copycount = -1;
		region->offset = -1;
		region->fillVal = 0;
	}

	TransferCommandLink* Find(int allocationIndex, int offset)
	{
		TransferCommandLink* link = linkHead;
		while (link && (link->command->allocationIndex != allocationIndex || link->command->offset != offset))
		{
			link = link->next;
		}
		return link;
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	TransferCommandLink* PopLink(TransferCommand* outputRegion, TransferCommandLink* link)
	{
		if (!link) return nullptr;
		outputRegion->allocationIndex = link->command->allocationIndex;
		outputRegion->size = link->command->size;
		outputRegion->copycount = link->command->copycount;
		outputRegion->fillVal = link->command->fillVal;
		outputRegion->offset = link->command->offset;
		outputRegion->dstStage = link->command->dstStage;
		outputRegion->dstAction = link->command->dstAction;
		TransferCommandLink* linkRet = link->next;
		if (link->command->copycount > 1)
		{
			link->command->copycount--;
			popPrev = &link->next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount--;
			TransferCommand* region = link->command;
			region->allocationIndex = -1;
			region->size = 0;
			region->copycount = -1;
			region->offset = -1;
			region->fillVal = 0;
		}
		return linkRet;
	}
};


template <int N>
struct RenderAllocationHolder
{
	std::array<RenderAllocation, N> allocations;
	
	RenderAllocation operator[](size_t index)
	{
		if (index >= N || index < 0)
			return { EntryHandle(), ~0ui64, ~0ui64 };

		return allocations[index];
	}

	RenderAllocation operator[](int index)
	{
		if (index >= N || index < 0)
			return { EntryHandle(), ~0ui64, ~0ui64 };

		return allocations[index];
	}

	int Allocate()
	{
		return allocationsIndex.fetch_add(1);
	}

	std::atomic<int> allocationsIndex;

};

enum PipelineLabels
{
	MESH_INTERPOLATE = 0,
	TEXT = 1,
	GENERIC = 2,
	POLY = 3,
};




struct RenderInstance
{

	RenderInstance();

	~RenderInstance();

	void CreateDepthImage( uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void DestroySwapChainAttachments();

	int RecreateSwapChain();

	void CreateRenderPass(uint32_t index, VkSampleCountFlagBits sampleCount);

	void UsePipelineBuilders(VKGraphicsPipelineBuilder* generic, VKGraphicsPipelineBuilder* text, VKGraphicsPipelineBuilder* debug, VkSampleCountFlagBits flags);

	EntryHandle CreateVulkanGraphicPipelineTemplate(VKGraphicsPipelineBuilder* pipeline, ShaderGraph* graph);

	EntryHandle CreateVulkanComputePipelineTemplate(ShaderGraph* graph);

	uint32_t BeginFrame();

	int SubmitFrame(uint32_t imageIndex);

	void CreateMSAAColorResources(uint32_t width, uint32_t height, uint32_t index, uint32_t sampleCount);

	void WaitOnRender();

	void CreatePipelines();

	void CreateSwapChain(uint32_t width, uint32_t height, bool recreate);

	void UpdateAllocation(void* data, int handle, size_t size, size_t offset, size_t frame, int copies);

	int GetAllocFromUniformBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	int GetAllocFromDeviceStorageBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	int GetAllocFromDeviceBuffer(size_t size, uint32_t alignment, AllocationType allocType);

	void CopyHostRegions();

	void InvokeTransferCommands(RecordingBufferObject* rbo);

	EntryHandle CreateImage(
		char* imageData,
		uint32_t* sizes,
		uint32_t blobSize,
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	EntryHandle CreateStorageImage(
		uint32_t width, uint32_t height,
		uint32_t mipLevels, ImageFormat type, int poolIndex);

	int CreateImagePool(size_t size, ImageFormat format, int maxWidth, int maxHeight, bool attachment);

	void CreateVulkanRenderer(WindowManager* window);

	uint32_t GetSwapChainHeight();

	uint32_t GetSwapChainWidth();

	int CreateGraphicsVulkanPipelineObject(GraphicsIntermediaryPipelineInfo *info, bool addToGraph);

	int CreateComputeVulkanPipelineObject(ComputeIntermediaryPipelineInfo* info);

	void CreateRenderTargetData(int* desc, int descCount);

	void DrawScene(uint32_t imageIndex);

	static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	void DestoryTexture(EntryHandle handle);

	void IncreaseMSAA();

	void DecreaseMSAA();

	void CreateShaderResourceMap(ShaderGraph *graph);

	int AllocateShaderResourceSet(uint32_t shaderGraphIndex, uint32_t targetSet, int setCount);

	uint32_t GetDynamicOffsetsForDescriptorSet(int descriptorSet, std::vector<uint32_t>& dyanmicOffsets);

	EntryHandle CreateShaderResourceSet(int descriptorSet);

	void AddVulkanMemoryBarrier(VKPipelineObject* vkPipelineObject, int* descriptorid, int descriptorcount);

	ShaderComputeLayout* GetComputeLayout(int shaderGraphIndex);

	void SetActiveComputePipeline(uint32_t objectIndex, bool active);

	void UpdateSamplerBinding(int descriptorSet, int bindingIndex, EntryHandle* handles, uint32_t destinationArray, uint32_t texCount);

	int GetBufferAllocationViaDescriptor(int descriptorSet, int bindingIndex);

	EntryHandle CreateBufferView(int allocationIndex, VkFormat bufferViewFormat);

	void DestroyBufferView(EntryHandle bufferViewIndex);

	void EndFrame();

	void AddPipelineToMainQueue(int psoIndex, int computeorgraphics);

	void ReadData(int handle, void* dest, int size, int offset);

	VKInstance *vkInstance = nullptr;
	DeviceIndex deviceIndex;
	DeviceIndex physicalIndex;
	EntryHandle swapChainIndex;
	EntryHandle stagingBufferIndex;
	EntryHandle globalIndex, globalDeviceBufIndex;
	EntryHandle computeGraphIndex;
	std::array<EntryHandle, MAX_FRAMES_IN_FLIGHT> currentCBIndex;
	EntryHandle graphicsOTQ, computeOTQ;

	uint32_t currentMSAALevel = 0;
	uint32_t maxMSAALevels = 0;

	std::array<EntryHandle, 5> swapchainRenderTargets{};
	std::array<EntryHandle, 5> depthViews{};
	std::array<EntryHandle, 5> colorViews{};
	std::array<EntryHandle, 5> depthImages{};
	std::array<EntryHandle, 5> colorImages{};
	std::array<EntryHandle, 5> renderPasses{};

	std::array<EntryHandle, 9> imagePools{};
	int imagePoolCounter = 0;

	WindowManager *windowMan = nullptr;

	uint32_t currentFrame = 0;

	ImageFormat depthFormat = ImageFormat::IMAGE_UNKNOWN;
	ImageFormat colorFormat = ImageFormat::IMAGE_UNKNOWN;

	ShaderGraphsHolder<50, 60> vulkanShaderGraphs{};
	
	ShaderResourceManager<50> descriptorManager{};

	std::array<std::vector<EntryHandle>, 25> pipelinesIdentifier{};
	std::array<EntryHandle, 60> vulkanDescriptorLayouts{};
	std::array<ShaderDetails*, 70> shaderDetails{};
	char* shaderDetailsData;
	std::atomic<int> shaderDetailAlloc = 0;

	RenderAllocationHolder<50> allocations{};

	ArrayAllocator<EntryHandle, 50> renderStateObjects{};

	ArrayAllocator<EntryHandle, 50> computeStateObjects{};

	ArrayAllocator<int, 50> graphIndices{};

	enum
	{
		COMPUTESO,
		GRAPHICSO,
		INDIRECTSO,
	};

	struct PipelineHandle
	{
		int group;
		int indexForHandles;
		int numHandles;
		int graphIndex;
		int graphCount;
	};

	ArrayAllocator<PipelineHandle, 100> stateObjectHandles{};

	int width = 0; 
	int height = 0;

	int minUniformAlignment;
	int minStorageAlignment; 


	HostDriverTransferPool<3> transferPool;

	TransferCommandsPool<3> transferCommandPool;
};

namespace VKRenderer {
	extern RenderInstance* gRenderInstance;
}