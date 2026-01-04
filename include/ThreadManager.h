#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "OSMutex.h"


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
    static std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> threadsFlags;

    static std::vector<std::jthread> backgroundTasks;

    static void ASyncThreadsDone();

    static void DestroyThreadManager();

    template<class F, class... Args>
    static void LaunchASyncThread(F&& f, Args&&... args)
    {
        auto df = std::make_shared<std::atomic<bool>>(false);

        std::jthread thread = std::jthread{ std::forward<F>(f), df, std::forward<Args>(args)... };

        threadsFlags.emplace_back(std::move(thread), std::move(df));
    }

    template<class F, class... Args>
    static void LaunchBackgroundThread(F&& f, Args&&... args)
    {
        backgroundTasks.emplace_back(std::forward<F>(f), std::stop_token(), std::forward<Args>(args)...);
    }
};
