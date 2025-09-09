#include "VKDescriptorLayoutCache.h"
#include "VKDevice.h"

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(VKDevice* d, VKDescriptorLayoutCache* coa, uint32_t _bc)
	:
	device(d),
	cacheObj(coa),
	bindingCounts(_bc),
	counter(0),
	descSetBindings(
		reinterpret_cast<VkDescriptorSetLayoutBinding*>(
		d->AllocFromDeviceCache(sizeof(VkDescriptorSetLayoutBinding)
			* _bc)))
{

}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::CreateDescriptorSetLayout(std::string name)
{
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	if (!counter)
		return descriptorSetLayout;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindingCounts;
	layoutInfo.pBindings = descSetBindings;

	if (vkCreateDescriptorSetLayout(device->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	cacheObj->AddLayout(name, descriptorSetLayout);

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


VkDescriptorSetLayout VKDescriptorLayoutCache::GetLayout(std::string name)
{
	auto found = cache.find(name);
	if (found == std::end(cache))
	{
		throw std::runtime_error("Cannot find descriptor set layout from cache");
	}
	return found->second;
}

void VKDescriptorLayoutCache::AddLayout(std::string name, VkDescriptorSetLayout& layout)
{
	auto found = cache.find(name);
	if (found == std::end(cache))
	{
		cache[name] = layout;
	}
}

void VKDescriptorLayoutCache::DestroyLayoutCache()
{
	for (auto& i : cache)
	{
		vkDestroyDescriptorSetLayout(device, i.second, nullptr);
	}
}