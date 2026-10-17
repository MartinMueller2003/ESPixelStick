#include "ESPixelStick.h"
#include "WatchdogProgress.hpp"

WatchdogProgress_t WatchdogProgress;

#ifdef ARDUINO_ARCH_ESP32
#include <rom/ets_sys.h>

// The task-watchdog's weak hook runs before panic/backtrace generation.
// Use ROM output and DRAM data; do not allocate, lock, or call Serial here.
static DRAM_ATTR char ProgressFormat[] =
    "\n[WDT progress] input=%u channel=%u type=%u effect=%u stackFree=%u\n";

extern "C" void IRAM_ATTR esp_task_wdt_isr_user_handler(void)
{
    ets_printf(ProgressFormat, unsigned(WatchdogProgress.input),
               unsigned(WatchdogProgress.channel), unsigned(WatchdogProgress.type),
               unsigned(WatchdogProgress.effect), unsigned(WatchdogProgress.stackFree));
}
#endif
