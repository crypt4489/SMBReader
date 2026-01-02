#include "TextureDictionary.h"
int TextureDictionary::AllocateTextureData(char* data, size_t size, ImageFormat format, int width, int height, int mips)
{
	size_t out = UpdateAtomic(textureAllocator, size, textureSize);

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

void TextureDictionary::UpdateTextureData(int index, char* data, size_t size, ImageFormat format, int width, int height, int mips)
{
	size_t out = UpdateAtomic(textureAllocator, size, textureSize);

	uintptr_t head = textureCache + out;

	int memPoolAllocate = FindPoolByFormat(format);

	textureAllocations[index].cacheAddress = head;
	textureAllocations[index].size = size;
	textureAllocations[index].pool = memPoolAllocate;
	textureAllocations[index].width = width;
	textureAllocations[index].height = height;
	textureAllocations[index].miplevels = mips;

	memcpy((void*)head, data, size);
}

int TextureDictionary::AllocateNTextureHandles(int n)
{
	int outIndex = allocationIndex.fetch_add(n);
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