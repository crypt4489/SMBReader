#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "FileManager.h"
#include "VKUtilities.h"
class VKShaderID
{
public:
	VKShaderID() = delete;
	explicit VKShaderID(uint32_t _id) : ID(_id) {};
	//delete copies / integer assignment
	constexpr VKShaderID& operator=(uint32_t) = delete;
	constexpr VKShaderID& operator=(VKShaderID&) = delete;
	VKShaderID(VKShaderID&) = delete;

	//keep moves
	VKShaderID(VKShaderID&&) = default;
	constexpr VKShaderID& operator=(VKShaderID&&) = default;

	constexpr uint32_t operator()() const {
		return ID;
	}
private:
	uint32_t ID;
};

class VKShaderCache
{
public:
	VKShaderCache() = default;

	VkShaderModule GetShader(VkDevice &device, const std::string& name)
	{
		auto found = shaders.find(name);
		if (found == std::end(shaders))
		{
			auto ret = shaders[name] = CreateShader(device, name);
			return ret;
		}
		return found->second;
	}

	void DestroyShaderCache(VkDevice& device)
	{
		for (auto& iter : shaders)
		{
			vkDestroyShaderModule(device, iter.second, nullptr);
		}
	}
private:

	VkShaderModule CreateShader(VkDevice &logicalDevice, const std::string& name)
	{
		std::vector<char> buffer;

		auto ret = FileManager::ReadFileInFull(name, buffer);

		if (ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

		VkShaderModule mod = ::VK::Utils::createShaderModule(logicalDevice, buffer);

		return mod;
	}

	std::unordered_map<std::string, VkShaderModule> shaders;

};

