#pragma once
#pragma once
struct OSThreadHandle
{
	int osDataHandle;

	OSThreadHandle()
	{
		osDataHandle = -1;
	}
};

enum OSThreadFlags
{
	
};



enum OSThreadErrorCodes
{
	OS_THREAD_FAILED_CREATE = -1,
	OS_THREAD_FAILED_JOIN = -2,
};