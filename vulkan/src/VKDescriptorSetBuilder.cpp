#include "pch.h"

#include "VKDescriptorSetBuilder.h"
#include "VKDevice.h"
#include "VKTexture.h"


DescriptorSetBuilder::DescriptorSetBuilder(VKDevice* _d, size_t _ds)
	:
	device(_d),
	descriptorSize(_ds),
	descriptorSets(reinterpret_cast<VkDescriptorSet*>(_d->AllocFromPerDeviceData(sizeof(VkDescriptorSet) * _ds)))
{

};


DescriptorSetBuilder::DescriptorSetBuilder(VKDevice* _d, EntryHandle _dsi)
	: device(_d),
	descriptorSets(_d->GetDescriptorSets(_dsi))
{

} 

void DescriptorSetBuilder::AddUniformBufferView(VkBufferView buffer, uint32_t binding, uint32_t firstSet, uint32_t setCount, uint32_t dstArrayElement)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));

	for (uint32_t i = firstSet; i < setCount + firstSet; i++)
	{
		descriptorWrites[i-firstSet].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[i-firstSet].dstSet = descriptorSets[i];
		descriptorWrites[i-firstSet].dstBinding = binding;
		descriptorWrites[i-firstSet].dstArrayElement = 0;
		descriptorWrites[i-firstSet].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		descriptorWrites[i-firstSet].descriptorCount = 1;
		descriptorWrites[i-firstSet].pTexelBufferView = &buffer;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddStorageBufferView(VkBufferView buffer, uint32_t binding, uint32_t firstSet, uint32_t setCount, uint32_t dstArrayElement)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));

	for (uint32_t i = firstSet; i < setCount + firstSet; i++)
	{
		descriptorWrites[i - firstSet].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[i - firstSet].dstSet = descriptorSets[i];
		descriptorWrites[i - firstSet].dstBinding = binding;
		descriptorWrites[i - firstSet].dstArrayElement = 0;
		descriptorWrites[i - firstSet].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		descriptorWrites[i - firstSet].descriptorCount = 1;
		descriptorWrites[i - firstSet].pTexelBufferView = &buffer;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddStorageImageDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount)
{
	VkDescriptorImageInfo* imageInfos = reinterpret_cast<VkDescriptorImageInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorImageInfo) * texCount));

	for (uint32_t i = 0; i < texCount; i++)
	{
		VkDescriptorImageInfo* imageInfo = &imageInfos[i];
		VKTexture* tex = device->GetTexture(textureHandles[i]);
		imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo->imageView = device->GetImageViewByTexture(textureHandles[i], 0);
		imageInfo->sampler = VK_NULL_HANDLE;
	}

	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));

	for (uint32_t set = firstSet; set < firstSet+setCount; set++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[set];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptorWrite.descriptorCount = texCount;
		descriptorWrite.pImageInfo = imageInfos;
		descriptorWrites[set-firstSet] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddCombinedTextureArray(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));
	VkDescriptorImageInfo* imageInfos = reinterpret_cast<VkDescriptorImageInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorImageInfo) * texCount));

	for (uint32_t i = 0; i < texCount; i++)
	{
		VkDescriptorImageInfo* imageInfo = &imageInfos[i];
		VKTexture* tex = device->GetTexture(textureHandles[i]);
		imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo->imageView = device->GetImageViewByTexture(textureHandles[i], 0);
		imageInfo->sampler = device->GetSamplerByTexture(textureHandles[i], 0);
	}

	for (uint32_t set = firstSet; set < firstSet+setCount; set++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[set];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = texCount;
		descriptorWrite.pImageInfo = imageInfos;
		descriptorWrites[set-firstSet] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypePerFrame(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddDynamicUniformBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypePerFrame(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypeDirect(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddDynamicUniformBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypeDirect(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypePerFrame(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddDynamicStorageBuffer(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypePerFrame(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypeDirect(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddDynamicStorageBufferDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, uint32_t firstSet, uint32_t dstArrayElement)
{
	AddBufferTypeDirect(buffer, size, binding, setCount, offset, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, firstSet, dstArrayElement);
}

void DescriptorSetBuilder::AddBufferTypePerFrame(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type, uint32_t firstSet, uint32_t dstArrayElement)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));
	VkDescriptorBufferInfo* bufferInfos = reinterpret_cast<VkDescriptorBufferInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorBufferInfo) * setCount));

	for (uint32_t i = firstSet; i < setCount+firstSet; i++)
	{

		auto& ref = bufferInfos[i];
		ref.buffer = buffer;
		ref.offset = offset;
		ref.range = size;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = type;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i-firstSet] = descriptorWrite;
		offset += size;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddImageResourceDescription(EntryHandle* textureHandles, uint32_t texCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount)
{


	VkDescriptorImageInfo* imageInfos = reinterpret_cast<VkDescriptorImageInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorImageInfo) * texCount));

	for (uint32_t i = 0; i < texCount; i++)
	{
		VkDescriptorImageInfo* imageInfo = &imageInfos[i];
		VKTexture* tex = device->GetTexture(textureHandles[i]);
		imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo->imageView = device->GetImageViewByTexture(textureHandles[i], 0);
	}

	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));

	for (uint32_t set = firstSet; set < firstSet+setCount; set++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[set];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		descriptorWrite.descriptorCount = texCount;
		descriptorWrite.pImageInfo = imageInfos;
		descriptorWrites[set-firstSet] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}
