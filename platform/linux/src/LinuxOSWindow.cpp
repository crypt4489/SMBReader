
#include "OSWindow.h"
#include "LinuxOSWindow.h"
#include <atomic>

static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;

#if defined(WINDOW_USE_WAYLAND)

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
    return -1;
}

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows)
{
    OSWindowMemoryRequirements memReqs{ 0, alignof(void*) };

    return memReqs;
}

void CloseAllWindows()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
}

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows)
{
    return 0;
}

int CreateOSWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    return 0;
}

int PollOSWindowEvents(OSWindow* window)
{
    int ret = 0;

    return ret;
}

int GetInternalOSData(OSWindow* window, void* internalDataStruct)
{
    return 0;
}

int SetOSWindowText(OSWindow* window, const char* text)
{
    return 0;
}

#elif defined(WINDOW_USE_X11)

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
    return -1;
}

OSWindowMemoryRequirements OSGetWindowMemoryRequirements(int maxNumberOfWindows)
{
    OSWindowMemoryRequirements memReqs{ 0, alignof(void*) };

    return memReqs;
}

void CloseAllWindows()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
}

int OSSeedWindowMemory(void* dataSource, int dataSize, int maxNumberOfWindows)
{
    return 0;
}

int CreateOSWindow(const char* name, int requestedDimensionX, int requestDimensionY, OSWindow* windowData)
{
    return 0;
}

int PollOSWindowEvents(OSWindow* window)
{
    int ret = 0;

    return ret;
}

int GetInternalOSData(OSWindow* window, void* internalDataStruct)
{
    return 0;
}

int SetOSWindowText(OSWindow* window, const char* text)
{
    return 0;
}

#endif