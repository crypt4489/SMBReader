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

void TextureDictionary::CreatePools(ImageFormat* formats, size_t* sizes, EntryHandle* poolHandles, EntryHandle* bufferHandles, int num)
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

std::pair<int, int> TextureDictionary::GetCacheUsageAndCapacity() const
{
	return { (int)textureAllocator.load(), (int)textureSize };
}