void DescriptorSetBuilder::AddSamplerDescription(EntryHandle* samplerHandles, uint32_t samplerCount, uint32_t dstArrayElement, uint32_t binding, uint32_t firstSet, uint32_t setCount)
{

	VkDescriptorImageInfo* imageInfos = reinterpret_cast<VkDescriptorImageInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorImageInfo) * samplerCount));

	for (uint32_t i = 0; i < samplerCount; i++)
	{
		VkDescriptorImageInfo* imageInfo = &imageInfos[i];
		imageInfos[i].sampler = device->GetSamplerByHandle(samplerHandles[i]);
	}


	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));

	for (uint32_t set = firstSet; set < setCount+firstSet; set++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[set];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		descriptorWrite.descriptorCount = samplerCount;
		descriptorWrite.pImageInfo = imageInfos;
		descriptorWrites[set-firstSet] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AddBufferTypeDirect(VkBuffer buffer, VkDeviceSize size, uint32_t binding, uint32_t setCount, VkDeviceSize offset, VkDescriptorType type, uint32_t firstSet, uint32_t dstArrayElement)
{
	VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(device->AllocFromDeviceCache(sizeof(VkWriteDescriptorSet) * setCount));
	VkDescriptorBufferInfo* bufferInfos = reinterpret_cast<VkDescriptorBufferInfo*>(device->AllocFromDeviceCache(sizeof(VkDescriptorBufferInfo) * 1));

	auto& ref = bufferInfos[0];
	ref.buffer = buffer;
	ref.offset = offset;
	ref.range = size;

	for (uint32_t i = firstSet; i < firstSet+setCount; i++)
	{
		VkWriteDescriptorSet descriptorWrite{};

		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = dstArrayElement;
		descriptorWrite.descriptorType = type;
		descriptorWrite.descriptorCount = 1;

		descriptorWrite.pBufferInfo = &ref;
		descriptorWrites[i-firstSet] = descriptorWrite;
	}

	vkUpdateDescriptorSets(device->device, setCount, descriptorWrites, 0, nullptr);
}

void DescriptorSetBuilder::AllocDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, uint32_t setCount)
{

	VkDescriptorSetLayout* layouts = reinterpret_cast<VkDescriptorSetLayout*>(device->AllocFromDeviceCache(sizeof(VkDescriptorSetLayout) * setCount));

	for (uint32_t i = 0; i < setCount; i++)
	{
		layouts[i] = descriptorSetLayout;
	}


	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = setCount;
	allocInfo.pSetLayouts = layouts;

	if (vkAllocateDescriptorSets(device->device, &allocInfo, descriptorSets) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}
}

EntryHandle DescriptorSetBuilder::AddDescriptorsToCache()
{
	return device->CreateDescriptorSet(descriptorSets, static_cast<uint32_t>(descriptorSize));
}
