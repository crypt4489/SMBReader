#pragma once
#include "VKUtilities.h"
#include <unordered_map>
#include <vector>

struct DescriptorSetLayoutBuilder
{
	VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice& device);
	void AddPixelImageSamplerLayout(uint32_t binding);

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	std::vector<VkDescriptorSetLayoutBinding> descSetBindings;
};

class VKDescriptorLayoutCache
{
public:
	VkDescriptorSetLayout GetLayout(std::string name);

	void AddLayout(std::string name, VkDescriptorSetLayout& layout);

	void DestroyLayoutCache();

	std::unordered_map<std::string, VkDescriptorSetLayout> cache;
	VkDevice device;
};

