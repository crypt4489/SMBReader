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

		return set->templateMetaData->constantsCount;
	}

	ShaderResourceHeader* GetConstantBuffer(int descriptorSet, int constantBuffer)
	{
		ShaderResourceSet* set = descriptorSets[descriptorSet];

		ShaderResourceConstantBuffer* ret = &set->constantBuffers[constantBuffer];

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER)
			return nullptr;

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
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (!(header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY))
		{
			//do something
		}

		header->arrayCount = (header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (varArrayCount & DESCRIPTOR_COUNT_MASK);
	}

	void BindBufferToShaderResource(ShaderResourceSetContext* context, int* allocationIndex, int firstBuffer, int bufferCount, int bindingIndex)
	{
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
			return;

		ShaderResourceBuffer* buffer = &header->resourceArray.buffers;

		for (int i = 0; i < bufferCount; i++)
			buffer->allocationIndex[firstBuffer + i] = allocationIndex[i];

		if ((firstBuffer + bufferCount) > buffer->bufferCount)
			buffer->bufferCount = (firstBuffer + bufferCount);

	}

	void BindImageResourceToShaderResource(ShaderResourceSetContext* context, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (header->type != ShaderResourceType::IMAGESTORE2D && header->type != ShaderResourceType::IMAGE2D)
			return;

		ShaderResourceImage* images = &header->resourceArray.images;

		for (int i = 0; i < textureCount; i++)
			images->textureHandles[firstTexture + i] = index[i];

		if ((firstTexture + textureCount) > images->textureCount)
			images->textureCount = (firstTexture + textureCount);
	}

	void BindSamplerResourceToShaderResource(int* indices, int samplerCount, int firstSampler, int bindingIndex)
	{
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLERSTATE)
			return;

		ShaderResourceSampler* samplers = &header->resourceArray.samplers;
	
		for (int i = 0; i < samplerCount; i++)
			samplers->samplerHandles[firstSampler + i] = indices[i];

		if ((firstSampler + samplerCount) > samplers->samplerCount)
			samplers->samplerCount = (firstSampler + samplerCount);
	}

	void BindSampledImageToShaderResource(ShaderResourceSetContext* context, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE && header->type != ShaderResourceType::SAMPLER3D)
			return;

		ShaderResourceImage* images = &header->resourceArray.images;

		for (int i = 0; i < textureCount; i++)
			images->textureHandles[firstTexture + i] = index[i];

		if ((firstTexture + textureCount) > images->textureCount)
			images->textureCount = (firstTexture + textureCount);
	}

	void BindBufferView(ShaderResourceSetContext* context, int* allocationIndex, int firstView, int viewCount, int bindingIndex)
	{
		ShaderResourceArray* header = &set->resourceBindings[bindingIndex];

		if (header->type != ShaderResourceType::BUFFER_VIEW)
			return;

		ShaderResourceBuffer* bufferView = &header->resourceArray.views;

		for (int i = 0; i < viewCount; i++)
		{
			bufferView->allocationIndex[firstView + i] = allocationIndex[i];
		}

		if ((firstView + viewCount) > bufferView->bufferCount)
			bufferView->bufferCount = (firstView + viewCount);

	}

	ShaderResourceConstantBuffer* GetConstantBuffer(int constantBuffer)
	{
		ShaderResourceConstantBuffer* ret = &set->constantBuffers[constantBuffer];

		if (ret->type != ShaderResourceType::CONSTANT_BUFFER) 
			return nullptr;

		return ret;
	}

	int GetConstantBufferCount()
	{
		return set->templateMetaData->constantsCount;
	}

	void UploadConstant(ShaderResourceSetContext* context, void* data, int bufferLocation)
	{
		ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(bufferLocation);
		
		if (!header) 
			return;
		
		header->data = data;
	}
};

int CreateShaderGraph(StringView filename, Allocator* readerMemory, ShaderGraph* graph, ShaderDetails* details, int* shaderDetailCount, Logger* outputLogger);
int CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, Allocator* tempAllocator, Logger* outputLogger);
int CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, Allocator* inputScratchAllocator, Logger* outputLogger);