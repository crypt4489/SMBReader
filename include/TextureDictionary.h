#pragma once
#include <array>

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

	int AllocateTextureData(char* data, size_t size, ImageFormat format, int width, int height, int mips)
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

		uintptr_t head = textureCache + out;
	
		int outIndex = allocationIndex.fetch_add(1);

		int memPoolAllocate = FindPoolByFormat(format);

		textureAllocations[outIndex].cacheAddress = head;
		textureAllocations[outIndex].size = size;
		textureAllocations[outIndex].pool = memPoolAllocate;
		textureAllocations[outIndex].width = width;
		textureAllocations[outIndex].height = height;
		textureAllocations[outIndex].miplevels = mips;

		memcpy((void*)head, data, size);

		return outIndex;

	}

	int FindPoolByFormat(ImageFormat format)
	{
		for (int i = 0; i < texturePoolsFormat.size(); i++)
		{
			if (texturePoolsFormat[i] == format)
			{
				return i;
			}
		}

		return -1;
	}

	void CreatePools(ImageFormat* formats, size_t* sizes, EntryHandle* poolHandles, EntryHandle* bufferHandles, int num)
	{
		if (num >= texturePoolHandle.size())
			return;

		for (int i = 0; i < num; i++)
		{
		
			texturePoolsSize[i] = sizes[i];
			texturePoolsAllocatedSize[i] = 0;
			texturePoolsFormat[i] = formats[i];
			texturePoolHandle[i] = poolHandles[i];
			deviceImageBuffers[i] = bufferHandles[i];
		}
	}
};

