#include "OSMutex.h"
#include <Windows.h>
#include <atomic>

enum WinHandleType
{
	SEMAPHORE_HANDLE = 1,
	SRWLOCK_HANDLE = 2,
};

static void** handles;
static int* handleTypes;
static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;

static int FindFreeIndex()
{
	for (int i = 0; i < maxFreeListEntry; i++)
	{
		int idx = i;

		int8_t expected = 1;
		if (freeList[idx].compare_exchange_strong(
			expected,
			-1,
			std::memory_order_acquire,
			std::memory_order_relaxed))
		{
			return idx;
		}
	}
	return -1;
}

OSSyncMemoryRequirements OSGetSyncMemoryRequirements(int maxNumberOfOpenSyncObjects)
{
	int handlesSize = (maxNumberOfOpenSyncObjects) * sizeof(void*);
	int handlesTypeSize = (maxNumberOfOpenSyncObjects) * sizeof(int);
	int freeListSize = (maxNumberOfOpenSyncObjects) * sizeof(std::atomic<int8_t>);

	OSSyncMemoryRequirements memReqs{ handlesSize + handlesTypeSize + freeListSize, alignof(void*) };
	
	return memReqs;
}

int OSSeedSyncMemory(void* dataSource, int dataSize, int maxNumberSyncObjects)
{
	uintptr_t dataHead = (uintptr_t)dataSource;
	uintptr_t dataStart = dataHead;

	handles = (void**)dataSource;

	int handleSize = maxNumberSyncObjects;

	dataHead += handleSize * sizeof(void*);

	handleTypes = (int*)dataHead;

	dataHead += sizeof(int) * handleSize;

	freeList = (std::atomic<int8_t>*)dataHead;

	for (int i = 0; i < handleSize; i++)
	{
		freeList[i] = 1;
	}

	maxFreeListEntry = handleSize;

	return 0;
}

int CreateOSSemaphore(OSSemaphore* semaphore, int count)
{
	
	HANDLE semaIndex = CreateSemaphore(NULL, count, count, NULL);
	if (semaIndex == NULL)
	{
		return ERROR_SEMAPHORE_CREATE_FAILED;
	}

	int osIndex = FindFreeIndex();

	handles[osIndex] = semaIndex;
	handleTypes[osIndex] = SEMAPHORE_HANDLE;

	semaphore->maxCount = count;
	semaphore->osDataHandle = osIndex;

	return 0;
}

int WaitOSSemaphore(OSSemaphore* semaphore, unsigned int waitMS)
{
	HANDLE sema = handles[semaphore->osDataHandle];

	DWORD waitResult = WaitForSingleObject(sema, waitMS);

	int ret = 0;

	switch (waitResult)
	{
	case WAIT_OBJECT_0:
		break;
	case WAIT_TIMEOUT:
		ret = ERROR_SEMAPHORE_TIMEOUT_FAILED;
		break;
	}

	return ret;
}

int NotifyOSSemaphore(OSSemaphore* semaphore)
{
	HANDLE sema = handles[semaphore->osDataHandle];

	if (!ReleaseSemaphore(
		sema,  
		1,       
		NULL))       
	{
		return ERROR_SEMAPHORE_RELEASE_FAILED;
	}

	return 0;
}

int DeleteOSSemaphore(OSSemaphore* semaphore)
{
	HANDLE sema = handles[semaphore->osDataHandle];
	CloseHandle(sema);
	handles[semaphore->osDataHandle] = INVALID_HANDLE_VALUE;
	handleTypes[semaphore->osDataHandle] = -1;
	freeList[semaphore->osDataHandle].store(1, std::memory_order_release);
	memset(semaphore, -1, sizeof(OSSemaphore));
	return 0;
}


int CreateOSSharedExclusive(OSSharedExclusive* osse)
{

	int osIndex = FindFreeIndex();

	InitializeSRWLock((SRWLOCK*)&handles[osIndex]);

	osse->internalOSHandle = osIndex;

	return 0;
}

int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	AcquireSRWLockExclusive((SRWLOCK*)&handles[osIndex]);

	return 0;
}

int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	AcquireSRWLockShared((SRWLOCK*)&handles[osIndex]);

	return 0;
}

int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	ReleaseSRWLockExclusive((SRWLOCK*)&handles[osIndex]);

	return 0;
}

int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	ReleaseSRWLockShared((SRWLOCK*)&handles[osIndex]);

	return 0;
}

int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (!TryAcquireSRWLockExclusive((SRWLOCK*)&handles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return 0;
}

int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{

	int osIndex = osse->internalOSHandle;

	if (!TryAcquireSRWLockShared((SRWLOCK*)&handles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return 0;
}

