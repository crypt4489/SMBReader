#pragma once

#include <vector>
#include <stdexcept>



#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/ResourceLimits.h>
#include "glslang/Public/resource_limits_c.h"

#include "VKUtilities.h"

namespace GLSLANG
{
	void resource();

    VkShaderModule CompileShader(VkDevice& device, std::vector<char>& data, VkShaderStageFlags stage);
}

