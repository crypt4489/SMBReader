#pragma once

#include "VKUtilities.h"
#include <unordered_map>
struct DescriptorSetBuilder
{

	void AddPixelShaderImageDescription(VkDevice device, VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames);

	void AddUniformBuffer(VkDevice device, VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AddDynamicUniformBuffer(VkDevice device, VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset);

	void AllocDescriptorSets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames);

	std::vector<VkDescriptorSet> descriptorSets;
};


class VKDescriptorSetCache
{
public:
	VkDescriptorSet GetDescriptorSetPerFrame(std::string& id, uint32_t frame);

	void AddDesciptorSet(std::string& id, std::vector<VkDescriptorSet>& set);

private:
	std::unordered_map<std::string, std::vector<VkDescriptorSet>> descriptorSetCache;

};