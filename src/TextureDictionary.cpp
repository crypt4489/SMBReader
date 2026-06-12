#include "TextureDictionary.h"

void* TextureDictionary::AllocateImageCache(size_t size)
{
	size_t out = UpdateAtomic(textureAllocator, size, textureSize);

	return (void*)(textureCache + out);
}

int TextureDictionary::AllocateNTextureHandles(int n, TextureDetails** details)
{
	int outIndex = allocationIndex.fetch_add(n);

	if (details)
	{
		for (int i = 0; i < n; i++)
		{
			details[i] = &textureAllocations[outIndex + i];
		}
	}

	return outIndex;
}

int TextureDictionary::FindPoolByFormat(ImageFormat format)
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

size_t TextureDictionary::AllocFromPoolAllocator(int poolIndex, size_t requestedImageSize, size_t alignment)
{
	return texturePoolAllocators[poolIndex].Allocate(requestedImageSize, alignment);
}

void TextureDictionary::CreatePoolAllocator(int poolIndex, size_t totalBlobSize)
{
	texturePoolAllocators[poolIndex].dataSize = totalBlobSize;
	texturePoolAllocators[poolIndex].dataAllocator = 0;
}

std::pair<int, int> TextureDictionary::GetCacheUsageAndCapacity() const
{
	return { (int)textureAllocator.load(), (int)textureSize };
}