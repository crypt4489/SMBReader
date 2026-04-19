#pragma once
#include <array>

#include "AppAllocator.h"
#include "AppTypes.h"

#include "TextureIO.h"
#include "IndexTypes.h"
#include "ThreadManager.h"

// manage central pool of image data;
// upload to different buffers of by types
// maintian upload handles with render instance



struct TextureAllocation
{
	size_t size;
	int pool;
	int width;
	int height;
	int miplevels;
	int layers;
};

struct TextureDictionary
{
	uintptr_t textureCache = 0;
	size_t textureSize = 0;
	std::atomic<int> allocationIndex = 0;
	std::atomic<size_t> textureAllocator = 0;
	

	std::array<TextureDetails, 50> textureAllocations{};
	std::array<EntryHandle, 50> textureHandles{};

	std::array<EntryHandle, 10> deviceImageBuffers{};
	std::array<int, 10> texturePoolHandle{};
	std::array<ImageFormat, 10> texturePoolsFormat{};
	std::array<size_t, 10> texturePoolsSize{};
	std::array<size_t, 10> texturePoolsAllocatedSize{};

	void* AllocateImageCache(size_t size);

	int AllocateNTextureHandles(int n, TextureDetails** details);

	int FindPoolByFormat(ImageFormat format);

	void CreatePools(ImageFormat* formats, size_t* sizes, EntryHandle* poolHandles, EntryHandle* bufferHandles, int num);

	std::pair<int, int> GetCacheUsageAndCapacity() const;

};

