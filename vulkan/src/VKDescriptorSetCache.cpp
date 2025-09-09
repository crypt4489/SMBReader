#include "VKDescriptorSetCache.h"
#include "VKDevice.h"


DescriptorSetBuilder::DescriptorSetBuilder(VKDevice* _d, VKDescriptorSetCache* c, size_t _ds)
	:
	device(_d),
	cache(c),
	counter(0),
	descriptorSize(_ds),
	descriptorSets(reinterpret_cast<VkDescriptorSet*>(_d->AllocTypeFromEntry(sizeof(VkDescriptorSet) * _ds)))
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

void DescriptorSetBuilder::AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
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
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i] = descriptorWrite;
		offset += size;
	}

	vkUpdateDescriptorSets(device->device, frames, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t frames, VkDeviceSize offset)
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
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i] = descriptorWrite;
		offset += size;
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

void DescriptorSetBuilder::AddDescriptorsToCache(std::string name)
{
	cache->AddDesciptorSet(name, descriptorSets, descriptorSize);
}

VkDescriptorSet VKDescriptorSetCache::GetDescriptorSetPerFrame(std::string& id, uint32_t frame)
{
	VkDescriptorSet set = VK_NULL_HANDLE;

	auto found = descriptorSetCache.find(id);
	if (found == std::end(descriptorSetCache))
	{
		throw std::runtime_error("No descriptor set found");
	}
	auto& sets = found->second;
	size_t size = sets.first;
	if (frame >= size)
	{
		throw std::runtime_error("No descriptor set found for frame number");
	}
	set = sets.second[frame];
	return set;
}

void VKDescriptorSetCache::AddDesciptorSet(std::string& id, VkDescriptorSet* set, uint32_t frames)
{
	descriptorSetCache[id] = { frames, set }; // this is a copy right
}