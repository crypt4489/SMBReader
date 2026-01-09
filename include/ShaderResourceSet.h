#pragma once

#include <array>
#include <atomic>
#include <functional>

#include "FileManager.h"
#include "ResourceDependencies.h"


struct ShaderResourceSet
{
	int bindingCount;
	int layoutHandle;
	int setCount;
	int barrierCount;
};

struct ShaderResourceHeader
{
	ShaderResourceType type;
	ShaderResourceAction action;
	int binding;
	uint32_t arrayCount;
};

struct ShaderResourceImage : public ShaderResourceHeader
{
	EntryHandle textureHandle;
};

struct ShaderResourceSamplerBindless : public ShaderResourceHeader
{
	EntryHandle *textureHandles;
	uint32_t textureCount;
};

struct ShaderResourceBuffer : public ShaderResourceHeader
{
	int allocation;
	int offset;
};

struct ShaderResourceBufferView : public ShaderResourceHeader
{
	EntryHandle bufferViewHandle;
	uint32_t subAllocations;
	int allocationIndex;
};


struct ShaderResourceConstantBuffer : public ShaderResourceHeader
{
	ShaderStageType stage;
	int size;
	int offset;
	void* data;
};

struct ShaderResourceBarrier
{
	MemoryBarrierType type;
	BarrierStage srcStage;
	BarrierStage dstStage;
	BarrierAction srcAction;
	BarrierAction dstAction;
};

struct ImageShaderResourceBarrier : public ShaderResourceBarrier
{
	VkImageLayout srcResourceLayout;
	VkImageLayout dstResourceLayout;
	VkImageAspectFlags imageType;
};

struct ShaderResourceBufferBarrier : public ShaderResourceBarrier
{
};

template <int N>
struct ShaderResourceManager
{
	EntryHandle deviceResourceHeap;
	uintptr_t hostResourceHeap;
	std::atomic<uintptr_t> hostResourceHead;
	std::array<uintptr_t, N> descriptorSets;
	std::array<EntryHandle, N> vkDescriptorSets;
	std::atomic<int> descriptorSetIndex;

	int AddShaderToSets(uintptr_t location, size_t size)
	{
		int indexRet = descriptorSetIndex.fetch_add(1);

		descriptorSets[indexRet] = location;
		hostResourceHead.store(hostResourceHead + size);

		return indexRet;
	}

	void BindBufferToShaderResource(int descriptorSet, int allocationIndex, int bindingIndex, int offset)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBuffer* header = (ShaderResourceBuffer*)offsets[bindingIndex];

		if (header->type != UNIFORM_BUFFER && header->type != STORAGE_BUFFER)
			return;

		header->allocation = allocationIndex;
		header->offset = offset;
	}

	void BindSampledImageToShaderResource(int descriptorSet, EntryHandle index, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != SAMPLER && header->type != IMAGESTORE2D)
			return;

		header->textureHandle = index;
	}

	void BindSampledImageArrayToShaderResource(int descriptorSet, EntryHandle *indices, uint32_t texCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceSamplerBindless* header = (ShaderResourceSamplerBindless*)offsets[bindingIndex];

		if (header->type != SAMPLERBINDLESS)
			return;

		header->textureHandles = indices;
		header->textureCount = texCount;
	}

	void BindBufferView(int descriptorSet, int allocationIndex, EntryHandle bufferViewHandle, int bindingIndex, int subAllocations)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBufferView* header = (ShaderResourceBufferView*)offsets[bindingIndex];

		if (header->type != BUFFER_VIEW)
			return;

		header->bufferViewHandle = bufferViewHandle;
		header->subAllocations = subAllocations;
		header->allocationIndex = allocationIndex;
	}

	void BindBarrier(int descriptorSet, int binding, BarrierStage stage, BarrierAction action)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];



		switch (desc->type)
		{
		case IMAGESTORE2D:
		case SAMPLER:
		{
			head += sizeof(ShaderResourceImage);
			ShaderResourceBarrier* barrier = (ShaderResourceBarrier*)head;

			barrier->dstAction = action;
			barrier->dstStage = stage;
			break;
		}
		case BUFFER_VIEW:
		{
			head += sizeof(ShaderResourceBufferView);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:
		{

			head += sizeof(ShaderResourceBuffer);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		}

		
	}

	void BindImageBarrier(int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, VkImageLayout oldLayout, VkImageLayout dstLayout, bool location)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];

		if (desc->type != SAMPLER && desc->type != IMAGESTORE2D)
			return;

		switch (desc->type)
		{
		case IMAGESTORE2D:
		case SAMPLER:
			head += sizeof(ShaderResourceImage);

			break;
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:

			head += sizeof(ShaderResourceBuffer);
			break;
		}


		ImageShaderResourceBarrier* imageBarrier = (ImageShaderResourceBarrier*)head;


		imageBarrier[barrierIndex].imageType = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier[barrierIndex].dstResourceLayout = dstLayout;
		imageBarrier[barrierIndex].srcResourceLayout = oldLayout;

		if (location)
		{
			imageBarrier[barrierIndex].dstAction = action;
			imageBarrier[barrierIndex].dstStage = stage;
		}
		else {
			imageBarrier[barrierIndex].srcAction = action;
			imageBarrier[barrierIndex].srcStage = stage;
		}
	}

	void UploadConstant(int descriptorset, void* data, int bufferLocation)
	{
		ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(descriptorset, bufferLocation);
		if (!header) return;
		header->data = data;
	}

	ShaderResourceHeader* GetConstantBuffer(int descriptorSet, int constantBuffer)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[set->bindingCount - (constantBuffer + 1)]);

		if (ret->type != CONSTANT_BUFFER) return nullptr;

		return ret;
	}

	int GetConstantBufferCount(int descriptorSet)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		int iter = set->bindingCount - 1;

		int count = 0;

		while (iter >= 0)
		{
			ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[iter--]);
			if (ret->type & CONSTANT_BUFFER) count++;
			else break;
		}

		return count;
	}

	int GetBarrierCount(int descriptorSet)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		return set->barrierCount;
	}

};

