#pragma once

#include <stdint.h>

struct WatchdogProgress_t
{
    volatile uint32_t input = 0;
    volatile uint32_t channel = 0;
    volatile uint32_t type = 0;
    volatile uint32_t effect = 0;
    volatile uint32_t stackFree = 0;
};

extern WatchdogProgress_t WatchdogProgress;
