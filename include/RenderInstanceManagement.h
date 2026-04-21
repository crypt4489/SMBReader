#pragma once
#include <atomic>
#include <cstdint>
#include "CommonRenderTypes.h"

enum RenderingBackend
{
	VULKAN = 1,
	DXD12 = 2,
};

enum class AllocationType
{
	STATIC = 0,
	PERFRAME = 1,
	PERDRAW = 2
};

enum class TransferType
{
	CACHED = 0,
	MEMORY = 1,
};

struct ShaderResourceUpdate
{
	ShaderResourceType type;
	int descriptorSet;
	int bindingIndex;
	int copyCount;
	void* data;
	int dataSize;
};

struct DeviceHandleArrayUpdate
{
	int resourceDstBegin;
	int resourceCount;
	int* resourceHandles;
};

struct BufferArrayUpdate
{
	int resourceDstBegin;
	int allocationCount;
	int* allocationIndices;
};

struct GraphicsIntermediaryPipelineInfo
{
	uint32_t drawType;
	int vertexBufferHandle;
	uint32_t vertexCount;
	uint32_t pipelinename;
	uint32_t descCount;
	int* descriptorsetid;
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

struct BufferMemoryTransferRegion
{
	void* data;
	int size;
	int copyCount;
	int allocationIndex;
	int allocoffset;
};

struct TextureMemoryRegion
{
	void* data;
	size_t totalSize;
	int textureIndex;
	int width;
	int height;
	int mipLevels;
	int layers;
	ImageFormat format;
};

struct TransferCommand
{
	int fillVal;
	int size;
	int offset;
	int allocationIndex;
	int copycount;
	BarrierStage dstStage;
	BarrierAction dstAction;
};

struct RenderAllocation
{
	size_t offset;
	size_t deviceAllocSize;
	size_t requestedSize;
	size_t alignment;
	EntryHandle viewIndex;
	AllocationType allocType;
	ComponentFormatType formatType;
	int structureCopies;
	int memIndex;
};

enum AppPipelineHandleType
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
	int pipelineIdentifierGroup;
};

struct PipelineInstanceData
{
	int frameGraphIndices[4];
	int frameGraphRenderPasses[4];
	int frameGraphPipelineIndices[4];
	int frameGraphCount;
	int pipelineCount;
};

enum GPUCommandStreamType
{
	ATTACHMENT_COMMANDS = 1,
	COMPUTE_QUEUE_COMMANDS = 2,
};

struct GPUCommand
{
	GPUCommandStreamType streamType;
	int indexForStreamType;
};


enum class DriverUpdateType
{
	MEMORYUPDATE = 1,
	RESOURCEUPDATE = 2,
	IMAGEMEMORYUPDATE = 3,
	TRANSFERCOMMAND = 4
};

struct RenderDriverUpdateCommandHeader
{
	DriverUpdateType updateType;
};

struct RenderDriverUpdateCommandMemory : public RenderDriverUpdateCommandHeader
{
	int allocationIndex;
	int size;
	int allocOffset;
	int copiesWithin;
	int pad1;
	void* data;

	RenderDriverUpdateCommandHeader* GetNext()
	{
		return (RenderDriverUpdateCommandHeader*)((uintptr_t)this + sizeof(RenderDriverUpdateCommandMemory));
	}
};

struct RenderDriverUpdateCommandFill : public RenderDriverUpdateCommandHeader
{
	int allocationIndex;
	int size;
	int allocOffset;
	int copiesWithin;
	uint32_t fillValue; 
	BarrierStage stage; 
	BarrierAction action;

	RenderDriverUpdateCommandHeader* GetNext()
	{
		return (RenderDriverUpdateCommandHeader*)((uintptr_t)this + sizeof(RenderDriverUpdateCommandFill));
	}
};

struct RenderDriverUpdateCommandImage : public RenderDriverUpdateCommandHeader
{
	int width; 
	int height; 
	int mipLevels; 
	int layers; 
	ImageFormat format;
	void* data;
	int textureIndex;
	size_t totalSize;
	int pad[4];

	RenderDriverUpdateCommandHeader* GetNext()
	{
		return (RenderDriverUpdateCommandHeader*)((uintptr_t)this + sizeof(RenderDriverUpdateCommandImage));
	}

};

