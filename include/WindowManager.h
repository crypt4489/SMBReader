#pragma once
#if defined(_WIN32) || defined(_WIN64)
#include "WinOSWindow.h"
#elif defined(__linux__)
#include "LinuxOSWindow.h"
#endif
#include "OSWindow.h"
#include "StringUtils.h"

struct WindowManager
{
	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	int CreateMainWindow();

	int PollEvents();

	void GetInternalData(OSWindowInternalData* data);

	void SetWindowTitle(StringView text);

	OSWindow windowData;
};

