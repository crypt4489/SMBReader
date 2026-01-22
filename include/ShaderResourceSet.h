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

#include "AppAllocator.h"

struct ShaderResourceUpdate
{
	ShaderResourceType type;
	int descriptorSet;
	int bindingIndex;
	int copyCount;
	void* data;
	int dataSize;
};

struct ShaderResourceUpdateLink
{
	int shaderResourceIndex;
	ShaderResourceUpdateLink* next;
};

struct BindlessSamplerUpdate
{
	uint32_t begdestinationslot;
	uint32_t samplercount;
	EntryHandle* handles;
};

template <int T_MaxRegionCopy>
struct ShaderResourceUpdatePool
{
	char stagingbuffer[32 * 1024];
	std::array<ShaderResourceUpdate, 1000> updateRegions;
	std::array<ShaderResourceUpdateLink, 1000> updateLinks;
	ShaderResourceUpdateLink* linkHead = nullptr;
	ShaderResourceUpdateLink** popPrev = nullptr;
	std::atomic<int> linkCount = 0;
	std::atomic<int> updateRegionAlloc = 0;
	std::atomic<int> currentStagingBufferWrite = 0;

	int Create(int descriptorid, int bindingindex, ShaderResourceType type, void* data)
	{
		ShaderResourceUpdateLink* link = Find(descriptorid, bindingindex);
		ShaderResourceUpdate* region = nullptr;

		int size = 0;

		switch (type)
		{
		case SAMPLERBINDLESS:
		{
			BindlessSamplerUpdate* update = (BindlessSamplerUpdate*)data;
			size = (sizeof(EntryHandle) * update->samplercount) + sizeof(BindlessSamplerUpdate);
			break;
		}
		}


		if (!link)
		{
			int regionAlloc = UpdateAtomic(updateRegionAlloc, 1, (int)updateRegions.size());

			region = &updateRegions[regionAlloc];

			link = &updateLinks[regionAlloc];

			

			
			int writeLoc = UpdateAtomic(currentStagingBufferWrite, size, (int)sizeof(stagingbuffer));

			switch (type)
			{
			case SAMPLERBINDLESS:
			{
				BindlessSamplerUpdate* update = (BindlessSamplerUpdate*)data;
				BindlessSamplerUpdate* cachedUpdate = (BindlessSamplerUpdate*)(stagingbuffer + writeLoc);
				cachedUpdate->begdestinationslot = update->begdestinationslot;
				cachedUpdate->samplercount = update->samplercount;
				cachedUpdate->handles = (EntryHandle*)(cachedUpdate + 1);
				memcpy(cachedUpdate->handles, update->handles, sizeof(EntryHandle) * cachedUpdate->samplercount);
				break;
			}
			}
			
			region->data = stagingbuffer + writeLoc;
			region->dataSize = size;
			region->descriptorSet = descriptorid;

			region->bindingIndex = bindingindex;

			link->shaderResourceIndex = regionAlloc;
			link->next = nullptr;



			Insert(link);




		}
		else
		{
			region = &updateRegions[link->shaderResourceIndex];

			if (region->type != type)
			{
				return -1;
			}
			
			if (size > region->dataSize)
			{
				int writeLoc = UpdateAtomic(currentStagingBufferWrite, size, (int)sizeof(stagingbuffer));
				region->data = stagingbuffer + writeLoc;
			}

			memcpy(region->data, data, size);
			
			

			region->dataSize = size;
		}

		region->type = type;
		region->copyCount = T_MaxRegionCopy;

		return 0;
	}

	void Insert(ShaderResourceUpdateLink* newlink)
	{
		int newid = updateRegions[newlink->shaderResourceIndex].descriptorSet;
		int newbindingindex = updateRegions[newlink->shaderResourceIndex].bindingIndex;
		ShaderResourceUpdateLink** test = &linkHead;
		while (*test && (updateRegions[(*test)->shaderResourceIndex].descriptorSet <= newid))
		{
			if ((updateRegions[(*test)->shaderResourceIndex].bindingIndex < newbindingindex))
				break;
			test = &((*test)->next);
		}
		newlink->next = *test;
		*test = newlink;
		linkCount.fetch_add(1);
	}

	void Delete(ShaderResourceUpdateLink* deletelink)
	{
		ShaderResourceUpdateLink** link = &linkHead;
		while (*link != deletelink)
		{
			link = &((*link)->next);
		}

		*link = deletelink->next;

		ShaderResourceUpdate* region = &updateRegions[deletelink->shaderResourceIndex];
		region->bindingIndex = -1;
		region->copyCount = -1;
		region->data = nullptr;
		region->descriptorSet = -1;
		region->type = -1;
		region->dataSize = -1;
	}

	ShaderResourceUpdateLink* Find(int descriptor, int bindingindex)
	{
		ShaderResourceUpdateLink* link = linkHead;
		while (link && ((updateRegions[link->shaderResourceIndex].descriptorSet != descriptor) || (updateRegions[link->shaderResourceIndex].descriptorSet != bindingindex)))
		{
			link = link->next;
		}
		return link;
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	ShaderResourceUpdateLink* PopLink(ShaderResourceUpdate* outputRegion, ShaderResourceUpdateLink* link)
	{
		if (!link) return nullptr;

		ShaderResourceUpdate* region = &updateRegions[link->shaderResourceIndex];
		outputRegion->type = region->type;
		outputRegion->descriptorSet = region->descriptorSet;
		outputRegion->bindingIndex = region->bindingIndex;
		outputRegion->copyCount = region->copyCount;
		outputRegion->dataSize = region->dataSize;
		outputRegion->data = region->data;
		ShaderResourceUpdateLink* linkRet = link->next;
		if (region->copyCount > 1)
		{
			region->copyCount--;
			popPrev = &link->next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount--;
			region->bindingIndex = -1;
			region->copyCount = -1;
			region->data = nullptr;
			region->descriptorSet = -1;
			region->type = INVALID_SHADER_RESOURCE;
			region->dataSize = -1;
		}
		return linkRet;
	}
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
		int size;
		int offset;
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

