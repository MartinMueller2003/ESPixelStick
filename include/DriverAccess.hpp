#pragma once

#ifdef ARDUINO_ARCH_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t DriverAccessMutex;
void InitializeDriverAccess();

// Config loads, input processing and output writes can run on different tasks.
// Recursive locking allows effects and config handlers to call manager methods.
class DriverAccessGuard
{
public:
    DriverAccessGuard() : Mutex(DriverAccessMutex)
    {
        if (Mutex) { xSemaphoreTakeRecursive(Mutex, portMAX_DELAY); }
    }
    ~DriverAccessGuard()
    {
        if (Mutex) { xSemaphoreGiveRecursive(Mutex); }
    }
    DriverAccessGuard(const DriverAccessGuard &) = delete;
    DriverAccessGuard &operator=(const DriverAccessGuard &) = delete;
private:
    SemaphoreHandle_t Mutex;
};
#else
inline void InitializeDriverAccess() {}
class DriverAccessGuard {};
#endif
