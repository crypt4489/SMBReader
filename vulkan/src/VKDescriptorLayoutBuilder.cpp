#include "pch.h"

#include "VKDescriptorLayoutBuilder.h"
#include "VKDevice.h"

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(VKDevice* d, uint32_t _bc)
	:
	device(d),
	bindingCounts(_bc),
	counter(0),
	descSetBindings(
		reinterpret_cast<VkDescriptorSetLayoutBinding*>(
		d->AllocFromDeviceCache(sizeof(VkDescriptorSetLayoutBinding)
			* _bc)))
{

}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	if (counter != bindingCounts)
		return descriptorSetLayout;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindingCounts;
	layoutInfo.pBindings = descSetBindings;

	if (vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	return descriptorSetLayout;
}

void DescriptorSetLayoutBuilder::AddPixelImageSamplerLayout(uint32_t binding)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	descSetBindings[counter++] = layoutBinding;
}

void DescriptorSetLayoutBuilder::AddBufferLayout(uint32_t binding, VkShaderStageFlags flags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = flags;

	descSetBindings[counter++] = layoutBinding;
}

void DescriptorSetLayoutBuilder::AddDynamicBufferLayout(uint32_t binding, VkShaderStageFlags flags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = flags;

	descSetBindings[counter++] = layoutBinding;
}
