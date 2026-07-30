#pragma once

#define OS_INFINITE_TIMEOUT 0xffffffff

struct OSSemaphore
{
	int maxCount;
	int osDataHandle;

	OSSemaphore()
	{
		maxCount = -1;
		osDataHandle = -1;
	}
};

enum OSSemaphoreErrorCodes
{
	OS_SEMAPHORE_SUCCESS = 0,
	OS_SEMAPHORE_HANDLE_EXHAUSTED = -1,
	OS_SEMAPHORE_HANDLE_OUT_OF_BOUNDS = -2,
	OS_SEMAPHORE_CREATE_FAILED = -3,
	OS_SEMAPHORE_TIMEOUT_FAILED = -4,
	OS_SEMAPHORE_RELEASE_FAILED = -5,
	
};

struct OSSyncMemoryRequirements
{
	int dataSize;
	int alignment;
};

OSSyncMemoryRequirements OSGetSyncMemoryRequirements(int maxNumberOfOpenSyncObjects);

void CloseAllSyncObject();

int OSSeedSyncMemory(void* dataSource, int dataSize, int maxNumberSyncObjects);

int CreateOSSemaphore(OSSemaphore* semaphore, int count);

int WaitOSSemaphore(OSSemaphore* semaphore, unsigned int waitMS);

int NotifyOSSemaphore(OSSemaphore* semaphore);

int DeleteOSSemaphore(OSSemaphore* semaphore);


struct OSSharedExclusive
{
	int internalOSHandle;
};

enum OSSharedExclusiveError
{
	OSSE_SUCCESS = 0,
	OSSE_ACQUIRE_FAILED = -1,
};

int CreateOSSharedExclusive(OSSharedExclusive* osse);
int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse);
int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse);
int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse);
int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse);
int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse);
int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse);
void DeleteOSSharedExclusive(OSSharedExclusive* osse);