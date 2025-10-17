#pragma once
#include "IndexTypes.h"
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <variant>

enum BarrierAction
{
	WRITE_SHADER_RESOURCE = 1,
	READ_SHADER_RESOURCE = 2,
	READ_UNIFORM_BUFFER = 4,
	READ_VERTEX_INPUT = 8,
};

enum BarrierStage
{
	VERTEX_SHADER_STAGE = 1,
	VERTEX_INPUT_STAGE = 2,
	COMPUTE_STAGE = 4,
};

enum MemoryBarrierType
{
	MEMORY_BARRIER = 0,
	IMAGE_BARRIER = 1,
	BUFFER_BARRIER = 2,
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
	flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT) * ((sourceStage & VERTEX_SHADER_STAGE) != 0);
	flags |= (VK_PIPELINE_STAGE_VERTEX_INPUT_BIT) * ((sourceStage & VERTEX_INPUT_STAGE) != 0);
	flags |= (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) * ((sourceStage & COMPUTE_STAGE) != 0);
	return flags;
}

struct BarrierHeader
{
	MemoryBarrierType type;
	BarrierStage sourceStage;
	BarrierStage destinationStage;
	BarrierAction srcActions;
	BarrierAction dstActions;
};


struct ImageBarrier : public BarrierHeader
{
	EntryHandle imageId;
};

struct BufferBarrier : public BarrierHeader
{
	EntryHandle allocationIndex;
	size_t size;
};

struct ResourceGraphNode
{
	char* data;
	uint32_t pointer;

	ResourceGraphNode(uint32_t barrierCount, uint32_t imageCount)
		: 
		data((char*)malloc((sizeof(ImageBarrier) * imageCount) + (sizeof(BufferBarrier) * barrierCount))), 
		pointer(0)
	{

	}

	~ResourceGraphNode()
	{
		free(data);
	}

	void AddBufferBarrier(BarrierStage sourceStage, BarrierStage desinationStage, BarrierAction srcActions, BarrierAction dstActions, EntryHandle allocationIndex, size_t size) {
		BufferBarrier* bBarrier = (BufferBarrier*)(data + pointer);
		bBarrier->type = BUFFER_BARRIER;
		bBarrier->sourceStage = sourceStage;
		bBarrier->destinationStage = desinationStage;
		bBarrier->srcActions = srcActions;
		bBarrier->dstActions = dstActions;
		bBarrier->allocationIndex = allocationIndex;
		bBarrier->size = size;
		pointer += sizeof(BufferBarrier);
	}

	void AddImageBarrier(BarrierStage sourceStage, BarrierStage desinationStage, BarrierAction srcActions, BarrierAction dstActions, EntryHandle imageIndex) {
		ImageBarrier* bBarrier = (ImageBarrier*)(data + pointer);
		bBarrier->type = IMAGE_BARRIER;
		bBarrier->sourceStage = sourceStage;
		bBarrier->destinationStage = desinationStage;
		bBarrier->srcActions = srcActions;
		bBarrier->dstActions = dstActions;
		bBarrier->imageId = imageIndex;
		pointer += sizeof(ImageBarrier);
	}


	BarrierHeader* GetBarrierInfo(uint32_t* dataOffset)
	{
		if (*dataOffset >= pointer) return nullptr;
		BarrierHeader* header = (BarrierHeader*)data + *dataOffset;
		switch (header->type)
		{
		case BUFFER_BARRIER:
		{
			*dataOffset += sizeof(BufferBarrier);
			break;
		}
		case IMAGE_BARRIER:
		{
			*dataOffset += sizeof(ImageBarrier);
			break;
		}
		}
		return header;
	}


	
};



enum ShaderResourceType
{
	SAMPLER = 1,
	STORAGE_BUFFER = 2,
	UNIFORM_BUFFER = 4,
};

enum ShaderResourceAction
{
	SHADERREAD = 1,
	SHADERWRITE = 2,
	SHADERREADWRITE = 3
};

enum ShaderStageType
{
	VERTEXSTAGE = 1,
	FRAGMENTSTAGE = 2,
	COMPUTESTAGE = 3,
};

struct ShaderResource
{
	ShaderResourceAction action;
	ShaderResourceType type;
	int set;
	int binding;
};

struct ShaderMap
{
	ShaderStageType type;
	int shaderReference;
	int resourceCount; //array of contigous ShaderResources 

	uintptr_t GetResource(int resourceIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderMap);
		for (int i = 0; i < resourceIndex; i++)
		{
			head += sizeof(ShaderResource);
		}
		return head;
	}

	int GetMapSize() const
	{
		return sizeof(ShaderMap) + (resourceCount * sizeof(ShaderResource));
	}
};

struct ShaderGraph
{
	int shaderMapCount;

	uintptr_t GetMap(int mapIndex)
	{
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph);
		for (int i = 0; i < mapIndex; i++)
		{
			ShaderMap* map = (ShaderMap*)head;
			head += map->GetMapSize();
		}
		return head;
	}

	int GetGraphSize() const
	{
		int size = sizeof(ShaderGraph);
		uintptr_t head = (uintptr_t)(this) + sizeof(ShaderGraph);
		for (int i = 0; i < shaderMapCount; i++)
		{
			ShaderMap* map = (ShaderMap*)head;
			int size2 = map->GetMapSize();
			head += size2;
			size += size2;
		}
		return size;
	}
};

struct ShaderGraphsHolder
{
	int graphCount;

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

