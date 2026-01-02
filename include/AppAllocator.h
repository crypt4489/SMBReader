#pragma once
#include "IndexTypes.h"
#include <type_traits>
#include <utility>
#include <atomic>

template <typename T_AtomicType>
T_AtomicType UpdateAtomic(std::atomic<T_AtomicType>& atomic, T_AtomicType stride, T_AtomicType wrapAroundSize)
{
	T_AtomicType val, desired, out;
	val = atomic.load(std::memory_order_relaxed);
	do {

		desired = val + stride;
		out = val;
		if (wrapAroundSize && desired >= wrapAroundSize)
		{
			out = 0;
			desired = out + stride;
		}
	} while (!atomic.compare_exchange_weak(val, desired, std::memory_order_relaxed,
		std::memory_order_relaxed));

	return out;
}


struct SlabAllocator
{
	void* dataHead;
	int dataSize;
	std::atomic<int> dataAllocator;
	constexpr SlabAllocator(void* _dataHead, int _size) :
		dataSize(_size), dataAllocator(0), dataHead(_dataHead)
	{

	}
	void* Allocate(int _allocSize);
};

struct DeviceSlabAllocator
{
	int dataSize;
	std::atomic<int> dataAllocator;
	constexpr DeviceSlabAllocator(int _size) :
		dataSize(_size), dataAllocator(0)
	{

	}
	int Allocate(int _allocSize);
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
		T_Type* ret = &dataArray[retI];
		return { retI, ret };
	}

	
	int Allocate(T_Type insert) requires ValueType<T_Type>
	{
		int ret = UpdateAtomic(allocatorPtr, 1, 0);
		dataArray[ret] = insert;
		return ret;
	}

	int AllocateN(int num)
	{
		int ret = UpdateAtomic(allocatorPtr, num, 0);
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

