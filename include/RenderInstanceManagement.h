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
		linkCount.fetch_add(1);
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
			linkCount--;
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
	std::array<TransferCommandLink, 1000> regionLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> ddsRegionAlloc = 0;

	int Create(int size, int allocationIndex, int allocOffset, uint32_t fillValue, BarrierStage stage, BarrierAction action)
	{
		TransferCommandLink* link = Find(allocationIndex, allocOffset);
		TransferCommand* region = nullptr;

		if (!link)
		{
			int regionAlloc = UpdateAtomic(ddsRegionAlloc, 1, (int)transferRegions.size());

			region = &transferRegions[regionAlloc];

			link = &regionLinks[regionAlloc];


			region->fillVal = fillValue;


			link->command = regionAlloc;
			link->next = -1;

			region->allocationIndex = allocationIndex;
			region->offset = allocOffset;
			region->size = size;

			Insert(regionAlloc);
		}
		else
		{
			region = &transferRegions[link->command];


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
		while ((*test >= 0) && (transferRegions[regionLinks[*test].command].allocationIndex <= transferRegions[regionLinks[newlink].command].allocationIndex))
		{
			test = &(regionLinks[*test].next);
		}
		regionLinks[newlink].next = *test;
		*test = newlink;
		linkCount.fetch_add(1);
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

	TransferCommandLink* Find(int allocationIndex, int offset)
	{
		int link = linkHead;
		while ((link >= 0) && (transferRegions[regionLinks[link].command].allocationIndex != allocationIndex || transferRegions[regionLinks[link].command].offset != offset))
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
		outputRegion->allocationIndex = transferRegions[regionLinks[link].command].allocationIndex;
		outputRegion->size = transferRegions[regionLinks[link].command].size;
		outputRegion->copycount = transferRegions[regionLinks[link].command].copycount;
		outputRegion->fillVal = transferRegions[regionLinks[link].command].fillVal;
		outputRegion->offset = transferRegions[regionLinks[link].command].offset;
		outputRegion->dstStage = transferRegions[regionLinks[link].command].dstStage;
		outputRegion->dstAction = transferRegions[regionLinks[link].command].dstAction;
		int linkRet = regionLinks[link].next;
		if (transferRegions[regionLinks[link].command].copycount > 1)
		{
			transferRegions[regionLinks[link].command].copycount--;
			popPrev = &regionLinks[link].next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount--;
			TransferCommand* region = &transferRegions[regionLinks[link].command];
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