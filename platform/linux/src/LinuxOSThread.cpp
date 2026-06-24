#include "OSThread.h"
#include <atomic>

#include <pthread.h>

struct ThreadData
{
    void* argumentToThread;
    ThreadPointer threadRoutine;
    pthread_t threadHandle;
    OSThreadFlags flags;
    int index;
    std::atomic<bool> doneFlag;
};

static std::atomic<int8_t>* freeList;
static int maxFreeListEntry = 0;
static ThreadData* dataThreads;

static void* ThreadFunction(void *arg);

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
    int threadDataSize = (maxNumberOfOpenThreads) * sizeof(ThreadData);
    int freeListSize = (maxNumberOfOpenThreads) * sizeof(std::atomic<int8_t>);

    OSThreadMemoryRequirements memReqs{ threadDataSize + freeListSize, alignof(ThreadData) };

    return memReqs;
}

int OSSeedThreadMemory(void* dataSource, int dataSize, int numberOfOpenThreads)
{
    uintptr_t dataHead = (uintptr_t)dataSource;

    dataThreads = (ThreadData*)dataSource;

    dataHead += sizeof(ThreadData) * numberOfOpenThreads;

    freeList = (std::atomic<int8_t>*)dataHead;

    for (int i = 0; i < numberOfOpenThreads; i++)
    {
        freeList[i] = 1;
    }

    maxFreeListEntry = numberOfOpenThreads;

    return 0;
}

int OSCreateThread(OSThreadHandle* handle, void* argumentToThread, ThreadPointer routine, OSThreadFlags flags)
{
    int index = FindFreeIndex();

    dataThreads[index].argumentToThread = argumentToThread;
    dataThreads[index].flags = flags;
    dataThreads[index].index = index;
    dataThreads[index].threadRoutine = routine;
    dataThreads[index].doneFlag = false;

    void* argumentPassed = argumentToThread;

    pthread_attr_t attributes;

    pthread_attr_init(&attributes);

    if (flags == OS_THREAD_ASYNC)
    {
        pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    }

    argumentPassed = &dataThreads[index];

    int threadCreateRet = pthread_create(&dataThreads[index].threadHandle, &attributes, ThreadFunction, argumentPassed);

    pthread_attr_destroy(&attributes);

    if (threadCreateRet)
        return OS_THREAD_FAILED_CREATE;

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
            if (dataThreads[idx].flags != OS_THREAD_ASYNC)
            {
                pthread_join(dataThreads[idx].threadHandle, NULL);
                freeList[idx].store(1, std::memory_order_release);
            }
        }
    }
}

int OSCloseThread(OSThreadHandle* handle)
{
    int index = handle->osDataHandle;

    if (index < 0 || index >= maxFreeListEntry)
        return OS_THREAD_FAILED_CLOSE;

    if (dataThreads[index].flags == OS_THREAD_ASYNC)
        return OS_THREAD_FAILED_CLOSE;
    
    if (!dataThreads[index].doneFlag.load(std::memory_order_acquire))
        return OS_THREAD_FAILED_CLOSE;

    freeList[index].store(1, std::memory_order_release);

    return 0;
}

int OSWaitThread(OSThreadHandle* handle, int timeout)
{
    /*
    int index = handle->osDataHandle;

    if (index < 0 || index >= maxFreeListEntry)
        return OS_THREAD_FAILED_JOIN;

    if (dataThreads[index].flags == OS_THREAD_ASYNC)
        return OS_THREAD_FAILED_JOIN;

    int ret = pthread_join(dataThreads[index].threadHandle, NULL);

    return (ret ? OS_THREAD_FAILED_JOIN : 0);
    */

    return -1;
}

int OSJoinThread(OSThreadHandle* handle)
{
    int index = handle->osDataHandle;

    if (index < 0 || index >= maxFreeListEntry)
        return OS_THREAD_FAILED_JOIN;

    if (dataThreads[index].flags == OS_THREAD_ASYNC)
        return OS_THREAD_FAILED_JOIN;

    int ret = pthread_join(dataThreads[index].threadHandle, NULL);

    return (ret ? OS_THREAD_FAILED_JOIN : 0);
}

void* ThreadFunction(void *arg) 
{
    ThreadData* data = (ThreadData*)arg;

    data->threadRoutine(data->argumentToThread);
    
    if (data->flags == OS_THREAD_ASYNC)
    {
        freeList[data->index].store(1, std::memory_order_release);
    }

    data->doneFlag.store(true, std::memory_order_release);

    return NULL; 
}