#include "ThreadManager.h"
std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> ThreadManager::threadsFlags;
std::vector<std::jthread>  ThreadManager::backgroundTasks;

void ThreadManager::ASyncThreadsDone()
{
    if (threadsFlags.size()) {
        threadsFlags.erase(std::remove_if(threadsFlags.begin(),
            threadsFlags.end(),
            [](auto& x) { return x.second->load(); }),
            threadsFlags.end());
    }
}

void ThreadManager::DestroyThreadManager()
{
    threadsFlags.clear();

    backgroundTasks.clear();
}

void Semaphore::Wait()
{
    std::unique_lock guard(lock);
    cv.wait(guard, [this]() { return count > 0; });
    count--;
}

bool Semaphore::Wait(std::chrono::milliseconds timeout)
{
    std::unique_lock guard(lock);
    if (!cv.wait_for(guard, timeout, [this]() { return count > 0; }))
    {
        return false;
    }
    count--;
    return true;
}

void Semaphore::Notify()
{
    {
        std::unique_lock guard(lock);
        count++;
    }
    cv.notify_one();
}