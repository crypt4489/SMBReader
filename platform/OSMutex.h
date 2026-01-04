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
	ERROR_SEMAPHORE_CREATE_FAILED = -1,
	ERROR_SEMAPHORE_TIMEOUT_FAILED = -2,
	ERROR_SEMAPHORE_RELEASE_FAILED
};

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
	OSSE_ACQUIRE_FAILED = -1,
};

int CreateOSSharedExclusive(OSSharedExclusive* osse);
int ExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse);
int SharedAcquireOSSharedExclusive(OSSharedExclusive* osse);
int ExclusiveReleaseOSSharedExclusive(OSSharedExclusive* osse);
int SharedReleaseOSSharedExclusive(OSSharedExclusive* osse);
int TryExclusiveAcquireOSSharedExclusive(OSSharedExclusive* osse);
int TrySharedAcquireOSSharedExclusive(OSSharedExclusive* osse);