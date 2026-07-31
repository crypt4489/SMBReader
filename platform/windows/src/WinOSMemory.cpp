#include "OSMemory.h"
#include <Windows.h>
#include <atomic>

#ifdef _MSC_VER
#define ALIGNAS(x) __declspec(align(x))
#endif

struct MPMCQueueData
{
	std::atomic<size_t> currentSequence;
	int freeIndex;
};

#define BLOCK_HEADER_SENTINEL_VALUE 0xbbadbeefbbadbeef

#define MAX_DETAILS_PROTECTION 255
#define MAX_DETAILS_ALLOC_TYPE 255
#define MAX_ALLOCATION_INDEX  65535

#define BLOCK_DETAILS_PROTECTION_OFFSET 0
#define BLOCK_DETAILS_ALLOC_TYPE_OFFSET 8
#define BLOCK_DETAILS_ALLOCATION_INDEX_OFFSET 16

#define MAKE_BLOCK_DETAILS(allocIndex, prot, allocType) ((allocIndex << BLOCK_DETAILS_ALLOCATION_INDEX_OFFSET) | (prot << BLOCK_DETAILS_PROTECTION_OFFSET) | (allocType << BLOCK_DETAILS_ALLOC_TYPE_OFFSET))

#define GET_BLOCK_DETAILS_ALLOCATION_INDEX(packed) ((packed >> BLOCK_DETAILS_ALLOCATION_INDEX_OFFSET) & MAX_ALLOCATION_INDEX)
#define GET_BLOCK_DETAILS_ALLOCATION_TYPE(packed) ((packed >> BLOCK_DETAILS_ALLOC_TYPE_OFFSET) & MAX_DETAILS_ALLOC_TYPE)
#define GET_BLOCK_DETAILS_PROTECTION(packed) ((packed >> BLOCK_DETAILS_PROTECTION_OFFSET) & MAX_DETAILS_PROTECTION)

struct MemBlockHeader
{
    size_t blockCommitSentinel;
	size_t blockDetails;
	size_t blockSize; 
    size_t blockCommitSize;
};

static MPMCQueueData* freeList;
static void** memoryLocations;
static int maxFreeListEntry = 0;

ALIGNAS(64) static std::atomic<int> boundedLinearAllocator{ 0 };
ALIGNAS(64) static std::atomic<size_t> enqueuePos{ 0 };
ALIGNAS(64) static std::atomic<size_t> dequeuePos{ 0 };

static int PopFromFreeList()
{
    MPMCQueueData* cell;

    size_t pos = dequeuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0)
        {
            if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return -1;
        else
            pos = dequeuePos.load(std::memory_order_relaxed);
    }

    cell->currentSequence.store(pos + maxFreeListEntry, std::memory_order_release);

    int freeListIndex = cell->freeIndex;

    cell->freeIndex = -1;

    return freeListIndex;
}

static void ReturnIndex(int index)
{
    MPMCQueueData* cell;

    size_t pos = enqueuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return;
        else
            pos = enqueuePos.load(std::memory_order_relaxed);
    }

    memoryLocations[index] = nullptr;

    cell->freeIndex = index;
    cell->currentSequence.store(pos + 1, std::memory_order_release);
}