struct RenderDriverUpdateCommandResource: public RenderDriverUpdateCommandHeader
{
	int descriptorid; 
	int bindingindex; 
	ShaderResourceType type;
	int cachedDataSize;
	int copies;
	void* data; 

	RenderDriverUpdateCommandHeader* GetNext()
	{
		return (RenderDriverUpdateCommandHeader*)((uintptr_t)this + sizeof(RenderDriverUpdateCommandResource));
	}

};

static_assert(sizeof(RenderDriverUpdateCommandMemory) % 32 == 0);
static_assert(sizeof(RenderDriverUpdateCommandResource) % 32 == 0);
static_assert(sizeof(RenderDriverUpdateCommandImage) % 32 == 0);
static_assert(sizeof(RenderDriverUpdateCommandFill) % 32 == 0);


struct MemoryDriverTransferPool
{
	BufferMemoryTransferRegion* transferRegions;
	int* regionLinks;
	int linkHead = -1;
	int linkCount = 0;
	int ddsRegionAlloc = 0;
	int ddsRegionSize = 0;

	int GetSize(int numberOfRegions)
	{
		return (sizeof(BufferMemoryTransferRegion) + sizeof(int)) * numberOfRegions;
	}

	void AllocateList(void* poolData, int poolSize)
	{
		ddsRegionSize = (int)((float)poolSize/(float)(sizeof(BufferMemoryTransferRegion) + sizeof(int)));

		transferRegions = (BufferMemoryTransferRegion*)poolData;

		regionLinks = (int*)(transferRegions + ddsRegionSize);
	}

	int Create(void* data, int size, int allocationIndex, int allocOffset, int copies)
	{
		int link = Find(allocationIndex, allocOffset);
		
		BufferMemoryTransferRegion* region = nullptr;
		
		if (link < 0)
		{
			int regionAlloc = ddsRegionAlloc;

			ddsRegionAlloc = (ddsRegionAlloc + 1) % ddsRegionSize;

			region = &transferRegions[regionAlloc];

			regionLinks[regionAlloc] = -1;
		
			region->allocationIndex = allocationIndex;
			region->allocoffset = allocOffset;
			region->size = size;
			region->data = data;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link];
			region->data = data;
			region->allocoffset = allocOffset;
			region->size = size;
		}

		region->copyCount = copies;
		return 0;
	}

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while ((*test >= 0) && (transferRegions[(*test)].allocationIndex <= transferRegions[newlink].allocationIndex))
		{
			test = &(regionLinks[(*test)]);
		}
		regionLinks[newlink] = *test;
		*test = newlink;
		linkCount++;
	}
	
	int Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while (link >= 0 && (transferRegions[link].allocationIndex != allocationIndex || transferRegions[link].allocoffset != offset))
		{
			link = regionLinks[link];
		}
		return (link);
	}

	int PopLink(BufferMemoryTransferRegion* outputRegion, int link, int** popprev)
	{
		if (link < 0 || link >= ddsRegionSize) return -1;
		outputRegion->allocationIndex = transferRegions[link].allocationIndex;
		outputRegion->size = transferRegions[link].size;
		outputRegion->copyCount = transferRegions[link].copyCount;
		outputRegion->data = transferRegions[link].data;
		outputRegion->allocoffset = transferRegions[link].allocoffset;
		int linkRet = regionLinks[link];
		if (transferRegions[link].copyCount > 1)
		{
			transferRegions[link].copyCount--;
			*popprev = &regionLinks[link];
		}
		else
		{
			*(*popprev) = linkRet;
			linkCount--;
			transferRegions[link].allocationIndex = -1;
			transferRegions[link].size = 0;
			transferRegions[link].copyCount = -1;
			transferRegions[link].allocoffset = -1;
			transferRegions[link].data = nullptr;
			regionLinks[link] = -1;
		}
		return linkRet;
	}
};

struct TransferCommandsPool
{
	TransferCommand* transferRegions;
	int* regionLinks;
	int linkHead = -1;
	int linkCount = 0;
	int ddsRegionAlloc = 0;
	int ddsRegionSize = 0;

	int GetSize(int numberOfRegions)
	{
		return (sizeof(TransferCommand) + sizeof(int)) * numberOfRegions;
	}

