#pragma once
#include "allocator/AppAllocator.h"
#include "CommonRenderTypes.h"
#include <array>

struct ShaderDetails
{
	int shaderNameSize;
	int shaderDataSize;

	ShaderDetails* GetNext();

	char* GetString();

	void* GetShaderData();
};

struct ShaderMap
{
	ShaderStageType type;
	int shaderReference;
	int GetMapSize() const;
};

struct ShaderGraph
{
	int shaderMapCount;
	int resourceSetCount;
	int resourceCount;

	uintptr_t GetSet(int setIndex);

	uintptr_t GetResource(int resourceIndex);

	uintptr_t GetMap(int mapIndex);

	int GetGraphSize() const;
};

template <int T_ShaderGraphCount, int T_ShaderCount>
struct ShaderGraphsHolder
{
	int graphCount;
	int shaderCount;

	SlabAllocator graphAllocator;
	SlabAllocator shaderDetailsAllocator;
	std::array<ShaderGraph*, T_ShaderGraphCount> shaderGraphPtrs{};
	std::array<EntryHandle, T_ShaderCount> shaders{};
	std::array<ShaderDetails*, T_ShaderCount> shaderDetails{};
	
	ShaderGraphsHolder() = default;

	ShaderGraphsHolder(void* gSlabHead, int gSlabSize, void* sSlabHead, int sSlabSize)
		: 
		graphCount(0), shaderCount(0),
		graphAllocator{gSlabHead, gSlabSize},
		shaderDetailsAllocator{sSlabHead, sSlabSize}
	{

	}
};

