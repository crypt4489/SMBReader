#pragma once
#include "AppTypes.h"
#include "IndexTypes.h"
#include <array>

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

	uintptr_t shaderGraphs;
	size_t shaderGraphOffset;
	std::array<ShaderGraph*, T_ShaderGraphCount> shaderGraphPtrs;
	std::array<EntryHandle, T_ShaderCount> shaders;
	

	uintptr_t GetGraph(int graphIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraphsHolder);
		for (int i = 0; i < graphIndex; i++)
		{
			ShaderGraph* map = (ShaderGraph*)head;
			head += map->GetGraphSize();
		}
		return head;
	}
};

