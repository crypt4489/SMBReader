#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <syncstream>
#include <thread>
#include <vector>


std::mutex vec_mutex;
std::vector<int> vec;

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
                     [](auto &x) { return x.second->load(); }), 
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

        std::jthread thread = std::jthread{std::forward<F>(f), df, std::forward<Args>(args)...};  

        threadsFlags.push_back({std::move(thread), std::move(df)});
    }

    template<class F, class... Args> 
    static void LaunchBackgroundThread(F&& f, Args&&... args)
    {
        std::jthread thread = std::jthread{std::forward<F>(f), std::forward<Args>(args)...};

        backgroundTasks.push_back(std::move(thread));
    }
};

std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> ThreadManager::threadsFlags;
std::vector<std::jthread> ThreadManager::backgroundTasks;

void AddX(std::shared_ptr<std::atomic<bool>> flag, int x)
{
    std::unique_lock guard(vec_mutex);
    vec.push_back(x);
    flag->store(true);
    return;
}

void PrintHello(std::stop_token stoken, int i)
{
    while(!stoken.stop_requested()) {
        std::osyncstream(std::cout) << "Hello " << i << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


int main()
{
    ThreadManager::LaunchASyncThread(AddX, 5);
    int i = 50;
    ThreadManager::LaunchBackgroundThread(PrintHello, 10);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    while(i--)
    {
        if (!ThreadManager::ASyncThreadsDone() && i > 40) {
            ThreadManager::LaunchASyncThread(AddX, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::unique_lock guard{vec_mutex};
        for (int i = 0; i<vec.size(); i++)
        {
            std::osyncstream(std::cout) << vec[i] << " ";
        }
        std::osyncstream(std::cout) << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        guard.unlock();
    }
    ThreadManager::DestroyThreadManager();
    return 0;
}