struct ShaderComputeLayout
{
	unsigned long x;
	unsigned long y;
	unsigned long z;
};

struct ShaderDetails
{
	int shaderNameSize;
	int shaderDataSize;

	ShaderDetails* GetNext();

	char* GetString();

	void* GetShaderData();
};

#define KB 1024
#define MB 1024 * KB
#define GB 1024 * MB

struct ShaderGraphReader
{
	static char readerMemBuffer[16 * MB];
	static int readerMemBufferAllocate;


	struct ShaderXMLTag
	{
		unsigned long hashCode;
	};

	struct ShaderGLSLShaderXMLTag : ShaderXMLTag //followed by shaderNameLen Bytes
	{
		ShaderStageType type;
	}; 

	struct ShaderComputeLayoutXMLTag : ShaderXMLTag
	{
		ShaderComputeLayout comps;
	};

	struct ShaderResourceItemXMLTag : ShaderXMLTag
	{
		ShaderStageType shaderstage;
		ShaderResourceType resourceType;
		ShaderResourceAction resourceAction;
		int arrayCount;
	};

	struct ShaderResourceSetXMLTag : ShaderXMLTag
	{
		int resourceCount;
	};

	static constexpr unsigned long
		hash(char* str);

	static constexpr unsigned long
		hash(const std::string& string);


	static ShaderGraph* CreateShaderGraph(const std::string& filename, uintptr_t graphmemory, size_t* outSize, void* shaderDataOut, int* shaderDataSize, int* shaderDetailCount);

	static int ProcessTag(std::vector<char>& fileData, int currentLocation, unsigned long* hash, bool* opening);

	static int SkipLine(std::vector<char>& fileData, int currentLocation);
	static int ReadValue(std::vector<char>& fileData, int currentLocation, char* str, int* len);

	static int ReadAttributeName(std::vector<char>& fileData, int currentLocation, unsigned long *hash);

	static int ReadAttributeValueHash(std::vector<char>& fileData, int currentLocation, unsigned long *hash);

	static int ReadAttributeValueVal(std::vector<char>& fileData, int currentLocation, unsigned long* val);

	static int ReadAttributes(std::vector<char>& fileData, int currentLocation, unsigned long* hashes, int* stackSize, int valType);

	static int HandleGLSLShader(std::vector<char>& fileData, int currentLocation, uintptr_t* offset, void* shaderData, int* shaderDataSize);

	static int HandleShaderResourceItem(std::vector<char>& fileData, int currentLocation, uintptr_t* offset);

	static constexpr int ASCIIToInt(char* str);

	static int HandleComputeLayout(std::vector<char>& fileData, int currentLocation, uintptr_t* offset);
};

