#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetLayoutBuilder
{
	DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc);
	VkDescriptorSetLayout [[maybe_unused]] CreateDescriptorSetLayout();

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddDynamicStorageBufferLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddStorageImageLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddUniformBufferViewLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddBindlessCombinedSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount);

	void AddBoundSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount);
	
	void AddStorageBufferViewLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddSamplerStateLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	void AddImageResourceLayout(uint32_t binding, VkShaderStageFlags flags, uint32_t arrayCount);

	VkDescriptorSetLayoutBinding* descSetBindings;
	VkDescriptorBindingFlags* flags;

	VkDescriptorSetLayoutCreateFlags layoutFlags;

	VKDevice *device;
	uint32_t bindingCounts;
};

