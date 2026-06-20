#pragma once

#include "allocator/AppAllocator.h"
#include "CommonRenderTypes.h"
#include "logger/Logger.h"
#include "ShaderManagement.h"

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
	uintptr_t* descriptorSets{};

	EntryHandle deviceResourceHeap = EntryHandle();

	ShaderResourceManager() = default;

	void Create(Allocator* shaderResourceMemoryAllocator, uint32_t shaderResourceSlabSize, uint32_t maxDescriptorSets)
	{
		std::construct_at(&shaderResourceInstAllocator, shaderResourceMemoryAllocator->Allocate(shaderResourceSlabSize, alignof(void*)), shaderResourceSlabSize);
		descriptorSetHandles.Create(shaderResourceMemoryAllocator, maxDescriptorSets);
		memset(descriptorSetHandles.pool, 0xFF, sizeof(EntryHandle) * maxDescriptorSets);
		descriptorSets = (uintptr_t*)shaderResourceMemoryAllocator->Allocate(sizeof(uintptr_t) * maxDescriptorSets, alignof(uintptr_t));
	}

	int AddShaderToSets(uintptr_t location)
	{
		int indexRet = descriptorSetHandles.Allocate();

		descriptorSets[indexRet] = location;

		return indexRet;
	}

	void SetVariableArrayCount(ShaderResourceSetContext* context, int descriptorSet, int bindingIndex, int varArrayCount)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		
		uintptr_t* setOffsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceHeader* header = (ShaderResourceHeader*)setOffsets[bindingIndex];

		if (!(header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY))
		{
			//do something
		}

		header->arrayCount = (header->arrayCount & UNBOUNDED_DESCRIPTOR_ARRAY) | (varArrayCount & DESCRIPTOR_COUNT_MASK);

	}

	void BindBufferToShaderResource(ShaderResourceSetContext* context, int descriptorSet, int* allocationIndex, int* offsets,  int firstBuffer, int bufferCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* setOffsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBuffer* header = (ShaderResourceBuffer*)setOffsets[bindingIndex];

		if (header->type != ShaderResourceType::UNIFORM_BUFFER && header->type != ShaderResourceType::STORAGE_BUFFER)
			return;

		header->allocationIndex = allocationIndex;
		header->offsets = offsets;
		header->firstBuffer = firstBuffer;
		header->bufferCount = bufferCount;
	}

	void BindImageResourceToShaderResource(ShaderResourceSetContext* context, int descriptorSet, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::IMAGESTORE2D && header->type != ShaderResourceType::IMAGE2D)
			return;

		header->textureHandles = index;
		header->textureCount = textureCount;
		header->firstTexture = firstTexture;
	}

	void BindSamplerResourceToShaderResource(int descriptorSet, int* indices, int samplerCount, int firstSampler, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceSampler* header = (ShaderResourceSampler*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLERSTATE)
			return;

		header->samplerHandles = indices;
		header->samplerCount = samplerCount;
		header->firstSampler = firstSampler;
	}

	void BindSampledImageToShaderResource(ShaderResourceSetContext* context, int descriptorSet, int* index, int textureCount, int firstTexture, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceImage* header = (ShaderResourceImage*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::SAMPLER2D && header->type != ShaderResourceType::SAMPLERCUBE && header->type != ShaderResourceType::SAMPLER3D)
			return;

		header->textureHandles = index;
		header->textureCount = textureCount;
		header->firstTexture = firstTexture;
	}

	void BindBufferView(ShaderResourceSetContext* context, int descriptorSet, int* allocationIndex, int firstView, int viewCount, int bindingIndex)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));

		ShaderResourceBufferView* header = (ShaderResourceBufferView*)offsets[bindingIndex];

		if (header->type != ShaderResourceType::BUFFER_VIEW)
			return;


		header->viewCount = viewCount;
		header->firstView = firstView;
		header->allocationIndex = allocationIndex;
	}

	void BindBarrier(ShaderResourceSetContext* context, int descriptorSet, int binding, BarrierStage stage, BarrierAction action)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];



		switch (desc->type)
		{
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER2D:
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

	void BindImageBarrier(ShaderResourceSetContext* context, int descriptorSet, int binding, int barrierIndex, BarrierStage stage, BarrierAction action, ImageLayout oldLayout, ImageLayout dstLayout, bool location)
	{
		uintptr_t head = descriptorSets[descriptorSet];
		ShaderResourceSet* set = (ShaderResourceSet*)head;
		uintptr_t* offsets = (uintptr_t*)(head + sizeof(ShaderResourceSet));


		head = offsets[binding];
		ShaderResourceHeader* desc = (ShaderResourceHeader*)offsets[binding];

		if (desc->type != ShaderResourceType::SAMPLER2D && desc->type != ShaderResourceType::SAMPLERCUBE && desc->type != ShaderResourceType::SAMPLER3D && desc->type != ShaderResourceType::IMAGESTORE2D && desc->type != ShaderResourceType::IMAGE2D)
			return;

		switch (desc->type)
		{
		case ShaderResourceType::IMAGE2D:
		case ShaderResourceType::IMAGESTORE2D:
		case ShaderResourceType::SAMPLER2D:
			head += sizeof(ShaderResourceImage);

			break;
		case ShaderResourceType::STORAGE_BUFFER:
		case ShaderResourceType::UNIFORM_BUFFER:

			head += sizeof(ShaderResourceBuffer);
			break;
		}


		ShaderResourceImageBarrier* imageBarrier = (ShaderResourceImageBarrier*)head;


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

	void UploadConstant(ShaderResourceSetContext* context, int descriptorset, void* data, int bufferLocation)
	{
		ShaderResourceConstantBuffer* header = (ShaderResourceConstantBuffer*)GetConstantBuffer(descriptorset, bufferLocation);
		if (!header) return;
		header->data = data;
	}
};

int CreateShaderGraph(StringView filename, Allocator* readerMemory, ShaderGraph* graph, ShaderDetails* details, int* shaderDetailCount, Logger* outputLogger);
int CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, Allocator* tempAllocator, Logger* outputLogger);
int CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, Allocator* inputScratchAllocator, Logger* outputLogger);