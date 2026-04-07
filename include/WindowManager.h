#pragma once

#include "WinOSWindow.h"
#include "OSWindow.h"
#include "AppTypes.h"

struct WindowManager
{
public:

	bool ShouldCloseWindow();

	void GetWindowSize(int* width, int* height);

	int CreateMainWindow();

	int PollEvents();

	void GetInternalData(OSWindowInternalData* data);

	void SetWindowTitle(StringView text);

	OSWindow windowData;
};

