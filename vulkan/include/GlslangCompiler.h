#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace GLSLANG
{
	void resource();

    VkShaderModule CompileShader(VkDevice& device, char* data, VkShaderStageFlags stage);
}

