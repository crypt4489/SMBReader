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




template<int T_MaxRegionCopy>
struct TransferCommandsPool
{
	std::array<TransferCommand, 1000> transferRegions;
	std::array<TransferCommandLink, 1000> regionLinks;
	TransferCommandLink* linkHead = nullptr;
	TransferCommandLink** popPrev = nullptr;
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