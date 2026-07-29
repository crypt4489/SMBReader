#pragma once
#include <stdint.h>

void OSTimeInitialize();
int64_t OSGetSystemTicks();
double OSGetTimeSeconds(int64_t tickCounter);
double OSGetCurrentTimeSeconds();