	void AllocateList(void* poolData, int poolSize)
	{
		ddsRegionSize = (int)((float)poolSize / (float)(sizeof(TransferCommand) + sizeof(int)));

		transferRegions = (TransferCommand*)poolData;

		regionLinks = (int*)(transferRegions + ddsRegionSize);
	}

	int Create(int allocationIndex, int size, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action, int copies)
	{
		int link = Find(allocationIndex, allocOffset);
		TransferCommand* region = nullptr;

		if (link < 0)
		{
			int regionAlloc = ddsRegionAlloc;

			ddsRegionAlloc = (ddsRegionAlloc + 1) % ddsRegionSize;

			region = &transferRegions[regionAlloc];

			regionLinks[regionAlloc] = -1;

			region->allocationIndex = allocationIndex;
			region->offset = allocOffset;
			region->size = size;
			region->fillVal = fillValue;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link];
			region->fillVal = fillValue;
			region->offset = allocOffset;
			region->size = size;
		}

		region->copycount = copies;
		region->dstStage = stage;
		region->dstAction = action;

		return 0;
	}

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while ((*test >= 0) && (transferRegions[*test].allocationIndex <= transferRegions[newlink].allocationIndex))
		{
			test = &(regionLinks[*test]);
		}
		regionLinks[newlink] = *test;
		*test = newlink;
		linkCount++;
	}

	int Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while ((link >= 0) && (transferRegions[link].allocationIndex != allocationIndex || transferRegions[link].offset != offset))
		{
			link = regionLinks[link];
		}
		return link;
	}

	int PopLink(TransferCommand* outputRegion, int link, int** popprev)
	{
		if (link < 0 || link >= ddsRegionSize) return -1;
		outputRegion->allocationIndex = transferRegions[link].allocationIndex;
		outputRegion->size = transferRegions[link].size;
		outputRegion->copycount = transferRegions[link].copycount;
		outputRegion->fillVal = transferRegions[link].fillVal;
		outputRegion->offset = transferRegions[link].offset;
		outputRegion->dstStage = transferRegions[link].dstStage;
		outputRegion->dstAction = transferRegions[link].dstAction;
		int linkRet = regionLinks[link];
		if (transferRegions[link].copycount > 1)
		{
			transferRegions[link].copycount--;
			*popprev = &regionLinks[link];
		}
		else
		{
			*(*popprev) = linkRet;
			linkCount--;
			TransferCommand* region = &transferRegions[link];
			region->allocationIndex = -1;
			region->size = 0;
			region->copycount = -1;
			region->offset = -1;
			region->fillVal = 0;
			region->dstAction = -1;
			region->dstStage = -1;
		}
		return linkRet;
	}
};

struct ImageMemoryUpdateManager
{
	TextureMemoryRegion* transferRegions;
	int* regionLinks;
	int linkHead = -1;
	int linkCount = 0;
	int ddsRegionAlloc = 0;
	int ddsRegionSize = 0;

	int GetSize(int numberOfRegions)
	{
		return (sizeof(TextureMemoryRegion) + sizeof(int)) * numberOfRegions;
	}

	void AllocateList(void* poolData, int poolSize)
	{
		ddsRegionSize = (int)((float)poolSize / (float)(sizeof(TextureMemoryRegion) + sizeof(int)));

		transferRegions = (TextureMemoryRegion*)poolData;

		regionLinks = (int*)(transferRegions + ddsRegionSize);
	}

