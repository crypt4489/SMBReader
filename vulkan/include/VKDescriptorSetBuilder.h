#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetBuilder
{
	DescriptorSetBuilder(VKDevice* _d, size_t _ds);

	DescriptorSetBuilder(VKDevice* _d, EntryHandle _dsi);

	void AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddDynamicUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddDynamicStorageBuffer(VkBuffer buffer, VkDeviceSize, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddDynamicStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement);

	void AddBufferTypePerFrame(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type, uint32_t firstSet, uint32_t dstArrayElement);

	void AddBufferTypeDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type, uint32_t firstSet, uint32_t dstArrayElement);

	void AddUniformBufferView(VkBufferView buffer, uint32_t binding, uint32_t firstSet, uint32_t setCount, uint32_t dstArrayElement);

	void AddStorageBufferView(VkBufferView buffer, uint32_t binding, uint32_t firstSet, uint32_t setCount, uint32_t dstArrayElement);

	void AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t setCount);

	void AddStorageImageDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount);

	void AddCombinedTextureArray(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet,  uint32_t setCount);

	void AddImageResourceDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount);

	void AddSamplerDescription(EntryHandle* samplerHandles, uint32_t samplerCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount);

	EntryHandle AddDescriptorsToCache();

	VkDescriptorSet* descriptorSets;
	VKDevice *device;
	size_t descriptorSize;
};
