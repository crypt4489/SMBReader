#include "OSMemory.h"
#include <Windows.h>

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
	DWORD ret = 0;

	ret |= PAGE_EXECUTE * ((protection & OSMemoryAllocationProtections::EXECUTE) != 0);
	ret |= PAGE_READONLY * ((protection & OSMemoryAllocationProtections::READONLY) != 0);
	ret |= PAGE_READWRITE * ((protection & OSMemoryAllocationProtections::READWRITE) != 0);
	
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

void* OSAllocateMemory(void* startingAddress, uint64_t size, OSMemoryAllocationType allocType, OSMemoryAllocationProtection protection)
{
	return VirtualAlloc(startingAddress, size, ConverMemoryAllocationType(allocType), ConvertMemoryProtection(protection));
}

int OSReleaseMemory(void* memAddr, uint64_t size, OSMemoryReleaseTypes freeType)
{
	if (size && (freeType == OSMemoryReleaseTypes::RELEASE)) return -1;

	BOOL virtualFreeReturn = VirtualFree(memAddr, size, ConvertReleaseType(freeType));

	return (virtualFreeReturn ? 0 : -1);
}