#pragma once

#include <stdexcept>

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
	void CreateWindowInstance()
	{
		bool good = glfwInit();
		if (!good) throw std::runtime_error("Cannot create window");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(800, 600, "SMB File Viewer", nullptr, nullptr);
		
	}

	void SetWindowResizeCallback(GLFWframebuffersizefun callback)
	{
		glfwSetFramebufferSizeCallback(window, callback);
	}

	GLFWwindow* GetWindow() const
	{
		return window;
	}

	void DestroyGLFWWindow()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	bool ShouldCloseWindow()
	{
		if (glfwWindowShouldClose(window))
			return true;
		glfwPollEvents();
		return false;
	}

private:
	GLFWwindow* window = nullptr;
};

