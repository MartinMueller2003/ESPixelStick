#pragma once
/*
* OutputI2S.hpp - I2S driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#ifdef SUPPORT_I2S
#include "ESPixelStick.h"
#include "m_i2s.h"
#include "driver/gpio.h"

// a slot is one bit in the output item
#define I2S_NUM_SLOTA i2s_bits_per_chan_t::I2S_BITS_PER_CHAN_8BIT
#define I2S_MAX_NUM_PORTS 8
union I2S_item_t
{
    struct
    {
        uint8_t data;
        uint8_t reserved;
    };
    uint16_t RawData;
};

class c_OutputI2S
{
public:
struct OutputI2SChannelConfig_t
{
    uint32_t    I2SChannelId;
    gpio_num_t  DataPin;
    void        *arg;
    bool        (*GetNextIntensityBit) (void*arg, I2S_item_t * data, uint32_t numSlices) = nullptr;
    bool        IsActive;
};

private:

    const int   clock_pin = I2S_PIN_NO_CHANGE; // Pixel Clock
    bool        OutputIsPaused = false;
    double      I2S_TickTimeInNS;

    #define I2STargetBitSliceTimeNS 150

    uint32_t SetBitSliceLen (uint8_t ChanId, double BitSliceLenNs);
    void StartSendingData   ();
    void StopSendingData    ();
    void InitI2Sdriver      ();

#ifndef HasBeenInitialized
    bool HasBeenInitialized = false;
#endif // ndef HasBeenInitialized

    TaskHandle_t I2sTaskHandle = NULL;

public:
    c_OutputI2S ();
    virtual ~c_OutputI2S ();

    void        Begin               ();
    uint8_t     RegisterSlotDevice  (OutputI2SChannelConfig_t config);
    void        RemoveSlotDevice    (OutputI2SChannelConfig_t config);
    void        GetStatus           (ArduinoJson::JsonObject& jsonStatus);
    void        PauseOutput         (bool State);
    void        GetDriverName       (String &value)  { value = F("I2S"); }
    uint32_t    GetBitTimeSlices    (uint32_t BitTimeInNanoSec);
    void        SetOutputState      (uint32_t I2SChannelId, bool NewState);
    void        SetGpio             (uint32_t I2SChannelId, gpio_num_t NewGpio);

// #define USE_I2S_DEBUG_COUNTERS
#ifdef USE_I2S_DEBUG_COUNTERS
// #define IncludeBufferData
   // debug counters
    struct I2SDebugCounters_t
    {
        uint32_t DataTaskcounter = 0;
        uint32_t IntensityBitsSent = 0;
        uint32_t IntensityBitsSentLastFrame = 0;
        uint32_t I2SEntriesTransfered = 0;
        uint32_t I2SXmtFills = 0;
        uint32_t ISRpaused = 0;
        uint32_t WriteToBuffer = 0;
    } ;
    I2SDebugCounters_t I2SDebugCounters;

#define I2S_DEBUG_INC_COUNTER(p) (++I2SDebugCounters.p)
#define I2S_DEBUG_COUNTER(p)  (I2SDebugCounters.p)

#else

#define I2S_DEBUG_INC_COUNTER(p)
#define I2S_DEBUG_COUNTER(p)

#endif // def USE_I2S_DEBUG_COUNTERS

};
#endif // def #ifdef SUPPORT_I2S