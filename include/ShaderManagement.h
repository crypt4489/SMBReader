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
	ShaderResource shaderResources[MAX_SHADER_RESOURCES];
	ShaderResourceSetTemplate shaderResourceSetTemplates[MAX_SHADER_RESOURCE_SET_TEMPLATES];
};

template <int T_ShaderGraphCount, int T_ShaderCount>
struct ShaderGraphsHolder
{
	int graphCount;
	int shaderCount;

	std::array<ShaderGraph, T_ShaderGraphCount> shaderGraphPtrs{};
	std::array<EntryHandle, T_ShaderCount> shaders{};
	std::array<ShaderDetails, T_ShaderCount> shaderDetails{};

	ShaderGraphsHolder()
		: 
		graphCount(0), shaderCount(0)
	{

	}
};

