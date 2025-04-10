#include "VKShaderCache.h"

std::pair<VkShaderModule, VkShaderStageFlagBits> VKShaderCache::GetShader(const std::string& name)
{
	auto found = shaders.find(name);
	if (found == std::end(shaders))
	{
		VkShaderStageFlagBits flags = VKShaderCache::ConvertShaderFlags(name);
		auto ret = shaders[name] = { CreateShader(name, flags), flags };
		return std::make_pair(ret.mod, ret.flags);
	}
	return std::make_pair(found->second.mod, found->second.flags);
}

void VKShaderCache::DestroyShaderCache()
{
	for (auto& iter : shaders)
	{
		vkDestroyShaderModule(device, iter.second.mod, nullptr);
	}
}

VkShaderModule VKShaderCache::CreateShader(const std::string& name, VkShaderStageFlags flags)
{
	std::vector<char> buffer;

	VkShaderModule mod = VK_NULL_HANDLE;
	//if exists, it was compiled, otherwise it wasn't
	if (FileManager::FileExists(name)) {
		auto ret = FileManager::ReadFileInFull(name, buffer);

		if (ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

		mod = ::VK::Utils::createShaderModule(device, buffer);
	}
	else
	{
		std::string uncompiled = name.substr(0, name.length() - 4);

		auto ret = FileManager::ReadFileInFull(uncompiled, buffer);

		if (ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

		if (buffer.back() != '\0') buffer.push_back('\0');

		mod = GLSLANG::CompileShader(device, buffer, flags);
	}

	return mod;
}