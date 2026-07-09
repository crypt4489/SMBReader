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
	PoolAllocator<EntryHandle> descriptorSetHandles{};
	ShaderResourceSet** descriptorSets{};

	EntryHandle deviceResourceHeap = EntryHandle();

	ShaderResourceManager() = default;

	void Create(Allocator* shaderResourceMemoryAllocator, uint32_t maxDescriptorSets);

	int AddShaderToSets(ShaderResourceSet* location);

	int GetConstantBufferCount(int descriptorSet);

	ShaderResourceHeader* GetConstantBuffer(int descriptorSet, int constantBuffer);
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

	ShaderResourceSetBuilder(int _descriptorManagerIndex, int _descriptorSetIndex, ShaderResourceSet* _setPtr);

	ShaderResourceSetHandle operator()();

	void SetVariableArrayCount(ShaderResourceSetContext* context, int bindingIndex, int varArrayCount);

	void BindBufferToShaderResource(ShaderResourceSetContext* context, int* allocationIndex, int firstBuffer, int bufferCount, int bindingIndex);

	void BindImageResourceToShaderResource(ShaderResourceSetContext* context, int* index, int* views, int textureCount, int firstTexture, int bindingIndex);

	void BindSamplerResourceToShaderResource(ShaderResourceSetContext* context, int* indices, int samplerCount, int firstSampler, int bindingIndex);

	void BindSampledImageToShaderResource(ShaderResourceSetContext* context, int* index, int* views, int* samplers, int textureCount, int firstTexture, int bindingIndex);

	void BindBufferView(ShaderResourceSetContext* context, int* allocationIndex, int firstView, int viewCount, int bindingIndex);

	ShaderResourceConstantBuffer* GetConstantBuffer(int constantBuffer);
	
	int GetConstantBufferCount();

	void UploadConstant(ShaderResourceSetContext* context, void* data, int bufferLocation);
};

int CreateShaderGraph(StringView filename, Allocator* readerMemory, ShaderGraph* graph, ShaderDetails* details, int* shaderDetailCount, Logger* outputLogger);
int CreatePipelineDescription(StringView filename, GenericPipelineStateInfo* stateInfo, Allocator* tempAllocator, Logger* outputLogger);
int CreateAttachmentGraphFromFile(StringView filename, AttachmentGraph* graph, Allocator* inputScratchAllocator, Logger* outputLogger);