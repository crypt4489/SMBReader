#pragma once

#include "OS.h"

struct OSThreadHandle
{
	uint32_t threadIdentifier;
	int osDataHandle;

	OSThreadHandle()
	{
		osDataHandle = -1;
		threadIdentifier = -1;
	}
};

enum OSThreadFlags
{
	OS_THREAD_NONE = 0,
	OS_THREAD_ASYNC = 1
};

enum OSThreadErrorCodes
{
	OS_THREAD_SUCCESS = 0,
	OS_THREAD_HANDLE_EXHAUSTED = -1,
	OS_THREAD_HANDLE_OUT_OF_BOUNDS = -2,
	OS_THREAD_FAILED_CREATE = -3,
	OS_THREAD_FAILED_JOIN = -4,
	OS_THREAD_FAILED_CLOSE = -5,
	OS_THREAD_FAILED_TIMEOUT = -6
};

typedef void (*ThreadPointer)(void*);

struct OSThreadMemoryRequirements
{
	int dataSize;
	int alignment;
};

OSThreadMemoryRequirements OSGetThreadMemoryRequirements(int maxNumberOfOpenThreads);

void CloseAllThreads();

int OSSeedThreadMemory(void* dataSource, int dataSize, int numberOfOpenThreads);

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags);

int OSCloseThread(OSThreadHandle* handle);

int OSWaitThread(OSThreadHandle* handle, int timeout);

int OSJoinThread(OSThreadHandle* handle);