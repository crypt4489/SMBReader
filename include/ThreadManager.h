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

class Semaphore
{
public:
    explicit Semaphore(int c = 1) : count(c) {};

    void Wait()
    {
        std::unique_lock guard(lock);
        cv.wait(guard, [this]() { return count > 0; });
        count--;
    }

    bool Wait(std::chrono::milliseconds timeout)
    {
        std::unique_lock guard(lock);
        if (!cv.wait_for(guard, timeout, [this]() { return count > 0; }))
        {
            return false;
        }
        count--;
        return true;
    }

    void Notify()
    {
        {
            std::unique_lock guard(lock);
            count++;
        }
        cv.notify_one();
    }


private:
    int count;
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

    static bool ASyncThreadsDone()
    {
        size_t sizeBefore = threadsFlags.size();
        threadsFlags.erase(std::remove_if(threadsFlags.begin(),
            threadsFlags.end(),
            [](auto& x) { return x.second->load(); }),
            threadsFlags.end());
        return sizeBefore == threadsFlags.size();
    }

    static void DestroyThreadManager()
    {
        threadsFlags.clear();

        backgroundTasks.clear();
    }

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
