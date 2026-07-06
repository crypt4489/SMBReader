#pragma once
#include "StringUtils.h"
#include "IndexTypes.h"

#include <array>
#include <atomic>
#include <type_traits>
#include <utility>

template <typename T_AtomicType>
T_AtomicType UpdateAtomic(std::atomic<T_AtomicType>& atomic, T_AtomicType stride, T_AtomicType wrapAroundSize, T_AtomicType alignment)
{
	T_AtomicType val, desired, out;
	val = atomic.load(std::memory_order_acquire);
	do {
		out = (val + alignment - 1) & ~(alignment - 1);
		
		if (wrapAroundSize && out >= wrapAroundSize)
		{
			out = 0;
		}

		desired = out + stride;

		if (wrapAroundSize && desired >= wrapAroundSize)
		{
			out = 0;
			desired = stride;
		}
	} while (!atomic.compare_exchange_weak(val, desired, std::memory_order_release,
		std::memory_order_relaxed));

	return out;
}

template <typename T_AtomicType>
T_AtomicType UpdateAtomic(std::atomic<T_AtomicType>& atomic, T_AtomicType stride, T_AtomicType wrapAroundSize)
{
	T_AtomicType val, desired, out;
	val = atomic.load(std::memory_order_acquire);
	do {
		out = val;

		if (wrapAroundSize && out >= wrapAroundSize)
		{
			out = 0;
		}

		desired = out + stride;

		if (wrapAroundSize && desired >= wrapAroundSize)
		{
			out = 0;
			desired = stride;
		}
	} while (!atomic.compare_exchange_weak(val, desired, std::memory_order_release,
		std::memory_order_relaxed));

	return out;
}

struct Allocator
{
	void* dataHead;
	int dataSize;
	std::atomic<int> dataAllocator;

	Allocator() = default;

	Allocator(void* _dataHead, int _size)
	{
		dataSize = _size;
		dataAllocator = 0;
		dataHead = _dataHead;
	}

	virtual void* Allocate(int _allocSize, int alignment) = 0;
	virtual void* Allocate(int _allocSize) = 0;
	virtual void Reset() = 0;
	virtual void* Head() = 0;
	virtual void* CAllocate(int _allocSize, int alignment) = 0;
	virtual void* CAllocate(int _allocSize) = 0;

	virtual StringView* AllocateFromNullString(const char* name) = 0;
	virtual StringView AllocateFromNullStringCopy(const char* name) = 0;

	virtual int OffsetInAllocator(void* dataPtr) = 0;

	std::pair<int, int> GetUsageAndCapacity() const;
};

struct RingAllocator : public Allocator
{
	RingAllocator() = default;

	RingAllocator(void* _dataHead, int _size) :
		Allocator(_dataHead, _size)
	{

	}

	void* Allocate(int _allocSize);
	void* Allocate(int _allocSize, int alignment);
	void Reset();
	void* Head();
	void* CAllocate(int _allocSize, int alignment);
	void* CAllocate(int _allocSize);

	StringView* AllocateFromNullString(const char* name);
	StringView AllocateFromNullStringCopy(const char* name);

	int OffsetInAllocator(void* dataPtr);
};


struct SlabAllocator : public Allocator
{
	SlabAllocator() = default;
	SlabAllocator(void* _dataHead, int _size) 
		:
		Allocator(_dataHead, _size)
	{
	}

	void* Allocate(int _allocSize, int alignment);
	void* Allocate(int _allocSize);
	void Reset();
	void* Head();
	void* CAllocate(int _allocSize, int alignment);
	void* CAllocate(int _allocSize);

	StringView* AllocateFromNullString(const char* name);
	StringView AllocateFromNullStringCopy(const char* name);

	int OffsetInAllocator(void* dataPtr);
};

struct DeviceSlabAllocator
{
	DeviceSlabAllocator() = default;
	int dataSize;
	std::atomic<int> dataAllocator;
	constexpr DeviceSlabAllocator(int _size) :
		dataSize(_size), dataAllocator(0)
	{

	}
	int Allocate(int _allocSize, int alignment);
	std::pair<int, int> GetUsageAndCapacity() const;
};

template<typename T>
struct PoolAllocator
{
	T* pool{};
	int* freeList{};
	int freeListTop = -1;
	int count = 0;
	int maxCount = 0;

	PoolAllocator() = default;

	void Create(Allocator* allocator, uint32_t maxElements)
	{
		pool = (T*)allocator->Allocate(sizeof(T) * maxElements, alignof(T));
		freeList = (int*)allocator->Allocate(sizeof(int) * maxElements, alignof(int));
		maxCount = maxElements;
		count = 0;
		freeListTop = -1;
	}

	int Allocate()
	{
		if (freeListTop >= 0)
		{
			return freeList[freeListTop--];
		}

		return count++;
	}

	int Allocate(int N)
	{
		int ret = count;

		count += N;

		return ret;
	}

	void Free(int index)
	{
		freeList[++freeListTop] = index;
	}

	T* Get(int index)
	{
		return &pool[index];
	}

	T operator[](int index)
	{
		return pool[index];
	}
};

struct MessageQueue
{
	std::atomic<uint64_t> write;
	uintptr_t bufferLocation;
	uint64_t bufferSize;

	MessageQueue(void* data, size_t size) :
		write(0), bufferLocation((uintptr_t)data), bufferSize(size)
	{

	}

	bool IsEmpty() const
	{
		return (write != 0);
	}

	void* Head() const
	{
		return (void*)bufferLocation;
	}

	uint64_t BytesUnread() const
	{
		return write.load(std::memory_order_acquire);
	}

	void* AcquireWrite(uint64_t dataSize)
	{
		uint64_t head = UpdateAtomic(write, dataSize, (uint64_t)0);
		if (write >= bufferSize) return nullptr;

		void* ret = (void*)(bufferLocation + head);
		return ret;
	}

	void Read()
	{
		write = 0;
	}
};

struct CircularMessageQueueMPSC
{
	std::atomic<uint64_t> write;
	uint64_t read;
	uintptr_t bufferLocation;
	uint64_t bufferSize;

	CircularMessageQueueMPSC(void* data, size_t size) :
		write(0), 
		read(0), 
		bufferLocation((uintptr_t)data), 
		bufferSize(size)
	{

	}

	void* Head() const
	{
		return (void*)bufferLocation;
	}

	uint64_t BytesUnread() const
	{
		return write.load(std::memory_order_acquire) - read;
	}

	bool IsEmpty() const
	{
		return (BytesUnread() != 0);
	}

	void* AcquireWrite(uint64_t dataSize)
	{
		uint64_t head = UpdateAtomic(write, dataSize, (uint64_t)0);

		void* ret = (void*)(bufferLocation + head);

		return ret;
	}

	void Read(uint64_t dataReadSize)
	{
		read += dataReadSize;
	}

	void* Pop(uint32_t dataSize)
	{
		void* ret = (void*)(bufferLocation + (read & bufferSize-1));

		read += dataSize;

		return ret;
	}
};