#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "AppTypes.h"
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

	VKShaderCache(VkDevice& _d);

	VKShaderCache(VKShaderCache&& other) = default;

	VKShaderCache(VKShaderCache& other) = default;

	VKShaderCache& operator=(VKShaderCache&& other) noexcept {
		this->device = other.device;
		this->shaders = std::move(other.shaders);
		return *this;
	};

	VKShaderCache& operator=(VKShaderCache& other) = default;

	std::pair<VkShaderModule, VkShaderStageFlagBits> GetShader(const std::string& name);

	void DestroyShaderCache();

	VkShaderStageFlagBits ConvertShaderFlags(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::PIX_SHADER:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		case ShaderType::VERT_SHADER:
			return VK_SHADER_STAGE_VERTEX_BIT;
		}

		return VK_SHADER_STAGE_ALL;
	}

	VkShaderStageFlagBits ConvertShaderFlags(const std::string& filename)
	{
		if (filename.find(".frag") != std::string::npos)
		{
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		else if (filename.find(".vert") != std::string::npos)
		{
			return VK_SHADER_STAGE_VERTEX_BIT;
		}

		return VK_SHADER_STAGE_ALL;
	}
	
	VkShaderModule CreateShader(const std::string& name, VkShaderStageFlags flags);

	struct ShaderCacheObject
	{
		VkShaderModule mod;
		VkShaderStageFlagBits flags;
	};
	std::unordered_map<std::string, ShaderCacheObject> shaders;
	VkDevice device;
};

