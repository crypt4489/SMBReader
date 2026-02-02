#include "ThreadManager.h"
void ThreadManager::ASyncThreadsDone()
{
  
}

void ThreadManager::DestroyThreadManager()
{

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