#include "OSThread.h"
#include "Windows.h"
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
static HANDLE* handles;
static ThreadData* dataThreads;

DWORD WINAPI MyThreadFunction(LPVOID lpParam);

static int FindFreeIndex()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        int idx = i;

        int8_t expected = 1;
        if (freeList[idx].compare_exchange_strong(
            expected,
            -1,
            std::memory_order_acquire,
            std::memory_order_relaxed))
        {
            return idx;
        }
    }
    return -1;
}

OSThreadMemoryRequirements OSGetThreadMemoryRequirements(int maxNumberOfOpenThreads)
{
    int handlesSize = (maxNumberOfOpenThreads) * sizeof(HANDLE);
    int threadDataSize = (maxNumberOfOpenThreads) * sizeof(ThreadData);
    int freeListSize = (maxNumberOfOpenThreads) * sizeof(std::atomic<int8_t>);

    OSThreadMemoryRequirements memReqs{ handlesSize + threadDataSize + freeListSize, alignof(HANDLE) };

    return memReqs;
}

int OSSeedThreadMemory(void* dataSource, int dataSize, int numberOfOpenThreads)
{
    uintptr_t dataHead = (uintptr_t)dataSource;

    handles = (void**)dataSource;

    int handleSize = numberOfOpenThreads;

    dataHead += sizeof(HANDLE) * handleSize;

    dataThreads = (ThreadData*)dataHead;

    dataHead += sizeof(ThreadData) * handleSize;

    freeList = (std::atomic<int8_t>*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        freeList[i] = 1;
    }

    maxFreeListEntry = handleSize;

    return 0;
}

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags)
{
    int index = FindFreeIndex();

    dataThreads[index].argumentToThread = argumentToThread;
    dataThreads[index].routine = routine;
    dataThreads[index].flags = flags;
    dataThreads[index].index = index;

    DWORD threadID;

   handles[index] = CreateThread(
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

void CloseAllThreads()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        int idx = i;

        int8_t expected = 1;
        if (freeList[idx].load(
            std::memory_order_acquire) == -1)
        {
            CloseHandle(handles[idx]);
            freeList[idx].store(1, std::memory_order_release);
        }
    }
}

int OSCloseThread(OSThreadHandle* handle)
{

    if (handle->osDataHandle == -1)
        return -1;

    HANDLE hand = handles[handle->osDataHandle];

    freeList[handle->osDataHandle].store(1, std::memory_order_release);

    CloseHandle(hand);

    return 0;
}

int OSWaitThread(OSThreadHandle* handle, int timeout)
{
    HANDLE hand = handles[handle->osDataHandle];
    WaitForSingleObject(hand, (DWORD)timeout);
    return 0;
}


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