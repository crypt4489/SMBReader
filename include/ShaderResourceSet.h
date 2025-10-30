#pragma once

#include <array>
#include <atomic>
#include <functional>

#include "ResourceDependencies.h"

enum ShaderBufferLocation
{
	DIRECT = 0,
	REPEAT = 1,
};

struct ShaderResourceSet
{
	int bindingCount;
	int layoutHandle;
	int setCount;
};

struct ShaderResourceHeader
{
	ShaderResourceType type;
	ShaderResourceAction action;
	int binding;
};

struct ShaderResourceImage : public ShaderResourceHeader
{
	EntryHandle textureHandle;
};

struct ShaderResourceBuffer : public ShaderResourceHeader
{
	int allocation;
	ShaderBufferLocation copyPattern;
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

template <int N>
struct ShaderResourceManager
{
	EntryHandle deviceResourceHeap;
	uintptr_t hostResourceHeap;
	uintptr_t hostResourceHead;
	std::array<uintptr_t, N> descriptorSets;
	std::atomic<int> descriptorSetIndex;

	int AddShaderToSets(uintptr_t location, size_t size)
	{
		int indexRet = descriptorSetIndex.fetch_add(1);

		descriptorSets[indexRet] = location;
		hostResourceHead += size;

		return indexRet;
	}

	void BindBufferToShaderResource(int descriptorSet, int allocationIndex, ShaderBufferLocation copy, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBuffer* header = (ShaderResourceBuffer*)offsets[bindingIndex];

		if (header->type != UNIFORM_BUFFER && header->type != STORAGE_BUFFER)
			return;

		header->copyPattern = copy;
		header->allocation = allocationIndex;
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
			head += sizeof(ShaderResourceImage);

			break;
		case STORAGE_BUFFER:
		case UNIFORM_BUFFER:

			head += sizeof(ShaderResourceBuffer);
			break;
		}

		ShaderResourceBarrier* barrier = (ShaderResourceBarrier*)head;

		barrier->dstAction = action;
		barrier->dstStage = stage;
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

};


