#pragma once

#include <vulkan/vulkan.h>

#include <glslang/Include/glslang_c_interface.h>

struct GLSLShaderProgram
{
	glslang_shader_t* shaderInfoData;
	glslang_program_t* programData;
	char* shaderRawData;
	size_t shaderRawDataSize;
	const char* glslangMessages;
	int retCode;
};

namespace GLSLANG
{
	void resource();

	int CompileShader(VkDevice& device, char* data, VkShaderStageFlags stage, GLSLShaderProgram* shaderProgram);

	void DeleteShader(GLSLShaderProgram* shaderProgram);
}

