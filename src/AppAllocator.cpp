#include "AppAllocator.h"


std::pair<int, int> Allocator::GetUsageAndCapacity() const {
	return { dataAllocator.load(), dataSize };
}

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

StringView* RingAllocator::AllocateFromNullString(const char* name)
{
	StringView* view = (StringView*)Allocate(sizeof(StringView));

	int strLen = strnlen(name, 250);

	view->stringData = (char*)Allocate(strLen);
	view->charCount = strLen;

	memcpy(view->stringData, name, strLen);

	return view;
}

StringView RingAllocator::AllocateFromNullStringCopy(const char* name)
{
	StringView view;

	int strLen = strnlen(name, 250);

	view.stringData = (char*)Allocate(strLen);
	view.charCount = strLen;

	memcpy(view.stringData, name, strLen);

	return view;
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

StringView* SlabAllocator::AllocateFromNullString(const char* name)
{
	StringView* view = (StringView*)Allocate(sizeof(StringView));

	int strLen = strnlen(name, 250);

	view->stringData = (char*)Allocate(strLen);
	view->charCount = strLen;

	memcpy(view->stringData, name, strLen);

	return view;
}

StringView SlabAllocator::AllocateFromNullStringCopy(const char* name)
{
	StringView view;

	int strLen = strnlen(name, 250);

	view.stringData = (char*)Allocate(strLen);
	view.charCount = strLen;

	memcpy(view.stringData, name, strLen);

	return view;
}

int DeviceSlabAllocator::Allocate(int _allocSize, int alignment)
{
	int out = UpdateAtomic(dataAllocator, _allocSize, 0, alignment);

	return (out);
}

std::pair<int, int> DeviceSlabAllocator::GetUsageAndCapacity() const {
	return { dataAllocator.load(), dataSize };
}

