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
static std::atomic<int> boundedLinearAllocator;

struct MPMCQueueData
{
	std::atomic<size_t> currentSequence;
	int freeIndex;
};

static MPMCQueueData* freeList;
static std::atomic<size_t> enqueuePos{ 0 };
static std::atomic<size_t> dequeuePos{ 0 };

static int maxFreeListEntry = 0;

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

	handles[index] = NULL;
	handleTypes[index] = 0;
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

void CloseAllSyncObject()
{
	for (int idx = 0; idx < maxFreeListEntry; idx++)
	{
		if (handles[idx] != NULL)
		{
			switch (handleTypes[idx])
			{
			case SEMAPHORE_HANDLE:
				CloseHandle(handles[idx]);
				break;
			case SRWLOCK_HANDLE:
				break;
			}

			handles[idx] = NULL;
			handleTypes[idx] = 0;
		}

		freeList[idx].currentSequence.store(idx, std::memory_order_relaxed);
	}

	enqueuePos.store(0, std::memory_order_relaxed);
	dequeuePos.store(0, std::memory_order_relaxed);
}

OSSyncMemoryRequirements OSGetSyncMemoryRequirements(int maxNumberOfOpenSyncObjects)
{
	int handlesSize = (maxNumberOfOpenSyncObjects) * sizeof(void*);
	int handlesTypeSize = (maxNumberOfOpenSyncObjects) * sizeof(int);
	int freeListSize = (maxNumberOfOpenSyncObjects) * sizeof(MPMCQueueData);

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

	freeList = (MPMCQueueData*)dataHead;

	for (int i = 0; i < handleSize; i++)
	{
		freeList[i].currentSequence.store(i, std::memory_order_relaxed);
		handles[i] = NULL;
		handleTypes[i] = 0;
	}

	maxFreeListEntry = handleSize;

	return OS_SEMAPHORE_SUCCESS;
}

int CreateOSSemaphore(OSSemaphore* semaphore, int count)
{
	int osIndex = FindFreeIndex();

	if (osIndex < 0)
	{
		return OS_SEMAPHORE_HANDLE_EXHAUSTED;
	}

	HANDLE semaIndex = CreateSemaphore(NULL, count, count, NULL);
	
	if (semaIndex == INVALID_HANDLE_VALUE)
	{
		ReturnIndex(osIndex);
		return OS_SEMAPHORE_CREATE_FAILED;
	}

	handles[osIndex] = semaIndex;
	handleTypes[osIndex] = SEMAPHORE_HANDLE;

	semaphore->maxCount = count;
	semaphore->osDataHandle = osIndex;

	return OS_SEMAPHORE_SUCCESS;
}

int WaitOSSemaphore(OSSemaphore* semaphore, unsigned int waitMS)
{
	int osIndex = semaphore->osDataHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	HANDLE sema = handles[semaphore->osDataHandle];

	DWORD waitResult = WaitForSingleObject(sema, waitMS);

	int ret = OS_SEMAPHORE_SUCCESS;

	switch (waitResult)
	{
	case WAIT_OBJECT_0:
		break;
	case WAIT_TIMEOUT:
		ret = OS_SEMAPHORE_TIMEOUT_FAILED;
		break;
	}

	return ret;
}

int NotifyOSSemaphore(OSSemaphore* semaphore)
{
	int osIndex = semaphore->osDataHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	HANDLE sema = handles[semaphore->osDataHandle];

	if (!ReleaseSemaphore(
		sema,  
		1,       
		NULL))       
	{
		return OS_SEMAPHORE_RELEASE_FAILED;
	}

	return OS_SEMAPHORE_SUCCESS;
}

int DeleteOSSemaphore(OSSemaphore* semaphore)
{
	int osIndex = semaphore->osDataHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	HANDLE sema = handles[semaphore->osDataHandle];

	CloseHandle(sema);

	ReturnIndex(semaphore->osDataHandle);

	memset(semaphore, -1, sizeof(OSSemaphore));

	return OS_SEMAPHORE_SUCCESS;
}


int CreateOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = FindFreeIndex();

	if (osIndex < 0)
	{
		return OS_SEMAPHORE_HANDLE_EXHAUSTED;
	}

	InitializeSRWLock((SRWLOCK*)&handles[osIndex]);

	osse->internalOSHandle = osIndex;

	return OSSE_SUCCESS;
}

int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	AcquireSRWLockExclusive((SRWLOCK*)&handles[osIndex]);

	return OSSE_SUCCESS;
}

int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	AcquireSRWLockShared((SRWLOCK*)&handles[osIndex]);

	return OSSE_SUCCESS;
}

int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	ReleaseSRWLockExclusive((SRWLOCK*)&handles[osIndex]);

	return OSSE_SUCCESS;
}

int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	ReleaseSRWLockShared((SRWLOCK*)&handles[osIndex]);

	return OSSE_SUCCESS;
}

int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	if (!TryAcquireSRWLockExclusive((SRWLOCK*)&handles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return OSSE_SUCCESS;
}

int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (osIndex < 0 || osIndex >= maxFreeListEntry)
	{
		return OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS;
	}

	if (!TryAcquireSRWLockShared((SRWLOCK*)&handles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return OSSE_SUCCESS;
}

