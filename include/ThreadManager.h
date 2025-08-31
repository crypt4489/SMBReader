#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class SharedExclusiveFlag
{
    std::atomic_flag exclusiveFlag;
    std::atomic<int> sharedCount;
public:

    void lock() noexcept
    {
        while (exclusiveFlag.test_and_set(std::memory_order_acquire))
        {
            exclusiveFlag.wait(true, std::memory_order_relaxed);
        }

        while (true) {
            int count = sharedCount.load(std::memory_order_acquire);
            if (count == 0)
                break;
            sharedCount.wait(count, std::memory_order_relaxed); // wait until it changes
        }
    }

    bool try_lock() noexcept
    {
        return !exclusiveFlag.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        exclusiveFlag.clear(std::memory_order_release);
        exclusiveFlag.notify_all();
    }

    void lock_shared()
    {
        while (true) {
            while (exclusiveFlag.test(std::memory_order_acquire)) {
                exclusiveFlag.wait(true, std::memory_order_relaxed);
            }

            sharedCount.fetch_add(1, std::memory_order_acquire);

            if (!exclusiveFlag.test(std::memory_order_acquire)) {
                break;
            }

            sharedCount.fetch_sub(1, std::memory_order_release);
        }
    }
    
    void unlock_shared() noexcept
    {
        int prev = sharedCount.fetch_sub(1, std::memory_order_release);
        if (prev == 1)
        {
            sharedCount.notify_all();
        }
    }

    bool try_lock_shared()
    {
        if (exclusiveFlag.test(std::memory_order_acquire)) {
            return false;
        }

        sharedCount.fetch_add(1, std::memory_order_acquire);

        if (exclusiveFlag.test(std::memory_order_acquire)) {
            sharedCount.fetch_sub(1, std::memory_order_release);
            return false;
        }

        return true;
    }
};

class Semaphore
{
public:
    explicit Semaphore(int c = 1) : count(c) {};

    void Wait();

    bool Wait(std::chrono::milliseconds timeout);
    
    void Notify();

private:
    int count;
    std::mutex lock;
    std::condition_variable cv;
};

class SPSC
{
public:
    explicit SPSC(bool c = false) : count(c) {};

    void Wait(); //both used by producer

    bool Wait(std::chrono::nanoseconds timeout);

    void Notify(); //used by consumer

private:
    bool count;
    std::mutex lock;
    std::condition_variable cv;
};

class SemaphoreGuard
{
public:
    SemaphoreGuard(Semaphore& _s) : sema(_s) { sema.Wait(); }
    ~SemaphoreGuard() { sema.Notify(); }

private:
    Semaphore& sema;
};

class ThreadManager
{
public:
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
