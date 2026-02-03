#pragma once
#include <atomic>
#include <cstdint>
#include "AppTypes.h"


template<int T_MaxRegionCopy>
struct HostDriverTransferPool
{
	char stagingbuffer[32 * 1024];
	std::array<HostTransferRegion, 1000> transferRegions;
	std::array<TransferRegionLink, 1000> regionLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
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

			link = &regionLinks[regionAlloc];



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


			link->region = regionAlloc;
			link->next = -1;

			region->allocationIndex = allocationIndex;
			region->allocoffset = allocOffset;
			region->size = size;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link->region];

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

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while ((*test >= 0) && (transferRegions[regionLinks[(*test)].region].allocationIndex <= transferRegions[regionLinks[newlink].region].allocationIndex))
		{
			test = &(regionLinks[(*test)].next);
		}
		regionLinks[newlink].next = *test;
		*test = newlink;
		linkCount.fetch_add(1, std::memory_order_release);
	}
	
	TransferRegionLink* Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while (link >= 0 && (transferRegions[regionLinks[link].region].allocationIndex != allocationIndex || transferRegions[regionLinks[link].region].allocoffset != offset))
		{
			link = regionLinks[link].next;
		}
		return (link == -1 || link >= 1000 ? nullptr : &regionLinks[link]);
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	int PopLink(HostTransferRegion* outputRegion, int link)
	{
		if (link < 0 || link >= 1000) return -1;
		outputRegion->allocationIndex = transferRegions[regionLinks[link].region].allocationIndex;
		outputRegion->size = transferRegions[regionLinks[link].region].size;
		outputRegion->copyCount = transferRegions[regionLinks[link].region].copyCount;
		outputRegion->data = transferRegions[regionLinks[link].region].data;
		outputRegion->allocoffset = transferRegions[regionLinks[link].region].allocoffset;
		int linkRet = regionLinks[link].next;
		if (transferRegions[regionLinks[link].region].copyCount > 1)
		{
			transferRegions[regionLinks[link].region].copyCount--;
			popPrev = &regionLinks[link].next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount.fetch_sub(1);
			transferRegions[regionLinks[link].region].allocationIndex = -1;
			transferRegions[regionLinks[link].region].size = 0;
			transferRegions[regionLinks[link].region].copyCount = -1;
			transferRegions[regionLinks[link].region].allocoffset = -1;
			transferRegions[regionLinks[link].region].data = nullptr;
			regionLinks[link].next = -1;
			regionLinks[link].region = -1;
		}
		return linkRet;
	}
};




template<int T_MaxRegionCopy>
struct TransferCommandsPool
{
	std::array<TransferCommand, 1000> transferRegions;
	std::array<TransferRegionLink, 1000> regionLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0;

