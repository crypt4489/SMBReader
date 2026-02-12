#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetLayoutBuilder
{
	DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc);
	VkDescriptorSetLayout [[maybe_unused]] CreateDescriptorSetLayout();

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddDynamicStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddStorageImageLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddUniformBufferViewLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddBindlessCombinedSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count);

	void AddBoundSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t count);
	
	void AddStorageBufferViewLayout(uint32_t binding, VkShaderStageFlags flags);

	void AddSamplerStateLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t count);

	void AddImageResourceLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t count);

	VkDescriptorSetLayoutBinding* descSetBindings;
	VkDescriptorBindingFlags* flags;

	VkDescriptorSetLayoutCreateFlags layoutFlags;

	VKDevice *device;
	uint32_t bindingCounts;
};

