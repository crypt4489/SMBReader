#pragma once
#include <atomic>
#include <stdint.h>
#include <stddef.h>

#ifdef _MSC_VER
#define ALIGNAS(x) __declspec(align(x))
#endif

struct MPMCQueueData
{
    std::atomic<size_t> currentSequence;
    int freeIndex;
};