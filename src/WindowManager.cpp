#include "WindowManager.h"

bool WindowManager::ShouldCloseWindow()
{
	return windowKeyData.shouldBeClosed;
}

void WindowManager::GetWindowSize(int* width, int* height)
{
    *width = windowKeyData.width;
    *height = windowKeyData.height;
}

int WindowManager::CreateMainWindow(int width, int height, const char* name, int nameLength)
{
    int ret = OSCreateWindow(name, width, height, &windowData);

    return ret;
}

int WindowManager::PollEvents()
{
    return OSWindowPollEvents(&windowData, &windowKeyData);
}

void WindowManager::GetInternalData(OSWindowInternalData* data)
{
   OSWindowGetInternalData(&windowData, data);
}

void WindowManager::SetWindowTitle(StringView text)
{
    OSWindowSetText(&windowData, text.stringData);
}