static int FindFreeIndex()
{
    int ret = PopFromFreeList();

    if (ret < 0)
    {
        int linearTop = boundedLinearAllocator.load(std::memory_order_acquire);

        while (linearTop < maxFreeListEntry)
        {
            if (boundedLinearAllocator.compare_exchange_weak(linearTop, linearTop + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                ret = linearTop;
                break;
            }
        }
    }

    return ret;
}

static DWORD ConverMemoryAllocationType(OSMemoryAllocationType allocType)
{
	DWORD ret = 0;

	ret |= MEM_LARGE_PAGES * ((allocType & OSMemoryAllocationTypes::USE_LARGE_PAGES) != 0);
	ret |= MEM_COMMIT * ((allocType & OSMemoryAllocationTypes::COMMIT) != 0);
	ret |= MEM_RESERVE * ((allocType & OSMemoryAllocationTypes::RESERVE) != 0);

	return ret;
}

static DWORD ConvertMemoryProtection(OSMemoryAllocationProtection protection)
{
    DWORD ret = PAGE_NOACCESS;

    if (protection == OSMemoryAllocationProtections::READONLY)
    {
        ret = PAGE_READONLY;
    }

    if (protection == OSMemoryAllocationProtections::READWRITE)
    {
        ret = PAGE_READWRITE;
    }

    if (protection == OSMemoryAllocationProtections::EXECUTE)
    {
        ret = PAGE_EXECUTE;
    }

    if (protection == (OSMemoryAllocationProtections::EXECUTE | OSMemoryAllocationProtections::READONLY))
    {
        ret = PAGE_EXECUTE_READ;
    }

    if (protection == (OSMemoryAllocationProtections::EXECUTE | OSMemoryAllocationProtections::READWRITE))
    {
        ret = PAGE_EXECUTE_READWRITE;
    }

    return ret;
}

static DWORD ConvertReleaseType(OSMemoryReleaseTypes release)
{
	if (release == OSMemoryReleaseTypes::DECOMMIT)
		return MEM_DECOMMIT;
	else if (release == OSMemoryReleaseTypes::RELEASE)
		return MEM_RELEASE;
	
	return 0;
}

OSMemoryRequirements OSGetMemoryRequirements(int maxNumberOfAllocations)
{
    int handlesSize = (maxNumberOfAllocations) * sizeof(void*);
    int freeListSize = (maxNumberOfAllocations) * sizeof(MPMCQueueData);

    OSMemoryRequirements memReqs{ handlesSize + freeListSize, alignof(void*) };

    return memReqs;
}

int OSSeedMemory(void* dataSource, int dataSize, int maxNumberOfAllocations)
{
    uintptr_t dataHead = (uintptr_t)dataSource;

    memoryLocations = (void**)dataSource;

    int handleSize = maxNumberOfAllocations;

    dataHead += sizeof(void*) * handleSize;

    freeList = (MPMCQueueData*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
        memoryLocations[i] = nullptr;
    }

    maxFreeListEntry = handleSize;

    return OS_MEMORY_SUCCESS;
}

void ReleaseAllMemoryAllocations()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        if (memoryLocations[i])
        {
            BOOL ret = VirtualFree(memoryLocations[i], 0, MEM_RELEASE);
            memoryLocations[i] = nullptr;
        }

        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
    }

    enqueuePos.store(0, std::memory_order_relaxed);
    dequeuePos.store(0, std::memory_order_relaxed);
    boundedLinearAllocator.store(0, std::memory_order_relaxed);
}

uint64_t OSGetStandardPageSize()
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	return systemInfo.dwPageSize;
}

uint64_t OSGetLargePageSize()
{
	return GetLargePageMinimum();
}

