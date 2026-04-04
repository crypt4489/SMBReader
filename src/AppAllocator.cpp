#include "AppAllocator.h"




void* RingAllocator::Allocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize, alignment);

	return (head + out);
}

void* RingAllocator::Allocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize);

	return (head + out);
}

void* RingAllocator::CAllocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize, alignment);

	memset((head + out), 0, _allocSize);

	return (head + out);
}

void* RingAllocator::CAllocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize);

	memset((head + out), 0, _allocSize);

	return (head + out);
}

void RingAllocator::Reset()
{
	dataAllocator = 0;
}

void* RingAllocator::Head()
{
	char* head = (char*)dataHead;
	return (void*)(head + dataAllocator);
}




void* SlabAllocator::Allocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0, alignment);

	return (head + out);
}

void* SlabAllocator::Allocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	return (head + out);
}

void* SlabAllocator::CAllocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize, alignment);

	memset((head + out), 0, _allocSize);

	return (head + out);
}

void* SlabAllocator::CAllocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, dataSize);

	memset((head + out), 0, _allocSize);

	return (head + out);
}

void SlabAllocator::Reset()
{
	dataAllocator = 0;
}

void* SlabAllocator::Head()
{
	char* head = (char*)dataHead;
	return (void*)(head + dataAllocator);
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

