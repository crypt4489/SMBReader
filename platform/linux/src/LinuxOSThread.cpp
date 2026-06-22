#include "OSThread.h"
#include <atomic>

struct ThreadData
{
    ThreadPointer routine;
    void* argumentToThread;
    OSThreadFlags flags;
    int index;
};


static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;
static ThreadData* dataThreads;


static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
       
    }
    return -1;
}

OSThreadMemoryRequirements OSGetThreadMemoryRequirements(int maxNumberOfOpenThreads)
{
    OSThreadMemoryRequirements memReqs{ 0, alignof(void*) };

    return memReqs;
}

int OSSeedThreadMemory(void* dataSource, int dataSize, int numberOfOpenThreads)
{
    return 0;
}

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags)
{
    return 0;
}

void CloseAllThreads()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
      
    }
}

int OSCloseThread(OSThreadHandle* handle)
{
    return 0;
}

int OSWaitThread(OSThreadHandle* handle, int timeout)
{
    return 0;
}

/*
DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;

    data->routine(data->argumentToThread);

    if (data->flags & OS_THREAD_ASYNC)
    {
        CloseHandle(handles[data->index]);
        freeList[data->index].store(1, std::memory_order_release);
    }

    ExitThread(0);

    return 0;
}
    */