#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "FileManager.h"
#include "GlslangCompiler.h"
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

	VkShaderModule GetShader(VkDevice &device, const std::string& name, VkShaderStageFlags flags)
	{
		auto found = shaders.find(name);
		if (found == std::end(shaders))
		{
			auto ret = shaders[name] = CreateShader(device, name, flags);
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

	VkShaderModule CreateShader(VkDevice &logicalDevice, const std::string& name, VkShaderStageFlags flags)
	{
		std::vector<char> buffer;

		VkShaderModule mod = VK_NULL_HANDLE;
		//if exists, it was compiled, otherwise it wasn't
		if (FileManager::FileExists(name)) {
			auto ret = FileManager::ReadFileInFull(name, buffer);

			if (ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

			mod = ::VK::Utils::createShaderModule(logicalDevice, buffer);
		}
		else 
		{
			std::string uncompiled = name.substr(0, name.length() - 4);

			auto ret = FileManager::ReadFileInFull(uncompiled, buffer);

			if (ret) throw std::runtime_error("Cannot handle shader file " + name + " being opened");

			if (buffer[buffer.size()-1] != '\0') buffer.push_back('\0');

			mod = GLSLANG::CompileShader(logicalDevice, buffer, flags);
		} 

		return mod;
	}

	std::unordered_map<std::string, VkShaderModule> shaders;

};

