#pragma once
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetLayoutBuilder
{
	DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc);
	VkDescriptorSetLayout [[maybe_unused]] CreateDescriptorSetLayout();

	void AddBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags stagestageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddStorageBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddDynamicStorageBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddStorageImageLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddUniformBufferViewLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddBindlessCombinedSamplersLayout(uint32_t binding, VkShaderStageFlags stagestageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddBoundSamplersLayout(uint32_t binding, VkShaderStageFlags stagestageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);
	
	void AddStorageBufferViewLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddSamplerStateLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	void AddImageResourceLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags);

	VkDescriptorSetLayoutBinding* descSetBindings;
	VkDescriptorBindingFlags* flags;

	VkDescriptorSetLayoutCreateFlags layoutFlags;

	VKDevice *device;
	uint32_t bindingCounts;
};

