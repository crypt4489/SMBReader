#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/include/GLFW/glfw3native.h"
#undef min
#undef max

class WindowManager
{
public:
	void CreateWindowInstance();

	void SetWindowResizeCallback(GLFWframebuffersizefun callback);

	GLFWwindow* GetWindow() const;

	void DestroyGLFWWindow();

	bool ShouldCloseWindow();

private:
	GLFWwindow* window = nullptr;
};

