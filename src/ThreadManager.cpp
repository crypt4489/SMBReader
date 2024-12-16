#include "ThreadManager.h"
std::vector<std::pair<std::jthread, std::shared_ptr<std::atomic<bool>>>> ThreadManager::threadsFlags;
std::vector<std::jthread>  ThreadManager::backgroundTasks;