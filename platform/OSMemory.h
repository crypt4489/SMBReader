#pragma once

#include <stdint.h>

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

typedef int OSMemoryAllocationType;
typedef int OSMemoryAllocationProtection;

void OSMemoryInitialize(int totalNumberOfAllocations);

uint64_t OSGetStandardPageSize();

uint64_t OSGetLargePageSize();

void* OSAllocateMemory(void* startingAddress, uint64_t size, OSMemoryAllocationType allocType, OSMemoryAllocationProtection protection);

int OSReleaseMemory(void* memAddr, uint64_t size, OSMemoryReleaseTypes freeType);