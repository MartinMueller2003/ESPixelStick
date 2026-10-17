#include "ESPixelStick.h"
#include "DriverAccess.hpp"

#ifdef ARDUINO_ARCH_ESP32
SemaphoreHandle_t DriverAccessMutex = nullptr;

void InitializeDriverAccess()
{
    // Called during setup, before input processing tasks are started.
    if (!DriverAccessMutex)
    {
        DriverAccessMutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(DriverAccessMutex);
    }
}
#endif
