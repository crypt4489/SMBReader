#pragma once

#include <array>

#include "allocator/AppAllocator.h"
#include "imageutils/TextureIO.h"
#include "IndexTypes.h"
#include "ThreadManager.h"

struct TextureDictionary
{
	uintptr_t textureCache = 0;
	size_t textureSize = 0;
	std::atomic<int> allocationIndex = 0;
	std::atomic<size_t> textureAllocator = 0;
	
	std::array<TextureDetails, 50> textureAllocations{};
	std::array<int, 50> textureHandles{};

	std::array<int, 10> texturePoolHandle{};
	std::array<ImageFormat, 10> texturePoolsFormat{};
	std::array<DeviceSlabAllocator, 10> texturePoolAllocators{};

	void* AllocateImageCache(size_t size);

	int AllocateNTextureHandles(int n, TextureDetails** details);

	int FindPoolByFormat(ImageFormat format);

	size_t AllocFromPoolAllocator(int poolIndex, size_t requestedImageSize, size_t alignment);

	void CreatePoolAllocator(int poolIndex, size_t totalBlobSize);

	std::pair<int, int> GetCacheUsageAndCapacity() const;

};

