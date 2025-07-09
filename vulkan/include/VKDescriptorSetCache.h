#pragma once

#include "VKUtilities.h"
#include <unordered_map>

class VKDescriptorSetCache;

struct DescriptorSetBuilder
{
	DescriptorSetBuilder(VkDevice _d, VKDescriptorSetCache* c) : device(_d), cache(c) {};

	void AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames);

	void AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames);

	void AddDescriptorsToCache(std::string name);

	std::vector<VkDescriptorSet> descriptorSets;
	VkDevice device;
	VKDescriptorSetCache* cache;
};


class VKDescriptorSetCache
{
public:
	VkDescriptorSet GetDescriptorSetPerFrame(std::string& id, uint32_t frame);

	void AddDesciptorSet(std::string& id, std::vector<VkDescriptorSet>& set);

private:
	std::unordered_map<std::string, std::vector<VkDescriptorSet>> descriptorSetCache;

};