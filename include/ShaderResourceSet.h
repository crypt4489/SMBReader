#pragma once

#include <array>
#include <atomic>

#include "AppAllocator.h"
#include "AppTypes.h"
#include "FileManager.h"
#include "ResourceDependencies.h"


BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type);


template <int T_MaxRegionCopy>
struct ShaderResourceUpdatePool
{
	char stagingbuffer[32 * 1024];
	std::array<ShaderResourceUpdate, 1000> updateRegions;
	std::array<ShaderResourceUpdateLink, 1000> updateLinks;
	int linkHead = -1;
	int* popPrev = nullptr;
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
		case ShaderResourceType::SAMPLERBINDLESS:
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
			case ShaderResourceType::SAMPLERBINDLESS:
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
			link->next = -1;



			Insert(regionAlloc);




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

	void Insert(int index)
	{
		int newid = updateRegions[index].descriptorSet;
		int newbindingindex = updateRegions[index].bindingIndex;
		int* test = &linkHead;
		while ((*test != -1) && (updateRegions[updateLinks[(*test)].shaderResourceIndex].descriptorSet <= newid))
		{
			if ((updateRegions[updateLinks[(*test)].shaderResourceIndex].bindingIndex < newbindingindex))
				break;
			test = &(updateLinks[(*test)].next);
		}
		updateLinks[index].next = *test;
		*test = index;
		linkCount.fetch_add(1);
	}

	void Delete(ShaderResourceUpdateLink* deletelink)
	{
		/*
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
		region->type = ShaderResourceType::INVALID_SHADER_RESOURCE;
		region->dataSize = -1;
		*/
	}

	ShaderResourceUpdateLink* Find(int descriptor, int bindingindex)
	{
		int link = linkHead;
		while ((link != -1) && ((updateRegions[updateLinks[link].shaderResourceIndex].descriptorSet != descriptor) || (updateRegions[updateLinks[link].shaderResourceIndex].descriptorSet != bindingindex)))
		{
			link = updateLinks[link].next;
		}
		return (link == -1 || link >= 1000 ? nullptr : &updateLinks[link]);
	}

	void SetupPop()
	{
		popPrev = &linkHead;
	}

	int PopLink(ShaderResourceUpdate* outputRegion, int link)
	{
		if (link < 0) return -1;

		ShaderResourceUpdate* region = &updateRegions[updateLinks[link].shaderResourceIndex];
		outputRegion->type = region->type;
		outputRegion->descriptorSet = region->descriptorSet;
		outputRegion->bindingIndex = region->bindingIndex;
		outputRegion->copyCount = region->copyCount;
		outputRegion->dataSize = region->dataSize;
		outputRegion->data = region->data;
		int linkRet = updateLinks[link].next;
		if (region->copyCount > 1)
		{
			region->copyCount--;
			popPrev = &updateLinks[link].next;
		}
		else
		{
			*popPrev = linkRet;
			linkCount--;
			region->bindingIndex = -1;
			region->copyCount = -1;
			region->data = nullptr;
			region->descriptorSet = -1;
			region->type = ShaderResourceType::INVALID_SHADER_RESOURCE;
			region->dataSize = -1;
			updateLinks[link].next = -1;
			updateLinks[link].shaderResourceIndex = -1;
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

		if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
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

		if (header->type != ShaderResourceType::SAMPLER && header->type != ShaderResourceType::IMAGESTORE2D)
			return;

		header->textureHandle = index;
	}

	void BindSampledImageArrayToShaderResource(int descriptorSet, EntryHandle *indices, uint32_t texCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceSamplerBindless* header = (ShaderResourceSamplerBindless*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLERBINDLESS)
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

		if (header->type != ShaderResourceType::BUFFER_VIEW)
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
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER:
		{
			head += sizeof(ShaderResourceImage);
			ShaderResourceBarrier* barrier = (ShaderResourceBarrier*)head;

			barrier->dstAction = action;
			barrier->dstStage = stage;
			break;
		}
		case ShaderResourceType::BUFFER_VIEW:
		{
			head += sizeof(ShaderResourceBufferView);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:
		{

			head += sizeof(ShaderResourceBuffer);
			ShaderResourceBufferBarrier* barrier = (ShaderResourceBufferBarrier*)head;
			barrier->dstStage = stage;
			barrier->dstAction = action;
			break;
		}
		}

		
	}

	void BindImageBarrier(int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, ImageLayout oldLayout, ImageLayout dstLayout, bool location)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];

		if (desc->type != ShaderResourceType::SAMPLER && desc->type != ShaderResourceType::IMAGESTORE2D)
			return;

		switch (desc->type)
		{
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER:
			head += sizeof(ShaderResourceImage);

			break;
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:

			head += sizeof(ShaderResourceBuffer);
			break;
		}


		ImageShaderResourceBarrier* imageBarrier = (ImageShaderResourceBarrier*)head;


		imageBarrier[barrierIndex].imageType = ImageUsage::COLOR;
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

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER) return nullptr;

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
			if (ret->type == ShaderResourceType::CONSTANT_BUFFER) count++;
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

struct ShaderDetails
{
	int shaderNameSize;
	int shaderDataSize;

	ShaderDetails* GetNext();

	char* GetString();

	void* GetShaderData();
};


struct ShaderGraphReader
{
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

