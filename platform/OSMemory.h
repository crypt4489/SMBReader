#pragma once

#include "OS.h"

enum OSMemoryAllocationTypes
{
	COMMIT = 1,
	RESERVE = 2,
	USE_LARGE_PAGES = 4
};

enum OSMemoryAllocationProtections
{
	READWRITE = 1,
	READONLY = 2,
	EXECUTE = 4,
};

enum OSMemoryReleaseTypes
{
	RELEASE = 1,
	DECOMMIT = 2,
};

enum OSMemoryAllocationErrors
{
	OS_MEMORY_SUCCESS = 0,
	OS_MEMORY_FREE_FAILURE = -1,
	OS_MEMORY_HANDLE_EXHAUSTION = -2
};

struct OSMemoryRequirements
{
	int dataSize;
	int alignment;
};

typedef int OSMemoryAllocationType;
typedef int OSMemoryAllocationProtection;

OSMemoryRequirements OSGetMemoryRequirements(int maxNumberOfAllocations);

int OSSeedMemory(void* dataSource, int dataSize, int maxNumberOfAllocations);

void ReleaseAllMemoryAllocations();

uint64_t OSGetStandardPageSize();

uint64_t OSGetLargePageSize();

void* OSMemoryAllocate(void* startingAddress, uint64_t size, OSMemoryAllocationType allocType, OSMemoryAllocationProtection protection);

int OSMemoryRelease(void* memAddr, uint64_t size, OSMemoryReleaseTypes freeType);