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


void Semaphore::Create(int c)
{
    CreateOSSemaphore(&semaphore, c);
}

void Semaphore::Wait()
{
    int ret = WaitOSSemaphore(&semaphore, OS_INFINITE_TIMEOUT);
}

bool Semaphore::Wait(int timeout)
{
    int ret = WaitOSSemaphore(&semaphore, timeout);

    return (ret ? false : true);
}

void Semaphore::Notify()
{
    int ret = NotifyOSSemaphore(&semaphore);
}

Semaphore::~Semaphore()
{
    DeleteOSSemaphore(&semaphore);
}


void SharedExclusiveFlag::lock() noexcept
{
    ExclusiveAcquireOSSharedExclusive(&osse);
}

bool SharedExclusiveFlag::try_lock() noexcept
{
    int ret = TryExclusiveAcquireOSSharedExclusive(&osse);

    return (ret ? false : true);
}

void SharedExclusiveFlag::unlock() noexcept
{
    ExclusiveReleaseOSSharedExclusive(&osse);
}

void SharedExclusiveFlag::lock_shared()
{
    SharedAcquireOSSharedExclusive(&osse);
}

void SharedExclusiveFlag::unlock_shared() noexcept
{
    SharedReleaseOSSharedExclusive(&osse);
}

bool SharedExclusiveFlag::try_lock_shared()
{
    int ret = TrySharedAcquireOSSharedExclusive(&osse);

    return (ret ? false : true);
}