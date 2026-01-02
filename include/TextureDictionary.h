#pragma once
#include <array>

#include "AppAllocator.h"
#include "AppTypes.h"

#include "IndexTypes.h"
#include "ThreadManager.h"

// manage central pool of image data;
// upload to different buffers of by types
// maintian upload handles with render instance



struct TextureAllocation
{
	uintptr_t cacheAddress;
	size_t size;
	int pool;
	int width;
	int height;
	int miplevels;
};

struct TextureDictionary
{
	uintptr_t textureCache = 0;
	size_t textureSize = 0;
	std::atomic<int> allocationIndex = 0;
	std::atomic<size_t> textureAllocator = 0;
	

	std::array<TextureAllocation, 50> textureAllocations{};
	std::array<EntryHandle, 50> textureHandles{};

	std::array<EntryHandle, 10> deviceImageBuffers{};
	std::array<EntryHandle, 10> texturePoolHandle{};
	std::array<ImageFormat, 10> texturePoolsFormat{};
	std::array<size_t, 10> texturePoolsSize{};
	std::array<size_t, 10> texturePoolsAllocatedSize{};

	int AllocateTextureData(char* data, size_t size, ImageFormat format, int width, int height, int mips);

	void UpdateTextureData(int index, char* data, size_t size, ImageFormat format, int width, int height, int mips);

	int AllocateNTextureHandles(int n);

	int FindPoolByFormat(ImageFormat format);

	void CreatePools(ImageFormat* formats, size_t* sizes, EntryHandle* poolHandles, EntryHandle* bufferHandles, int num);
};