	int Create(void* data, int textureIndex, size_t totalSize, int width, int height, int mipLevels, int layers, ImageFormat format)
	{
		int link = Find(textureIndex);
		TextureMemoryRegion* region = nullptr;

		if (link >= 0) return -1;
		
		int regionAlloc = ddsRegionAlloc;

		ddsRegionAlloc = (ddsRegionAlloc + 1) % ddsRegionSize;

		region = &transferRegions[regionAlloc];

		regionLinks[regionAlloc] = -1;

		region->data = data;
		region->height = height;
		region->width = width;
		region->totalSize = totalSize;
		region->mipLevels = mipLevels;
		region->textureIndex = textureIndex;
		region->format = format;
		region->layers = layers;

		Insert(regionAlloc);
		
		return 0; 
	}

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while (*test >= 0)
		{
			test = &(regionLinks[(*test)]);
		}
		regionLinks[newlink] = *test;
		*test = newlink;
		linkCount++;
	}

	int Find(int textureIndex)
	{
		int link = linkHead;
		while (link >= 0 && transferRegions[link].textureIndex != textureIndex)
		{
			link = regionLinks[link];
		}
		return (link);
	}

	int PopLink(TextureMemoryRegion* outputRegion, int link)
	{
		if (link < 0 || link >= ddsRegionSize)
			return -1;

		int regionIndex = link;
		TextureMemoryRegion* src = &transferRegions[regionIndex];

		outputRegion->data = src->data;
		outputRegion->totalSize = src->totalSize;
		outputRegion->textureIndex = src->textureIndex;
		outputRegion->width = src->width;
		outputRegion->height = src->height;
		outputRegion->mipLevels = src->mipLevels;
		outputRegion->format = src->format;
		outputRegion->layers = src->layers;

		int linkRet = regionLinks[link];

		linkCount--;


		src->data = nullptr;
		src->totalSize = 0;
		src->textureIndex = {};
		src->width = 0;
		src->height = 0;
		src->mipLevels = 0;
		src->format = {};
		src->layers = -1;

		regionLinks[link] = -1;

		return linkRet;
	}
};

struct ShaderResourceUpdatePool
{
	ShaderResourceUpdate* transferRegions;
	int* regionLinks;
	int linkHead = -1;
	int linkCount = 0;
	int ddsRegionAlloc = 0;
	int ddsRegionSize = 0;

	int GetSize(int numberOfRegions)
	{
		return (sizeof(ShaderResourceUpdate) + sizeof(int)) * numberOfRegions;
	}

	void AllocateList(void* poolData, int poolSize)
	{
		ddsRegionSize = (int)((float)poolSize / (float)(sizeof(ShaderResourceUpdate) + sizeof(int)));

		transferRegions = (ShaderResourceUpdate*)poolData;

		regionLinks = (int*)(transferRegions + ddsRegionSize);
	}

	int Create(int descriptorid, int bindingindex, ShaderResourceType type, void* data, int cachedDataSize, int copies)
	{
		ShaderResourceUpdate* region = nullptr;

		int regionAlloc = ddsRegionAlloc;

		ddsRegionAlloc = (ddsRegionAlloc + 1) % (int)ddsRegionSize;

		region = &transferRegions[regionAlloc];

		region->data = data;
		region->dataSize = cachedDataSize;
		region->descriptorSet = descriptorid;
		region->bindingIndex = bindingindex;
		region->type = type;
		region->copyCount = copies;

		regionLinks[regionAlloc] = -1;

		Insert(regionAlloc);

		return 0;
	}

	void Insert(int index)
	{
		int newid = transferRegions[index].descriptorSet;
		int newbindingindex = transferRegions[index].bindingIndex;
		int* test = &linkHead;
		while ((*test >= 0) && (transferRegions[(*test)].descriptorSet <= newid))
		{
			if (transferRegions[(*test)].bindingIndex < newbindingindex)
				break;
			test = &(regionLinks[(*test)]);
		}
		regionLinks[index] = *test;
		*test = index;
		linkCount++;
	}

	int Find(int descriptor, int bindingindex)
	{
		int link = linkHead;
		while ((link >= 0) && ((transferRegions[link].descriptorSet != descriptor) || (transferRegions[link].bindingIndex != bindingindex)))
		{
			link = regionLinks[link];
		}
		return (link);
	}

	int PopLink(ShaderResourceUpdate* outputRegion, int link, int** popprev)
	{
		if (link < 0 || link >= ddsRegionSize) return -1;

		ShaderResourceUpdate* region = &transferRegions[link];

		outputRegion->type = region->type;
		outputRegion->descriptorSet = region->descriptorSet;
		outputRegion->bindingIndex = region->bindingIndex;
		outputRegion->copyCount = region->copyCount;
		outputRegion->dataSize = region->dataSize;
		outputRegion->data = region->data;

		int linkRet = regionLinks[link];

		if (region->copyCount > 1)
		{
			region->copyCount--;
			*popprev = &regionLinks[link];
		}
		else
		{
			*(*popprev) = linkRet;
			linkCount--;
			region->bindingIndex = -1;
			region->copyCount = -1;
			region->data = nullptr;
			region->descriptorSet = -1;
			region->type = ShaderResourceType::INVALID_SHADER_RESOURCE;
			region->dataSize = -1;
			regionLinks[link] = -1;
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