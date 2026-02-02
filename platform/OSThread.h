#pragma once
struct OSThreadHandle
{
	unsigned long threadIdentifier;
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
	OS_THREAD_FAILED_CREATE = -1,
	OS_THREAD_FAILED_JOIN = -2,
};

typedef void (*ThreadPointer)(void*);

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags);

int OSCloseThread(OSThreadHandle* handle);

int OSWaitThread(OSThreadHandle* handle, int timeout);