void* OSMemoryAllocate(void* startingAddress, uint64_t size, OSMemoryAllocationType allocType, OSMemoryAllocationProtection protection)
{
    uint64_t pageSize;

    if (allocType & OSMemoryAllocationTypes::USE_LARGE_PAGES)
    {
        if (!(allocType & OSMemoryAllocationTypes::COMMIT))
        {
            return nullptr;
        }

        pageSize = OSGetLargePageSize();
    }
    else
    {
        pageSize = OSGetStandardPageSize();
    }

    int index = -1;

    MemBlockHeader* blockHeader = nullptr;

    void* retAddr = nullptr;

    uint64_t adjustedSize = size;

    if (!startingAddress)
    {
        adjustedSize = ((adjustedSize + sizeof(MemBlockHeader)) + (pageSize - 1)) & ~(pageSize - 1);

        index = FindFreeIndex();

        if (index < 0)
        {
            return retAddr;
        }

        retAddr = VirtualAlloc(startingAddress, adjustedSize, ConverMemoryAllocationType(allocType), ConvertMemoryProtection(protection));

        if (!retAddr)
        {
            ReturnIndex(index);
            return retAddr;
        }
    }
    else
    {
        MemBlockHeader* potentialBlockHeader = ((MemBlockHeader*)startingAddress) - 1;

        size_t currentCommitHeader = 0;

        if (potentialBlockHeader->blockCommitSentinel == BLOCK_HEADER_SENTINEL_VALUE)
        {
            currentCommitHeader = potentialBlockHeader->blockCommitSize;
        }
        else
        {
            index = FindFreeIndex();

            if (index < 0)
            {
                return retAddr;
            }

            adjustedSize = ((size + sizeof(MemBlockHeader)) + (pageSize - 1)) & ~(pageSize - 1);
        }

        void* retAddr = VirtualAlloc((void*)((uintptr_t)potentialBlockHeader + currentCommitHeader), adjustedSize, ConverMemoryAllocationType(allocType), ConvertMemoryProtection(protection));

        if (!retAddr)
        {
            if (potentialBlockHeader->blockCommitSentinel != BLOCK_HEADER_SENTINEL_VALUE)
            {
                ReturnIndex(index);
            }

            return retAddr;
        }

        if ((potentialBlockHeader->blockCommitSentinel == BLOCK_HEADER_SENTINEL_VALUE) && (allocType & OSMemoryAllocationTypes::COMMIT))
        {
            potentialBlockHeader->blockCommitSize += adjustedSize;
            return retAddr;
        }
    }

    if ((allocType & OSMemoryAllocationTypes::RESERVE) && !(allocType & OSMemoryAllocationTypes::COMMIT))
    {
        void* committedAddr = VirtualAlloc(retAddr, sizeof(MemBlockHeader), ConverMemoryAllocationType(OSMemoryAllocationTypes::COMMIT), ConvertMemoryProtection(protection));

        if (!committedAddr)
        {
            VirtualFree(retAddr, 0, MEM_RELEASE);
            ReturnIndex(index);
            return committedAddr;
        }
    }

    blockHeader = (MemBlockHeader*)retAddr;

    blockHeader->blockCommitSentinel = BLOCK_HEADER_SENTINEL_VALUE;
    blockHeader->blockDetails = MAKE_BLOCK_DETAILS(index, protection, allocType);
    blockHeader->blockSize = adjustedSize;
    blockHeader->blockCommitSize = (allocType & OSMemoryAllocationTypes::COMMIT) ? adjustedSize : sizeof(MemBlockHeader);

    memoryLocations[index] = blockHeader;

    return blockHeader + 1;
}

int OSMemoryRelease(void* memAddr, uint64_t size, OSMemoryReleaseTypes freeType)
{
    if (size && (freeType == OSMemoryReleaseTypes::RELEASE))
    {
        return OS_MEMORY_FREE_FAILURE;
    }

    MemBlockHeader* header = ((MemBlockHeader*)memAddr) - 1;

    if (header->blockCommitSentinel != BLOCK_HEADER_SENTINEL_VALUE)
    {
        return OS_MEMORY_FREE_FAILURE;
    }

    size_t headerCommitSize = header->blockCommitSize;

    if (headerCommitSize < size)
    {
        return OS_MEMORY_FREE_FAILURE;
    }

    uintptr_t absoluteMemAddr = (uintptr_t)header;

    if (freeType == OSMemoryReleaseTypes::DECOMMIT)
    {
        absoluteMemAddr = absoluteMemAddr + (headerCommitSize - size);
    }

    BOOL virtualFreeReturn = VirtualFree((void*)absoluteMemAddr, size, ConvertReleaseType(freeType));

    if (freeType == OSMemoryReleaseTypes::RELEASE)
    {
        int index = GET_BLOCK_DETAILS_ALLOCATION_INDEX(header->blockDetails);
        ReturnIndex(index);
    }
    else
    {
        header->blockCommitSize -= size;
    }

	return (virtualFreeReturn ? OS_MEMORY_SUCCESS : OS_MEMORY_FREE_FAILURE);
}