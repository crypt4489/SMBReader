#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"
#include <unordered_map>



class VKDescriptorLayoutCache
{
public:
	VkDescriptorSetLayout GetLayout(std::string name);

	void AddLayout(std::string name, VkDescriptorSetLayout& layout);

	void DestroyLayoutCache();

	std::unordered_map<std::string, VkDescriptorSetLayout> cache;
	VkDevice device;
};

struct DescriptorSetLayoutBuilder
{
	DescriptorSetLayoutBuilder(VKDevice* d, VKDescriptorLayoutCache* coa, uint32_t _bc);
	VkDescriptorSetLayout [[maybe_unused]] CreateDescriptorSetLayout(std::string name);
	void AddPixelImageSamplerLayout(uint32_t binding);

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	VkDescriptorSetLayoutBinding* descSetBindings;

	VKDevice *device;
	VKDescriptorLayoutCache* cacheObj;
	uint32_t bindingCounts;
	uint32_t counter;
};

