#include "OSThread.h"
#include "Windows.h"
#include <atomic>

static HANDLE intThreadHandles[50];
static std::atomic<int> intThreadHandleCounter;

struct ThreadData
{
    ThreadPointer routine;
    void* argumentToThread;
    OSThreadFlags flags;
    int index;
};

ThreadData dataThreads[50];


DWORD WINAPI MyThreadFunction(LPVOID lpParam);


int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags)
{
	int index = intThreadHandleCounter.fetch_add(1);

    dataThreads[index].argumentToThread = argumentToThread;
    dataThreads[index].routine = routine;
    dataThreads[index].flags = flags;
    dataThreads[index].index = index;

    DWORD threadID;

    intThreadHandles[index] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        MyThreadFunction,       // thread function name
        &dataThreads[index],          // argument to thread function 
        0,                      // use default creation flags 
        &threadID);

    handle->threadIdentifier = threadID;
    handle->osDataHandle = index;

    return 0;
}

int OSCloseThread(OSThreadHandle* handle)
{
    HANDLE hand = intThreadHandles[handle->osDataHandle];

    CloseHandle(hand);

    return 0;
}

int OSWaitThread(OSThreadHandle* handle, int timeout)
{
    HANDLE hand = intThreadHandles[handle->osDataHandle];
    WaitForSingleObject(hand, (DWORD)timeout);
    return 0;
}


DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;

    data->routine(data->argumentToThread);

    if (data->flags & OS_THREAD_ASYNC)
    {
        CloseHandle(intThreadHandles[data->index]);
    }

    ExitThread(0);

    return 0;
}