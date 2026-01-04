#include "WindowManager.h"
#include <stdexcept>


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
    int ret = CreateOSWindow("MyGameEngine", 800, 600, &windowData);

    return ret;
}

int WindowManager::PollEvents()
{
    return PollOSWindowEvents(&windowData);;
}


void WindowManager::GetInternalData(OSWindowInternalData* data)
{
   GetInternalOSData(&windowData, data);
}