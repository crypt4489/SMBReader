#include "pch.h"

#include "VKDescriptorSetBuilder.h"
#include "VKDevice.h"


DescriptorSetBuilder::DescriptorSetBuilder(VKDevice* _d, size_t _ds)
	:
	device(_d),
	counter(0),
	descriptorSize(_ds),
	descriptorSets(reinterpret_cast<VkDescriptorSet*>(_d->AllocFromPerDeviceData(sizeof(VkDescriptorSet) * _ds)))
{

};

void DescriptorSetBuilder::AddPixelShaderImageDescription(VkImageView view, VkSampler sampler, uint32_t binding, uint32_t frames)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = view;
	imageInfo.sampler = sampler;


	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * frames));

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

	vkUpdateDescriptorSets(device->device, frames, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddStorageImageDescription(VkImageView view, uint32_t binding, uint32_t frames)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = view;
	imageInfo.sampler = VK_NULL_HANDLE;


	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * frames));

	for (uint32_t frame = 0; frame < frames; frame++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[frame];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;
		descriptorWrites[frame] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, frames, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{

	AddBufferTypePerFrame(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void DescriptorSetBuilder::AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypePerFrame(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

void DescriptorSetBuilder::AddUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypeDirect(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void DescriptorSetBuilder::AddDynamicUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypeDirect(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

void DescriptorSetBuilder::AddStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypePerFrame(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void DescriptorSetBuilder::AddDynamicStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypePerFrame(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

void DescriptorSetBuilder::AddStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypeDirect(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void DescriptorSetBuilder::AddDynamicStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
{
	AddBufferTypeDirect(buffer, size, binding, frames, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

void DescriptorSetBuilder::AddBufferTypePerFrame(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset, VkDescriptorType type)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * frames));
	VkDescriptorBufferInfo* bufferInfos = reinterpret_cast<VkDescriptorBufferInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorBufferInfo) * frames));

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
		descriptorWrite.descriptorType = type;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i] = descriptorWrite;
		offset += size;
	}

	vkUpdateDescriptorSets(device->device, frames, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddBufferTypeDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset, VkDescriptorType type)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * frames));
	VkDescriptorBufferInfo* bufferInfos = reinterpret_cast<VkDescriptorBufferInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorBufferInfo) * 1));

	auto& ref = bufferInfos[0];
	ref.buffer = buffer;
	ref.offset = offset;
	ref.range = size;

	for (uint32_t i = 0; i < frames; i++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = type;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, frames, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t frames)
{

	VkDescriptorSetLayout* layouts = reinterpret_cast<VkDescriptorSetLayout*>(device->AllocFromDeviceCache(sizeof(VkDescriptorSetLayout) * frames));

	for (uint32_t i = 0; i < frames; i++)
	{
		layouts[i] = descriptorSetLayout;
	}


	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = frames;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(device->device, &allocInfo, descriptorSets) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}
}

EntryHandle DescriptorSetBuilder::AddDescriptorsToCache()
{
	return device->CreateDescriptorSet(descriptorSets, descriptorSize);
}