	int Create(int size, int allocationIndex, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action)
	{
		TransferRegionLink* link = Find(allocationIndex, allocOffset);
		TransferCommand* region = nullptr;

		if (!link)
		{
			int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());

			region = &transferRegions[regionAlloc];

			link = &regionLinks[regionAlloc];


			region->fillVal = fillValue;


			link->region = regionAlloc;
			link->next = -1;

			region->allocationIndex = allocationIndex;
			region->offset = allocOffset;
			region->size = size;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link->region];


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

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while ((*test >= 0) && (transferRegions[regionLinks[*test].region].allocationIndex <= transferRegions[regionLinks[newlink].region].allocationIndex))
		{
			test = &(regionLinks[*test].next);
		}
		regionLinks[newlink].next = *test;
		*test = newlink;
		linkCount.fetch_add(1, std::memory_order_release);
	}
	/*
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
	*/

	TransferRegionLink* Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while ((link >= 0) && (transferRegions[regionLinks[link].region].allocationIndex != allocationIndex || transferRegions[regionLinks[link].region].offset != offset))
		{
			link = regionLinks[link].next;
		}
		return (link == -1 || link >= 1000 ? nullptr : &regionLinks[link]);
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	int PopLink(TransferCommand* outputRegion, int link)
	{
		if (link < 0) return -1;
		outputRegion->allocationIndex = transferRegions[regionLinks[link].region].allocationIndex;
		outputRegion->size = transferRegions[regionLinks[link].region].size;
		outputRegion->copycount = transferRegions[regionLinks[link].region].copycount;
		outputRegion->fillVal = transferRegions[regionLinks[link].region].fillVal;
		outputRegion->offset = transferRegions[regionLinks[link].region].offset;
		outputRegion->dstStage = transferRegions[regionLinks[link].region].dstStage;
		outputRegion->dstAction = transferRegions[regionLinks[link].region].dstAction;
		int linkRet = regionLinks[link].next;
		if (transferRegions[regionLinks[link].region].copycount > 1)
		{
			transferRegions[regionLinks[link].region].copycount--;
			popPrev = &regionLinks[link].next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount.fetch_sub(1);
			TransferCommand* region = &transferRegions[regionLinks[link].region];
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


struct DeviceMemoryUpdateManager
{
	char stagingbuffer[32 * 1024];
	std::array<DeviceTransferRegion, 1000> transferRegions;
	std::array<TransferRegionLink, 1000> regionLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0;
	std::atomic<int> currentStagingBufferWrite = 0;

	int Create(void* data, int size, int allocationIndex, int allocOffset, int copyCount, TransferType transferType)
	{

		TransferRegionLink* link = Find(allocationIndex, allocOffset);
		DeviceTransferRegion* region = nullptr;

		if (!link)
		{
			int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());

			region = &transferRegions[regionAlloc];

			link = &regionLinks[regionAlloc];

			if (transferType == TransferType::CACHED)
			{
				int writeLoc = UpdateAtomic(currentStagingBufferWrite, size, (int)sizeof(stagingbuffer));
				memcpy(stagingbuffer + writeLoc, data, size);
				region->data = stagingbuffer + writeLoc;
			}
			else if (transferType == TransferType::MEMORY)
			{
				region->data = data;
			}


			link->region = regionAlloc;
			link->next = -1;
			
			region->allocationIndex = allocationIndex;
			region->allocoffset = allocOffset;
			region->size = size;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link->region];

			if (transferType == TransferType::CACHED)
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


		region->transferType = transferType;
		region->copyCount = copyCount;

		return 0;
	}

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while ((*test >= 0))
		{
			test = &(regionLinks[(*test)].next);
		}
		regionLinks[newlink].next = *test;
		*test = newlink;
		linkCount.fetch_add(1, std::memory_order_release);
	}

	TransferRegionLink* Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while (link >= 0 && (transferRegions[regionLinks[link].region].allocationIndex != allocationIndex || transferRegions[regionLinks[link].region].allocoffset != offset))
		{
			link = regionLinks[link].next;
		}
		return (link == -1 || link >= 1000 ? nullptr : &regionLinks[link]);
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	int PopLink(DeviceTransferRegion* outputRegion, int link)
	{
		if (link < 0 || link >= 1000) return -1;
		outputRegion->allocationIndex = transferRegions[regionLinks[link].region].allocationIndex;
		outputRegion->size = transferRegions[regionLinks[link].region].size;
		outputRegion->copyCount = transferRegions[regionLinks[link].region].copyCount;
		outputRegion->data = transferRegions[regionLinks[link].region].data;
		outputRegion->allocoffset = transferRegions[regionLinks[link].region].allocoffset;
		outputRegion->transferType = transferRegions[regionLinks[link].region].transferType;
		int linkRet = regionLinks[link].next;
		if (transferRegions[regionLinks[link].region].copyCount > 1)
		{
			transferRegions[regionLinks[link].region].copyCount--;
			popPrev = &regionLinks[link].next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount.fetch_sub(1);
			transferRegions[regionLinks[link].region].allocationIndex = -1;
			transferRegions[regionLinks[link].region].size = 0;
			transferRegions[regionLinks[link].region].copyCount = -1;
			transferRegions[regionLinks[link].region].allocoffset = -1;
			transferRegions[regionLinks[link].region].data = nullptr;
			//transferRegions[regionLinks[link].region].transferType
			regionLinks[link].next = -1;
			regionLinks[link].region = -1;
		}
		return linkRet;
	}
};



struct ImageMemoryUpdateManager
{
	std::array<TextureMemoryRegion, 1000> transferRegions;
	std::array<TransferRegionLink, 1000> regionLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0;
	std::atomic<int> currentStagingBufferWrite = 0;

	int Create(void* data, EntryHandle textureIndex, uint32_t* imageSizes, size_t totalSize, int width, int height, int mipLevels, ImageFormat format)
	{
		
		TransferRegionLink* link = Find(textureIndex);
		TextureMemoryRegion* region = nullptr;


		if (link) return -1;
		
		int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());

		region = &transferRegions[regionAlloc];

		link = &regionLinks[regionAlloc];

		
		region->data = data;
		region->imageSizes = imageSizes;
		region->height = height;
		region->width = width;
		region->totalSize = totalSize;
		region->mipLevels = mipLevels;
		region->textureIndex = textureIndex;
		region->format = format;

		link->region = regionAlloc;
		link->next = -1;


		Insert(regionAlloc);
		
		return 0; 
	}

	void Insert(int newlink)
	{
		int* test = &linkHead;
		while (*test >= 0)
		{
			test = &(regionLinks[(*test)].next);
		}
		regionLinks[newlink].next = *test;
		*test = newlink;
		linkCount.fetch_add(1, std::memory_order_release);
	}

	TransferRegionLink* Find(EntryHandle textureIndex)
	{
		int link = linkHead;
		while (link >= 0 && transferRegions[regionLinks[link].region].textureIndex != textureIndex)
		{
			link = regionLinks[link].next;
		}
		return (link == -1 || link >= 1000 ? nullptr : &regionLinks[link]);
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	int PopLink(TextureMemoryRegion* outputRegion, int link)
	{
		if (link < 0 || link >= 1000)
			return -1;

		int regionIndex = regionLinks[link].region;
		TextureMemoryRegion* src = &transferRegions[regionIndex];

		outputRegion->data = src->data;
		outputRegion->imageSizes = src->imageSizes;
		outputRegion->totalSize = src->totalSize;
		outputRegion->textureIndex = src->textureIndex;
		outputRegion->width = src->width;
		outputRegion->height = src->height;
		outputRegion->mipLevels = src->mipLevels;
		outputRegion->format = src->format;

		int linkRet = regionLinks[link].next;

		*popPrev = linkRet;

		linkCount.fetch_sub(1);


		src->data = nullptr;
		src->imageSizes = nullptr;
		src->totalSize = 0;
		src->textureIndex = {};
		src->width = 0;
		src->height = 0;
		src->mipLevels = 0;
		src->format = {};

		regionLinks[link].next = -1;
		regionLinks[link].region = -1;

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