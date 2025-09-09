#pragma once

#include "VKTypes.h"
#include "VKUtilities.h"
#include <unordered_map>

class VKDescriptorSetCache;

struct DescriptorSetBuilder
{
	DescriptorSetBuilder(VKDevice* _d, VKDescriptorSetCache* c, size_t _ds);

	void AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames);

	void AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames);

	void AddDescriptorsToCache(std::string name);

	VkDescriptorSet* descriptorSets;
	VKDevice *device;
	VKDescriptorSetCache* cache;
	size_t counter, descriptorSize;
};


class VKDescriptorSetCache
{
public:
	VkDescriptorSet GetDescriptorSetPerFrame(std::string& id, uint32_t frame);

	void AddDesciptorSet(std::string& id, VkDescriptorSet* set, uint32_t frames);

private:
	std::unordered_map<std::string, std::pair<uint32_t, VkDescriptorSet*>> descriptorSetCache;

};