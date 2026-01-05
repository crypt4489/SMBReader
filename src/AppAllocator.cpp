#include "AppAllocator.h"








void* SlabAllocator::Allocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	return (head + out);
}

int DeviceSlabAllocator::Allocate(int _allocSize, int alignment)
{



	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	if (out & alignment-1) {
		int alignUp = (alignment - (alignment - 1 & out));
		out = (out + alignUp);
		UpdateAtomic(dataAllocator, alignUp, 0);
	}
		

	return (out);
}

