#pragma once
#include <array>

#include "ThreadManager.h"

struct TextureDictionary
{
	char* textureCache;
	size_t textureSize;
	std::atomic<size_t> textureAllocator;
	std::array<uintptr_t, 50> textureAllocations;
	int AllocateTextureData(char* data, size_t size);
};

