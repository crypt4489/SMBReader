#pragma once

#include "WinOSWindow.h"
#include "OSWindow.h"
#include <string>

struct WindowManager
{
public:

	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	int CreateMainWindow();

	int PollEvents();

	void GetInternalData(OSWindowInternalData* data);

	void SetWindowTitle(const std::string& text);

	OSWindow windowData;
};

