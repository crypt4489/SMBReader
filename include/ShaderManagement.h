#pragma once

#include "allocator/AppAllocator.h"
#include "CommonRenderTypes.h"
#include <array>

#define SHADER_NAME_SIZE 64
#define MAX_SHADER_MAPS 4
#define MAX_SHADER_RESOURCES 32
#define MAX_SHADER_RESOURCE_SET_TEMPLATES 16

struct ShaderDetails
{
	char glslShaderName[SHADER_NAME_SIZE];
	int glslShaderNameSize;
	char hlslShaderName[SHADER_NAME_SIZE];
	int hlslShaderNameSize;
	ShaderComputeLayout computeLayout;
};

struct ShaderMap
{
	ShaderStageType type;
	int shaderReference;
};

struct ShaderGraph
{
	int shaderMapCount;
	int resourceSetCount;
	int resourceCount;

	ShaderMap shaderMaps[MAX_SHADER_MAPS];
	ShaderResourceTemplate shaderResources[MAX_SHADER_RESOURCES];
	ShaderResourceSetTemplate shaderResourceSetTemplates[MAX_SHADER_RESOURCE_SET_TEMPLATES];
};

struct ShaderGraphsHolder
{
	PoolAllocator<ShaderGraph> shaderGraphPtrs{};
	PoolAllocator<EntryHandle> shaders{};
	ShaderDetails* shaderDetails{};

	ShaderGraphsHolder() = default;

	void Create(Allocator* shaderGraphAllocator, uint32_t maxShaderGraphs, uint32_t maxShaderHandles)
	{
		shaderGraphPtrs.Create(shaderGraphAllocator, maxShaderGraphs);
		
		shaders.Create(shaderGraphAllocator, maxShaderHandles);

		shaderDetails = (ShaderDetails*)shaderGraphAllocator->Allocate(sizeof(ShaderDetails) * maxShaderHandles, alignof(ShaderDetails));
	}
};

