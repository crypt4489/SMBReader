#include "WindowManager.h"
#include <stdexcept>

void WindowManager::CreateWindowInstance()
{
	bool good = glfwInit();
	if (!good) throw std::runtime_error("Cannot create window");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(800, 600, "SMB File Viewer", nullptr, nullptr);

}

void WindowManager::SetWindowResizeCallback(GLFWframebuffersizefun callback)
{
	glfwSetFramebufferSizeCallback(window, callback);
}

GLFWwindow* WindowManager::GetWindow() const
{
	return window;
}

void WindowManager::DestroyGLFWWindow()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

bool WindowManager::ShouldCloseWindow()
{
	if (glfwWindowShouldClose(window))
		return true;
	glfwPollEvents();
	return false;
}