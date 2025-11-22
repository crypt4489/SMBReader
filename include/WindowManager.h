#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/include/GLFW/glfw3native.h"
#undef min
#undef max

struct WindowManager
{
public:
	void CreateWindowInstance();

	void SetWindowResizeCallback(GLFWframebuffersizefun callback);

	GLFWwindow* GetWindow() const;

	void DestroyGLFWWindow();

	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	GLFWwindow* window = nullptr;
};

