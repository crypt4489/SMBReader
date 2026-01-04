#include "OSMutex.h"
#include <Windows.h>
#include <atomic>

static HANDLE semaphoreHandles[50];
//static CONDITION_VARIABLE cvHandles[50];
static std::atomic<int> semaphoreHandleIndex = 0;

static SRWLOCK slimLockHandles[50];
static std::atomic<int> slimLockIndex = 0;

int CreateOSSemaphore(OSSemaphore* semaphore, int count)
{
	
	HANDLE semaIndex = CreateSemaphore(NULL, count, count, NULL);
	if (semaIndex == NULL)
	{
		return ERROR_SEMAPHORE_CREATE_FAILED;
	}

	int osIndex = semaphoreHandleIndex.fetch_add(1);

	semaphoreHandles[osIndex] = semaIndex;

	semaphore->maxCount = count;
	semaphore->osDataHandle = osIndex;

	return 0;
}

int WaitOSSemaphore(OSSemaphore* semaphore, unsigned int waitMS)
{

	HANDLE sema = semaphoreHandles[semaphore->osDataHandle];

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
	HANDLE sema = semaphoreHandles[semaphore->osDataHandle];

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
	HANDLE sema = semaphoreHandles[semaphore->osDataHandle];
	CloseHandle(sema);
	semaphoreHandles[semaphore->osDataHandle] = INVALID_HANDLE_VALUE;
	memset(semaphore, -1, sizeof(OSSemaphore));

	return 0;
}


int CreateOSSharedExclusive(OSSharedExclusive* osse)
{

	int osIndex = slimLockIndex.fetch_add(1);

	InitializeSRWLock(&slimLockHandles[osIndex]);

	osse->internalOSHandle = osIndex;

	return 0;
}

int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	AcquireSRWLockExclusive(&slimLockHandles[osIndex]);

	return 0;
}

int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	AcquireSRWLockShared(&slimLockHandles[osIndex]);

	return 0;
}

int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	ReleaseSRWLockExclusive(&slimLockHandles[osIndex]);

	return 0;
}

int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	ReleaseSRWLockShared(&slimLockHandles[osIndex]);

	return 0;
}

int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse)
{
	int osIndex = osse->internalOSHandle;

	if (!TryAcquireSRWLockExclusive(&slimLockHandles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return 0;
}

int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse)
{

	int osIndex = osse->internalOSHandle;

	if (!TryAcquireSRWLockShared(&slimLockHandles[osIndex]))
	{
		return OSSE_ACQUIRE_FAILED;
	}

	return 0;
}

