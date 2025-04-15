#pragma once

#include "VKUtilities.h"
#include <unordered_map>
struct DescriptorSetBuilder
{

	void AddPixelShaderImageDescription(VkDevice device, VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames)
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = view;
		imageInfo.sampler = sampler;

		std::vector<VkWriteDescriptorSet> descriptorWrites(frames);

		for (uint32_t frame = 0; frame < frames; frame++)
		{
			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSets[frame];
			descriptorWrite.dstBinding = binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;
			descriptorWrites[frame] = descriptorWrite;
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	void AddUniformBuffer(VkDevice device, VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
	{
		std::vector<VkDescriptorBufferInfo> bufferInfos(frames);
		std::vector<VkWriteDescriptorSet> descriptorWrites(frames);
		
		for (uint32_t i = 0; i < frames; i++)
		{

			auto& ref = bufferInfos[i];
			ref.buffer = buffer;
			ref.offset = offset;
			ref.range = size;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSets[i];
			descriptorWrite.dstBinding = binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrite.descriptorCount = 1;

			descriptorWrite.pBufferInfo = &ref;
			descriptorWrites[i] = descriptorWrite;
			offset += size;
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	void AddDynamicUniformBuffer(VkDevice device, VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
	{
		std::vector<VkDescriptorBufferInfo> bufferInfos(frames);
		std::vector<VkWriteDescriptorSet> descriptorWrites(frames);

		for (uint32_t i = 0; i < frames; i++)
		{

			auto& ref = bufferInfos[i];
			ref.buffer = buffer;
			ref.offset = offset;
			ref.range = size;

			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSets[i];
			descriptorWrite.dstBinding = binding;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			descriptorWrite.descriptorCount = 1;

			descriptorWrite.pBufferInfo = &ref;
			descriptorWrites[i] = descriptorWrite;
			offset += size;
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	void AllocDescriptorSets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames)
	{
		descriptorSets.resize(frames);
		std::vector<VkDescriptorSetLayout> layouts(frames, descriptorSetLayout);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = frames;
		allocInfo.pSetLayouts = layouts.data();

		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
	}

	std::vector<VkDescriptorSet> descriptorSets;
};


class VKDescriptorSetCache
{
public:
	VkDescriptorSet GetDescriptorSetPerFrame(std::string& id, uint32_t frame)
	{
		VkDescriptorSet set = VK_NULL_HANDLE;

		auto found = descriptorSetCache.find(id);
		if (found == std::end(descriptorSetCache))
		{
			throw std::runtime_error("No descriptor set found");
		}
		auto& sets = found->second;
		size_t size = sets.size();
		if (frame >= size)
		{
			throw std::runtime_error("No descriptor set found for frame number");
		}
		set = sets[frame];
		return set;
	}

	void AddDesciptorSet(std::string& id, std::vector<VkDescriptorSet>& set)
	{
		descriptorSetCache[id] = set; // this is a copy right
	}
private:
	std::unordered_map<std::string, std::vector<VkDescriptorSet>> descriptorSetCache;

};

