#include "OSTime.h"

#include <Windows.h>

LARGE_INTEGER systemFrequency;
bool initialize = false;

void OSTimeInitialize()
{
	initialize = QueryPerformanceFrequency(&systemFrequency);
}

int64_t OSGetSystemTicks()
{
	if (!initialize) return 0;

	LARGE_INTEGER currentTickCount;

	QueryPerformanceCounter(&currentTickCount);

	return currentTickCount.QuadPart;
}

double OSGetTimeSeconds(int64_t tickCounter)
{
	if (!initialize) return 0.0;

	return static_cast<double>(tickCounter / systemFrequency.QuadPart);
}

double OSGetCurrentTimeSeconds()
{
	if (!initialize) return 0.0;

	LARGE_INTEGER currentTickCount;

	QueryPerformanceCounter(&currentTickCount);

	return static_cast<double>(currentTickCount.QuadPart / systemFrequency.QuadPart);
}
