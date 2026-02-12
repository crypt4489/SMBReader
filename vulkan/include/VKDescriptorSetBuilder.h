#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetBuilder
{
	DescriptorSetBuilder(VKDevice* _d, size_t _ds);

	DescriptorSetBuilder(VKDevice* _d, EntryHandle _dsi);

	void AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddDynamicUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddDynamicStorageBuffer(VkBuffer buffer, VkDeviceSize, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddDynamicStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset);

	void AddBufferTypePerFrame(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type);

	void AddBufferTypeDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type);

	void AddUniformBufferViewPerFrame(VkBufferView buffer, uint32_t binding, uint32_t setTarget);

	void AddStorageBufferViewPerFrame(VkBufferView buffer, uint32_t binding, uint32_t setTarget);

	void AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t setCount);

	void AddStorageImageDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount);

	void AddCombinedTextureArray(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet,  uint32_t setCount);

	void AddImageResourceDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount);

	void AddSamplerDescription(VkSampler sampler, uint32_t binding, uint32_t setCount);

	EntryHandle AddDescriptorsToCache();

	VkDescriptorSet* descriptorSets;
	VKDevice *device;
	size_t descriptorSize;
};
