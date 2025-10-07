#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetLayoutBuilder
{
	DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc);
	VkDescriptorSetLayout [[maybe_unused]] CreateDescriptorSetLayout();
	void AddPixelImageSamplerLayout(uint32_t binding);

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	VkDescriptorSetLayoutBinding* descSetBindings;

	VKDevice *device;
	uint32_t bindingCounts;
	uint32_t counter;
};

