#pragma once

#include "WinOSWindow.h"
#include "OSWindow.h"


struct WindowManager
{
public:

	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	int CreateMainWindow();

	int PollEvents();

	void GetInternalData(OSWindowInternalData* data);

	OSWindow windowData;
};

