#pragma once
#include "StringUtils.h"
#include "IndexTypes.h"
#include <type_traits>
#include <utility>
#include <atomic>

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


template <typename T_Type>
concept ValueType = (std::is_integral_v<T_Type> || std::is_pointer<T_Type>::value || std::is_same_v<T_Type, EntryHandle>);

template <typename T_Type>
concept ClassType = !(std::is_integral_v<T_Type> || std::is_pointer<T_Type>::value || std::is_same_v<T_Type, EntryHandle>);

template <typename T_Type, int T_Count>
struct ArrayAllocator
{
	std::array<T_Type, T_Count> dataArray;
	std::atomic<int> allocatorPtr;
	
	ArrayAllocator()
		: allocatorPtr(0)
	{

	}

	std::pair<int, T_Type*> Allocate() requires ClassType<T_Type>
	{
		int retI = UpdateAtomic(allocatorPtr, 1, 0);

		if (retI >= T_Count)
		{
			return { -1, nullptr };
		}

		T_Type* ret = &dataArray[retI];
		return { retI, ret };
	}

	
	int Allocate(T_Type insert) requires ValueType<T_Type>
	{
		int ret = UpdateAtomic(allocatorPtr, 1, 0);
		
		if (ret >= T_Count)
		{
			return { -1 };
		}

		dataArray[ret] = insert;
		return ret;
	}

	int AllocateN(int num)
	{
		
		int ret = UpdateAtomic(allocatorPtr, num, 0);

		if (ret + num >= T_Count)
		{
			return { -1 };
		}

		return ret;
	}
	
	void Update(int index, T_Type insert) requires ValueType<T_Type>
	{
		dataArray[index] = insert;
	}

	T_Type* Update(int index) requires ClassType<T_Type>
	{
		return &dataArray[index];
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
		uint64_t head = UpdateAtomic(write, dataSize, 0ull);
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
		uint64_t head = UpdateAtomic(write, dataSize, 0ull);

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