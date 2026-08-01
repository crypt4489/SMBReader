#pragma once
#include "OS.h"

void OSTimeInitialize();
int64_t OSGetSystemTicks();
double OSGetTimeSeconds(int64_t tickCounter);
double OSGetCurrentTimeSeconds();

