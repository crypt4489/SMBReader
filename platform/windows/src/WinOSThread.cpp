#include "OSThread.h"
#include "Windows.h"

struct ThreadData
{
    ThreadPointer routine;
    void* argumentToThread;
    OSThreadFlags flags;
    int index;
};

static MPMCQueueData* freeList;
static HANDLE* handles;
static ThreadData* dataThreads;
static int maxFreeListEntry = 0;

ALIGNAS(64) static std::atomic<int> boundedLinearAllocator{ 0 };
ALIGNAS(64) static std::atomic<size_t> enqueuePos{ 0 };
ALIGNAS(64) static std::atomic<size_t> dequeuePos{ 0 };

static DWORD WINAPI MyThreadFunction(LPVOID lpParam);

static int InternalOSCloseThread(int index);

static int PopFromFreeList()
{
    MPMCQueueData* cell;

    size_t pos = dequeuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
        if (diff == 0)
        {
            if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return -1;
        else
            pos = dequeuePos.load(std::memory_order_relaxed);
    }

    cell->currentSequence.store(pos + maxFreeListEntry, std::memory_order_release);

    int freeListIndex = cell->freeIndex;

    cell->freeIndex = -1;

    return freeListIndex;
}

static void ReturnIndex(int index)
{
    MPMCQueueData* cell;

    size_t pos = enqueuePos.load(std::memory_order_relaxed);

    for (;;)
    {
        cell = &freeList[pos % maxFreeListEntry];
        size_t seq = cell->currentSequence.load(std::memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;
        if (diff == 0)
        {
            if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                break;
        }
        else if (diff < 0)
            return;
        else
            pos = enqueuePos.load(std::memory_order_relaxed);
    }

    handles[index] = INVALID_HANDLE_VALUE;

    cell->freeIndex = index;
    cell->currentSequence.store(pos + 1, std::memory_order_release);
}

static int FindFreeIndex()
{
    int ret = PopFromFreeList();

    if (ret < 0)
    {
        int linearTop = boundedLinearAllocator.load(std::memory_order_acquire);

        while (linearTop < maxFreeListEntry)
        {
            if (boundedLinearAllocator.compare_exchange_weak(linearTop, linearTop + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                ret = linearTop;
                break;
            }
        }
    }

    return ret;
}

OSThreadMemoryRequirements OSGetThreadMemoryRequirements(int maxNumberOfOpenThreads)
{
    int handlesSize = (maxNumberOfOpenThreads) * sizeof(HANDLE);
    int threadDataSize = (maxNumberOfOpenThreads) * sizeof(ThreadData);
    int freeListSize = (maxNumberOfOpenThreads) * sizeof(MPMCQueueData);

    OSThreadMemoryRequirements memReqs{ handlesSize + threadDataSize + freeListSize, alignof(HANDLE) };

    return memReqs;
}

int OSSeedThreadMemory(void* dataSource, int dataSize, int numberOfOpenThreads)
{
    uintptr_t dataHead = (uintptr_t)dataSource;

    handles = (HANDLE*)dataSource;

    int handleSize = numberOfOpenThreads;

    dataHead += sizeof(HANDLE) * handleSize;

    dataThreads = (ThreadData*)dataHead;

    dataHead += sizeof(ThreadData) * handleSize;

    freeList = (MPMCQueueData*)dataHead;

    for (int i = 0; i < handleSize; i++)
    {
        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
        handles[i] = INVALID_HANDLE_VALUE;
    }

    maxFreeListEntry = handleSize;

    return OS_THREAD_SUCCESS;
}

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags)
{
    int index = FindFreeIndex();

    if (index < 0)
    {
        return OS_THREAD_HANDLE_EXHAUSTED;
    }

    DWORD threadID;

    dataThreads[index].argumentToThread = argumentToThread;
    dataThreads[index].routine = routine;
    dataThreads[index].flags = flags;
    dataThreads[index].index = index;

    HANDLE hThread = CreateThread(
        NULL,                 
        0,                       
        MyThreadFunction,      
        &dataThreads[index],        
        0,                       
        &threadID);

    if (hThread == INVALID_HANDLE_VALUE)
    {
        ReturnIndex(index);
        return OS_THREAD_FAILED_CREATE;
    }

    handles[index] = hThread;

    handle->threadIdentifier = threadID;
    handle->osDataHandle = index;

    return OS_THREAD_SUCCESS;
}

void CloseAllThreads()
{
    for (int i = 0; i < maxFreeListEntry; i++)
    {
        if (handles[i] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handles[i]);
            handles[i] = INVALID_HANDLE_VALUE;
        }

        freeList[i].currentSequence.store(i, std::memory_order_relaxed);
    }

    enqueuePos.store(0, std::memory_order_relaxed);
    dequeuePos.store(0, std::memory_order_relaxed);
    boundedLinearAllocator.store(0, std::memory_order_relaxed);
}

int OSCloseThread(OSThreadHandle* handle)
{
    int ret = InternalOSCloseThread(handle->osDataHandle);

    handle->osDataHandle = -1;
    handle->threadIdentifier = 0;

    return ret;
}

int InternalOSCloseThread(int index)
{
    int osIndex = index;

    if (osIndex < 0 || osIndex >= maxFreeListEntry)
    {
        return OS_THREAD_HANDLE_OUT_OF_BOUNDS;
    }

    HANDLE hand = handles[osIndex];

    int retCode = 0;

    if (!CloseHandle(hand))
    {
        retCode = OS_THREAD_FAILED_CLOSE;
    }

    ReturnIndex(osIndex);

    return OS_THREAD_SUCCESS;
}


int OSWaitThread(OSThreadHandle* handle, int timeout)
{
    int handleIdx = handle->osDataHandle;

    if (handleIdx < 0 || handleIdx >= maxFreeListEntry)
    {
        return OS_THREAD_HANDLE_OUT_OF_BOUNDS;
    }

    HANDLE hand = handles[handle->osDataHandle];

    DWORD waitResult = WaitForSingleObject(hand, (DWORD)timeout);

    int ret = OS_THREAD_SUCCESS;

    switch (waitResult)
    {
    case WAIT_OBJECT_0:
        break;
    case WAIT_TIMEOUT:
        ret = OS_THREAD_FAILED_TIMEOUT;
        break;
    }

    return ret;
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
    ThreadData* data = (ThreadData*)lpParam;

    data->routine(data->argumentToThread);

    if (data->flags & OS_THREAD_ASYNC)
    {
        InternalOSCloseThread(data->index);
    }

    ExitThread(0);

    return 0;
}