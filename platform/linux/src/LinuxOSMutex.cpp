#include "OSMutex.h"
#include <atomic>

static void** handles;
static int* handleTypes;
static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;

static int FindFreeIndex()
{

	return -1;
}

void CloseAllSyncObject()
{
	for (int i = 0; i < maxFreeListEntry; i++)
	{
		
    }
}

OSSyncMemoryRequirements OSGetSyncMemoryRequirements(int maxNumberOfOpenSyncObjects)
{
	OSSyncMemoryRequirements memReqs{ 0, alignof(void*) };
	
	return memReqs;
}

int OSSeedSyncMemory(void* dataSource, int dataSize, int maxNumberSyncObjects)
{
	return 0;
}

int CreateOSSemaphore(OSSemaphore* semaphore, int count)
{
	return 0;
}

int WaitOSSemaphore(OSSemaphore* semaphore, unsigned int waitMS)
{
	return 0;
}

int NotifyOSSemaphore(OSSemaphore* semaphore)
{
	return 0;
}

int DeleteOSSemaphore(OSSemaphore* semaphore)
{
	return 0;
}


int CreateOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	return 0;
}

