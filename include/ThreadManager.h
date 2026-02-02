#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "OSMutex.h"
#include "OSThread.h"

struct SharedExclusiveFlag
{
    OSSharedExclusive osse;

    void lock() noexcept;

    bool try_lock() noexcept;

    void unlock() noexcept;

    void lock_shared();
    
    void unlock_shared() noexcept;

    bool try_lock_shared();
};

struct Semaphore
{
    Semaphore() = default;

    ~Semaphore();

    void Create(int c = 1);

    void Wait();

    bool Wait(int timeout);
    
    void Notify();

    OSSemaphore semaphore;
};


struct SemaphoreGuard
{

    SemaphoreGuard(Semaphore& _s) : sema(_s) { sema.Wait(); }
    ~SemaphoreGuard() { sema.Notify(); }

    Semaphore& sema;
};

struct ThreadManager
{
    static void ASyncThreadsDone();

    static void DestroyThreadManager();


    static void LaunchOSASyncThread(ThreadPointer routine, void* args)
    {
        OSThreadHandle thread;
        OSCreateThread(&thread, args, routine, OS_THREAD_ASYNC);
        return;
    }


    static OSThreadHandle LaunchOSBackgroundThread(ThreadPointer routine, void* args)
    {
        OSThreadHandle thread;
        OSCreateThread(&thread, args, routine, OS_THREAD_NONE);
        return thread;
    }


};
