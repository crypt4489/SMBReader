#include "AppAllocator.h"








void* SlabAllocator::Allocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	return (head + out);
}

int DeviceSlabAllocator::Allocate(int _allocSize)
{
	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	return (out);
}

