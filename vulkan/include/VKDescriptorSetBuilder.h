#pragma once
#include "IndexTypes.h"
#include "VKTypes.h"
#include "VKUtilities.h"


struct DescriptorSetBuilder
{
	DescriptorSetBuilder(VKDevice* _d, size_t _ds);

	DescriptorSetBuilder(VKDevice* _d, EntryHandle _dsi);

	void AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames);

	void AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicStorageBuffer(VkBuffer buffer, VkDeviceSize, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddBufferTypePerFrame(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset, VkDescriptorType type);

	void AddBufferTypeDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset, VkDescriptorType type);

	void AddUniformBufferView(VkBufferView buffer, uint32_t binding, uint32_t frames);

	void AddStorageBufferView(VkBufferView buffer, uint32_t binding, uint32_t frames);

	void AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames);

	void AddStorageImageDescription(VkImageView view, uint32_t binding, uint32_t frames);

	void AddBindlessTextureArray(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t arrayCount, uint32_t frames, uint32_t binding);

	EntryHandle AddDescriptorsToCache();

	VkDescriptorSet* descriptorSets;
	VKDevice *device;
	size_t descriptorSize;
};
