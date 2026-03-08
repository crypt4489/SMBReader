#include "pch.h"

#include "VKDescriptorLayoutBuilder.h"
#include "VKDevice.h"

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc)
	:
	device(d),
	bindingCounts(_bc),
	layoutFlags(0),
	descSetBindings(
		reinterpret_cast<VkDescriptorSetLayoutBinding*>(
		d->AllocFromDeviceCache(sizeof(VkDescriptorSetLayoutBinding)
			* _bc))),
	flags(reinterpret_cast<VkDescriptorBindingFlags*>(
		d->AllocFromDeviceCache(sizeof(VkDescriptorBindingFlags)
			* _bc)))
{
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;


	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindingCounts;
	layoutInfo.pBindings = descSetBindings;
	layoutInfo.flags = layoutFlags;

	
	VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{};
	binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	binding_flags.bindingCount = bindingCounts;
	binding_flags.pBindingFlags = flags;
	layoutInfo.pNext = &binding_flags;
	

	if (vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	return descriptorSetLayout;
}

void DescriptorSetLayoutBuilder::AddStorageImageLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddUniformBufferViewLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddStorageBufferViewLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddStorageBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddDynamicStorageBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddBindlessCombinedSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;


	flags[binding] = bindFlags;

	descSetBindings[binding] = layoutBinding;
	
}

void DescriptorSetLayoutBuilder::AddBoundSamplersLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;


	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}


void DescriptorSetLayoutBuilder::AddSamplerStateLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;

	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}

void DescriptorSetLayoutBuilder::AddImageResourceLayout(uint32_t binding, VkShaderStageFlags stageFlags, uint32_t arrayCount, VkDescriptorBindingFlags bindFlags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	layoutBinding.descriptorCount = arrayCount;
	layoutBinding.stageFlags = stageFlags;


	descSetBindings[binding] = layoutBinding;
	flags[binding] = bindFlags;
}