#pragma once

#include "allocator/AppAllocator.h"
#include "CommonRenderTypes.h"
#include "logger/Logger.h"
#include "ShaderManagement.h"

#include <string.h>

BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type);

struct ShaderResourceSetContext
{
	Logger* contextLogger;
	bool contextFailed;
};

struct ShaderResourceManager
{
	SlabAllocator shaderResourceInstAllocator{};
	PoolAllocator<EntryHandle> descriptorSetHandles{};
	ShaderResourceSet** descriptorSets{};

	EntryHandle deviceResourceHeap = EntryHandle();

	ShaderResourceManager() = default;

	void Create(Allocator* shaderResourceMemoryAllocator, uint32_t shaderResourceSlabSize, uint32_t maxDescriptorSets)
	{
		std::construct_at(&shaderResourceInstAllocator, shaderResourceMemoryAllocator->Allocate(shaderResourceSlabSize, alignof(void*)), shaderResourceSlabSize);
		descriptorSetHandles.Create(shaderResourceMemoryAllocator, maxDescriptorSets);
		memset(descriptorSetHandles.pool, 0xFF, sizeof(EntryHandle) * maxDescriptorSets);
		descriptorSets = (ShaderResourceSet**)shaderResourceMemoryAllocator->Allocate(sizeof(ShaderResourceSet*) * maxDescriptorSets, alignof(ShaderResourceSet*));
	}

	int AddShaderToSets(ShaderResourceSet* location)
	{
		int indexRet = descriptorSetHandles.Allocate();

		descriptorSets[indexRet] = location;

		return indexRet;
	}

	int GetConstantBufferCount(int descriptorSet)
	{
		ShaderResourceSet* set = descriptorSets[descriptorSet];

		uintptr_t* offsets = (uintptr_t*)(set + 1);

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

	ShaderResourceHeader* GetConstantBuffer(int descriptorSet, int constantBuffer)
	{
		ShaderResourceSet* set = descriptorSets[descriptorSet];

		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[set->bindingCount - (constantBuffer + 1)]);

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER) return nullptr;

		return ret;
	}
};

struct ShaderResourceSetHandle
{
	ShaderResourceSetHandle() = default;

	ShaderResourceSetHandle(int _descriptorManagerIndex, int _descriptorSetIndex)
		:
		descriptorManagerIndex(_descriptorManagerIndex), descriptorSetIndex(_descriptorSetIndex)
	{

	}

	int descriptorManagerIndex;
	int descriptorSetIndex;
};

struct ShaderResourceSetBuilder
{
	ShaderResourceSet* set;
	ShaderResourceSetHandle handle{};

	ShaderResourceSetBuilder(int _descriptorManagerIndex, int _descriptorSetIndex, ShaderResourceSet* _setPtr)
		:
		set(_setPtr), handle(_descriptorManagerIndex, _descriptorSetIndex)
	{

	}

	ShaderResourceSetHandle operator()()
	{
		return handle;
	}

	void SetVariableArrayCount(ShaderResourceSetContext* context, int bindingIndex, int varArrayCount)
	{
		uintptr_t* setOffsets = (uintptr_t*)(set + 1);

		ShaderResourceHeader* header = (ShaderResourceHeader*)setOffsets[bindingIndex];

		if (!(header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY))
		{
			//do something
		}

		header->arrayCount = (header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (varArrayCount & DESCRIPTOR_COUNT_MASK);

	}

	void BindBufferToShaderResource(ShaderResourceSetContext* context, int* allocationIndex, int* offsets, int firstBuffer, int bufferCount, int bindingIndex)
	{
		uintptr_t* setOffsets = (uintptr_t*)(set + 1);

		ShaderResourceBuffer* header = (ShaderResourceBuffer*)setOffsets[bindingIndex];

		if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
			return;

		for (int i = 0; i < bufferCount; i++)
		{
			header->allocationIndex[firstBuffer + i] = allocationIndex[i];
		}

		if ((firstBuffer + bufferCount) > header->bufferCount)
			header->bufferCount = (firstBuffer + bufferCount);

	}

	void BindImageResourceToShaderResource(ShaderResourceSetContext* context, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::IMAGESTORE2D && header->type != ShaderResourceType::IMAGE2D)
			return;

		for (int i = 0; i < textureCount; i++)
		{
			header->textureHandles[firstTexture + i] = index[i];
		}

		if ((firstTexture + textureCount) > header->textureCount)
			header->textureCount = (firstTexture + textureCount);
	}

	void BindSamplerResourceToShaderResource(int* indices, int samplerCount, int firstSampler, int bindingIndex)
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceSampler* header = (ShaderResourceSampler*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLERSTATE)
			return;
	
		for (int i = 0; i < samplerCount; i++)
		{
			header->samplerHandles[firstSampler + i] = indices[i];
		}

		if ((firstSampler + samplerCount) > header->samplerCount)
			header->samplerCount = (firstSampler + samplerCount);
	}

	void BindSampledImageToShaderResource(ShaderResourceSetContext* context, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE && header->type != ShaderResourceType::SAMPLER3D)
			return;

		for (int i = 0; i < textureCount; i++)
		{
			header->textureHandles[firstTexture + i] = index[i];
		}

		if ((firstTexture + textureCount) > header->textureCount)
			header->textureCount = (firstTexture + textureCount);
	}

	void BindBufferView(ShaderResourceSetContext* context, int* allocationIndex, int firstView, int viewCount, int bindingIndex)
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceBufferView* header = (ShaderResourceBufferView*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::BUFFER_VIEW)
			return;

		for (int i = 0; i < viewCount; i++)
		{
			header->allocationIndex[firstView + i] = allocationIndex[i];
		}

		if ((firstView + viewCount) > header->viewCount)
			header->viewCount = (firstView + viewCount);

	}

	ShaderResourceHeader* GetConstantBuffer(int constantBuffer)
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

		ShaderResourceHeader* ret = (ShaderResourceHeader*)(offsets[set->bindingCount - (constantBuffer + 1)]);

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER) return nullptr;

		return ret;
	}

	int GetConstantBufferCount()
	{
		uintptr_t* offsets = (uintptr_t*)(set + 1);

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

	void UploadConstant(ShaderResourceSetContext* context, void* data, int bufferLocation)
	{
		ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(bufferLocation);
		if (!header) return;
		header->data = data;
	}
};

int CreateShaderGraph(StringView filename, Allocator* readerMemory, ShaderGraph* graph, ShaderDetails* details, int* shaderDetailCount, Logger* outputLogger);
int CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, Allocator* tempAllocator, Logger* outputLogger);
int CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, Allocator* inputScratchAllocator, Logger* outputLogger);