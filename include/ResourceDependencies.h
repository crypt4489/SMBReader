#pragma once
#include "IndexTypes.h"
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <variant>

enum BarrierActionBits
{
	WRITE_SHADER_RESOURCE = 1,
	READ_SHADER_RESOURCE = 2,
	READ_UNIFORM_BUFFER = 4,
	READ_VERTEX_INPUT = 8,
};

enum BarrierStageBits
{
	VERTEX_SHADER_BARRIER = 1,
	VERTEX_INPUT_BARRIER = 2,
	COMPUTE_BARRIER = 4,
	FRAGMENT_BARRIER = 8,
	BEGINNING_OF_PIPE = 16
};

typedef int BarrierAction;

typedef int BarrierStage;

enum MemoryBarrierType
{
	MEMORY_BARRIER = 0,
	IMAGE_BARRIER = 1,
	BUFFER_BARRIER = 2,
	BARRIER_MAX_ENUM
};

inline VkAccessFlags ConvertResourceActionToVulkan(BarrierAction action)
{
	VkAccessFlags flags = 0;
	flags |= (VK_ACCESS_SHADER_WRITE_BIT) * ((action & WRITE_SHADER_RESOURCE) != 0);
	flags |= (VK_ACCESS_SHADER_READ_BIT) * ((action & READ_SHADER_RESOURCE) != 0);
	flags |= (VK_ACCESS_UNIFORM_READ_BIT) * ((action & READ_UNIFORM_BUFFER) != 0);
	flags |= (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) * ((action & READ_VERTEX_INPUT) != 0);
	return flags;
}

inline VkPipelineStageFlags ConvertResourceStageToVulkan(BarrierStage sourceStage)
{
	VkPipelineStageFlags flags = 0;
	flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) * ((sourceStage & VERTEX_SHADER_BARRIER) != 0);
	flags |= (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) * ((sourceStage & VERTEX_INPUT_BARRIER) != 0);
	flags |= (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) * ((sourceStage & COMPUTE_BARRIER) != 0);
	flags |= (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) * ((sourceStage & BEGINNING_OF_PIPE) != 0);
	flags |= (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) * ((sourceStage & FRAGMENT_BARRIER) != 0);
	return flags;
}

enum ShaderResourceType
{
	SAMPLER = 1,
	STORAGE_BUFFER = 2,
	UNIFORM_BUFFER = 4,
	CONSTANT_BUFFER = 8,
	IMAGESTORE2D = 16,
	IMAGESTORE3D = 32,
	SAMPLERBINDLESS = 64,
};

enum ShaderResourceAction
{
	SHADERREAD = 1,
	SHADERWRITE = 2,
	SHADERREADWRITE = 3
};

enum ShaderStageTypeBits
{
	VERTEXSHADERSTAGE = 1,
	FRAGMENTSHADERSTAGE = 2,
	COMPUTESHADERSTAGE = 4,
};

typedef int ShaderStageType;


inline BarrierStage ConvertShaderStageToBarrierStage(ShaderStageType type)
{
	BarrierStage flags = 0;
	flags |= (VERTEX_SHADER_BARRIER) * ((type & VERTEXSHADERSTAGE) != 0);
//	flags |= (VK_SHADER_STAGE_FRAGMENT_BIT) * ((type & FRAGMENTSHADERSTAGE) != 0);
	flags |= (COMPUTE_BARRIER) * ((type & COMPUTESHADERSTAGE) != 0);
	return flags;
}

inline VkShaderStageFlags ConvertShaderStageToVKShaderStageFlags(ShaderStageType type)
{
	VkShaderStageFlags flags = 0;
	flags |= (VK_SHADER_STAGE_VERTEX_BIT) * ((type & VERTEXSHADERSTAGE) != 0);
	flags |= (VK_SHADER_STAGE_FRAGMENT_BIT) * ((type & FRAGMENTSHADERSTAGE) != 0);
	flags |= (VK_SHADER_STAGE_COMPUTE_BIT) * ((type & COMPUTESHADERSTAGE) != 0);
	return flags;
}

struct ShaderSetLayout
{
	int vkDescriptorLayout;
	int bindingCount;
	int resourceStart;
};

struct ShaderResource
{
	ShaderStageType stages;
	ShaderResourceAction action;
	ShaderResourceType type;
	int set;
	int binding;
	int arrayCount;
};

struct ShaderMap
{
	ShaderStageType type;
	int shaderReference;


	int GetMapSize() const
	{
		return sizeof(ShaderMap);
	}
};

struct ShaderGraph
{
	int shaderMapCount;
	int resourceSetCount;
	int resourceCount;

	uintptr_t GetSet(int setIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (setIndex * sizeof(ShaderSetLayout));
		return head;
	}

	uintptr_t GetResource(int resourceIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderSetLayout)) + (shaderMapCount * sizeof(ShaderMap));

		head += (sizeof(ShaderResource) * resourceIndex);

		return head;
	}

	uintptr_t GetMap(int mapIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderSetLayout)) + (mapIndex * sizeof(ShaderMap));
	
		return head;
	}

	int GetGraphSize() const
	{
		int size = sizeof(ShaderGraph) + (resourceSetCount * sizeof(ShaderSetLayout)) + (shaderMapCount * sizeof(ShaderMap)) + (resourceCount * sizeof(ShaderResource));
		return size;
	}
};

template <int N, int M>
struct ShaderGraphsHolder
{
	int graphCount;

	uintptr_t shaderGraphs;
	size_t shaderGraphOffset;
	std::array<ShaderGraph*, N> shaderGraphPtrs;
	std::array<EntryHandle, M> shaders;
	

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

