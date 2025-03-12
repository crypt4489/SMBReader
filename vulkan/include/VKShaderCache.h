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

	VkShaderModule GetShader(VkDevice& device, const std::string& name, VkShaderStageFlags flags);

	void DestroyShaderCache(VkDevice& device);
	
private:

	VkShaderModule CreateShader(VkDevice& logicalDevice, const std::string& name, VkShaderStageFlags flags);

	std::unordered_map<std::string, VkShaderModule> shaders;

};

