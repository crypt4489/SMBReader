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
#include <thread>
#include <vector>


std::mutex vec_mutex;
std::vector<int> vec;
std::vector<std::jthread> threadss;

/*
class ThreadObject
{
public:
    std::jthread thread;
    ThreadObject()
    {
        done.store(false);
    }

    template<class F, class... Args> 
    void Execute(F&& f, Args&&... args)
    {
        thread = std::jthread(std::forward(f),
        std::ref(done), std::forward(args)...);
    }
};

*/

class ThreadManager
{
public:
    static std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> threadsFlags;

    static bool CheckDone()
    {
        size_t sizeBefore = threadsFlags.size(); 
        threadsFlags.erase(std::remove_if(threadsFlags.begin(), 
                     threadsFlags.end(), 
                     [](auto &x) { return x.second->load(); }), 
                     threadsFlags.end());

        return sizeBefore == threadsFlags.size();
    }

    template<class F, class... Args> 
    static void AddThread(F&& f, Args&&... args)
    {
        auto df = std::make_shared<std::atomic<bool>>(false); 

        std::jthread thread = std::jthread{std::forward<F>(f), df, std::forward<Args>(args)...};  
        
        thread.detach();

        threadsFlags.push_back({std::move(thread), std::move(df)});
    }
};

std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> ThreadManager::threadsFlags;

void AddX(std::shared_ptr<std::atomic<bool>> flag, int x)
{
    std::unique_lock guard(vec_mutex);
    vec.push_back(x);
    flag->store(true);
    return;
}


int main()
{
    ThreadManager::AddThread(AddX, 5);
    int i = 50;

    std::this_thread::sleep_for(std::chrono::seconds(3));
    while(i--)
    {
        if (!ThreadManager::CheckDone() && i > 40) {
            std::cout << "Here\n";
            ThreadManager::AddThread(AddX, i);
            //std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::unique_lock guard{vec_mutex};
        for (int i = 0; i<vec.size(); i++)
        {
            std::cout << vec[i] << " ";
        }
        std::cout << "\n";
        guard.unlock();
       // std::jthread add6(AddX, 6);
    }
    return 0;
}