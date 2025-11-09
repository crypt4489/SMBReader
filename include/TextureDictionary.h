#pragma once
#include <array>

#include "ThreadManager.h"

struct TextureDictionary
{
	char* textureCache = nullptr;
	size_t textureSize = 0;
	std::atomic<int> allocationIndex = 0;
	std::atomic<size_t> textureAllocator = 0;
	std::array<uintptr_t, 50> textureAllocations{};
	int AllocateTextureData(char* data, size_t size)
	{
		size_t val, desired, out;
		val = textureAllocator.load(std::memory_order_relaxed);
		do {

			desired = val + size;
			out = val;
			if (desired >= textureSize)
			{
				out = 0;
				desired = out + size;
			}
		} while (!textureAllocator.compare_exchange_weak(val, desired, std::memory_order_relaxed,
			std::memory_order_relaxed));

		uintptr_t head = reinterpret_cast<uintptr_t>(textureCache) + out;
	
		int val2 = allocationIndex.fetch_add(1);

		textureAllocations[val2] = head;

		return val2;

	}
};

