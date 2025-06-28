#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace GLSLANG
{
	void resource();

    VkShaderModule CompileShader(VkDevice& device, std::vector<char>& data, VkShaderStageFlags stage);
}

