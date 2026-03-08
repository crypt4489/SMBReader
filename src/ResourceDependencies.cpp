#include "ResourceDependencies.h"
uintptr_t ShaderGraph::GetSet(int setIndex)
{
	uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (setIndex * sizeof(ShaderResourceSetTemplate));
	return head;
}

uintptr_t ShaderGraph::GetResource(int resourceIndex)
{
	uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderResourceSetTemplate)) + (shaderMapCount * sizeof(ShaderMap));

	head += (sizeof(ShaderResource) * resourceIndex);

	return head;
}

uintptr_t ShaderGraph::GetMap(int mapIndex)
{
	uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderResourceSetTemplate)) + (mapIndex * sizeof(ShaderMap));

	return head;
}

int ShaderGraph::GetGraphSize() const
{
	int size = sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderResourceSetTemplate)) + (shaderMapCount * sizeof(ShaderMap)) + (resourceCount * sizeof(ShaderResource));
	return size;
}
int ShaderMap::GetMapSize() const
{
	return sizeof(ShaderMap);
}
