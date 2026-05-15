#include "allocator/AppAllocator.h"


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

	char* strBuf = (char*)Allocate(strLen);
	view->charCount = strLen;

	memcpy(strBuf, name, strLen);

	view->stringData = strBuf;

	return view;
}

StringView RingAllocator::AllocateFromNullStringCopy(const char* name)
{
	StringView view;

	int strLen = strnlen(name, 250);

	char* strBuf = (char*)Allocate(strLen);
	
	view.charCount = strLen;

	memcpy(strBuf, name, strLen);

	view.stringData = strBuf;

	return view;
}

int RingAllocator::OffsetInAllocator(void* dataPtr)
{
	uintptr_t dataPtrInT = (uintptr_t)dataPtr;
	uintptr_t dataHeadInT = (uintptr_t)dataHead;

	if (dataPtrInT < dataHeadInT || dataPtrInT >= (dataHeadInT + dataSize))
	{
		return -1;
	}

	return (int)(dataPtrInT - dataHeadInT);
}

void* SlabAllocator::Allocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0, alignment);

	if ((out + _allocSize) > dataSize)
	{
		return nullptr;
	}

	return (head + out);
}

void* SlabAllocator::Allocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	if ((out + _allocSize) > dataSize)
	{
		return nullptr;
	}

	return (head + out);
}

void* SlabAllocator::CAllocate(int _allocSize, int alignment)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0, alignment);

	if ((out + _allocSize) > dataSize)
	{
		return nullptr;
	}

	memset((head + out), 0, _allocSize);

	return (head + out);
}

void* SlabAllocator::CAllocate(int _allocSize)
{
	char* head = (char*)dataHead;

	int out = UpdateAtomic(dataAllocator, _allocSize, 0);

	if ((out + _allocSize) > dataSize)
	{
		return nullptr;
	}

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

	if (!view)
	{
		return view;
	}

	int strLen = strnlen(name, 250);
	
	char* strBuf = (char*)Allocate(strLen);

	if (!strBuf)
	{
		return nullptr;
	}

	view->charCount = strLen;

	memcpy(strBuf, name, strLen);

	view->stringData = strBuf;

	return view;
}

StringView SlabAllocator::AllocateFromNullStringCopy(const char* name)
{
	StringView view{};

	int strLen = strnlen(name, 250);

	char* strBuf = (char*)Allocate(strLen);

	if (!strBuf)
	{
		return view;
	}

	view.charCount = strLen;

	memcpy(strBuf, name, strLen);

	view.stringData = strBuf;

	return view;
}

int SlabAllocator::OffsetInAllocator(void* dataPtr)
{
	uintptr_t dataPtrInT = (uintptr_t)dataPtr;
	uintptr_t dataHeadInT = (uintptr_t)dataHead;

	if (dataPtrInT < dataHeadInT || dataPtrInT >= (dataHeadInT + dataSize))
	{
		return -1;
	}

	return (int)(dataPtrInT - dataHeadInT);
}

int DeviceSlabAllocator::Allocate(int _allocSize, int alignment)
{
	int out = UpdateAtomic(dataAllocator, _allocSize, 0, alignment);

	if ((out + _allocSize) > dataSize)
	{
		return -1;
	}

	return (out);
}

std::pair<int, int> DeviceSlabAllocator::GetUsageAndCapacity() const {
	return { dataAllocator.load(), dataSize };
}

