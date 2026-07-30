#include "WindowManager.h"

bool WindowManager::ShouldCloseWindow()
{
	return windowData.info.shouldBeClosed;
}

void WindowManager::GetWindowSize(int* width, int* height)
{
    *width = windowData.info.width;
    *height = windowData.info.height;
}

int WindowManager::CreateMainWindow()
{
    int ret = OSCreateWindow("MyGameEngine", 800, 600, &windowData);

    return ret;
}

int WindowManager::PollEvents()
{
    return OSWindowPollEvents(&windowData);;
}

void WindowManager::GetInternalData(OSWindowInternalData* data)
{
   OSWindowGetInternalData(&windowData, data);
}

void WindowManager::SetWindowTitle(StringView text)
{
    OSWindowSetText(&windowData, text.stringData);
}