/*
* OutputWS2811I2S.cpp - WS2811 driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2026 Shelby Merrick
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
#include "ESPixelStick.h"
#if defined(SUPPORT_OutputProtocol_WS2811) && defined(SUPPORT_I2S)

#include "output/OutputWS2811I2S.hpp"
#include "output/OutputMgr.hpp"

//----------------------------------------------------------------------------
static bool GetNextBitToSendBase (void * arg, I2S_item_t * DataToSend, uint32_t numSlices)
{
    return reinterpret_cast<c_OutputWS2811I2S*>(arg)->GetNextBitToSend(DataToSend, numSlices);
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
c_OutputWS2811I2S::c_OutputWS2811I2S (OM_OutputPortDefinition_t & OutputPortDefinition,
                                      c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputWS2811 (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_0_H: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_0_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_0_L: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_0_LOW,  HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_1_H: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_1_HIGH, HEX));
    // DEBUG_V (String ("WS2811_PIXEL_I2S_TICKS_BIT_1_L: 0x") + String (WS2811_PIXEL_I2S_TICKS_BIT_1_LOW,  HEX));

    // DEBUG_END;

} // c_OutputWS2811I2S

//----------------------------------------------------------------------------
c_OutputWS2811I2S::~c_OutputWS2811I2S ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputWS2811I2S

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::Begin ()
{
    // DEBUG_START;

    I2Sdriver = static_cast<c_OutputI2S*>(OutputMgr.GetI2sDriver());

    c_OutputWS2811::Begin ();

    // DEBUG_V (String ("DataPin: ") + String (DataPin));

    ZeroHighBitCount = I2Sdriver->GetBitTimeSlices(WS2811_PIXEL_NS_BIT_0_HIGH);
    ZeroLowBitCount  = I2Sdriver->GetBitTimeSlices(WS2811_PIXEL_NS_BIT_0_LOW);
    OneHighBitCount  = I2Sdriver->GetBitTimeSlices(WS2811_PIXEL_NS_BIT_1_HIGH);
    OneLowBitCount   = I2Sdriver->GetBitTimeSlices(WS2811_PIXEL_NS_BIT_1_LOW);

    DEBUG_V(String("ZeroHighBitCount: ") + String(ZeroHighBitCount));
    DEBUG_V(String(" ZeroLowBitCount: ") + String(ZeroLowBitCount));
    DEBUG_V(String(" OneHighBitCount: ") + String(OneHighBitCount));
    DEBUG_V(String("  OneLowBitCount: ") + String(OneLowBitCount));

    IfgBitCount         = 0;
    IfgBitCurrentCount  = 0;
    HighBitCurrentCount = 0;
    LowBitCurrentCount  = 0;

    DataBit = 0; // no data bit assigned yet
    DataBitMask = ~DataBit;

    c_OutputI2S::OutputI2SChannelConfig_t OutputI2SConfig;
    OutputI2SConfig.I2SChannelId            = uint32_t(OutputPortDefinition.PortId);
    OutputI2SConfig.DataPin                 = gpio_num_t(OutputPortDefinition.gpios.data);
    OutputI2SConfig.arg                     = this;
    OutputI2SConfig.GetNextIntensityBit     = GetNextBitToSendBase;
    OutputI2SConfig.IsActive                = false; // do not output data

    DataBit = I2Sdriver->RegisterSlotDevice(OutputI2SConfig);
    DataBitMask = ~ DataBit;

    // DEBUG_V(String("    DataBit: 0x") + String(DataBit, HEX));
    // DEBUG_V(String("DataBitMask: 0x") + String(DataBitMask, HEX));

    HasBeenInitialized = true;

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
bool c_OutputWS2811I2S::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputWS2811::SetConfig (jsonConfig);

    uint32_t SavedIfgBitCount = IfgBitCount;
    IfgBitCount = I2Sdriver->GetBitTimeSlices(InterFrameGapInMicroSec * NanoSecondsInAMicroSecond);
    // DEBUG_V(String("     IfgBitCount: ") + String(IfgBitCount));

    // update the output GPIO
    I2Sdriver->SetGpio(OutputPortDefinition.PortId, OutputPortDefinition.gpios.data);

    if(!SavedIfgBitCount)
    {
        // get all of the slice counters set up for the first frame
        StartNewDataFrame();

        // start the transmiter
        I2Sdriver->SetOutputState(OutputPortDefinition.PortId, true);
    }

    // DEBUG_V();

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::SetOutputBufferSize (uint32_t NumChannelsAvailable)
{
    // DEBUG_START;

    c_OutputWS2811::SetOutputBufferSize (NumChannelsAvailable);

    // DEBUG_END;

} // SetBufferSize

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputWS2811::GetStatus (jsonStatus);
    I2Sdriver->GetStatus (jsonStatus);

    #ifdef USE_I2S_DEBUG_COUNTERS
    jsonStatus[F("FrameDurationInMicroSec")] = FrameDurationInMicroSec;
    jsonStatus[F("FrameStartTimeInMicroSec")] = GetFrameStartTimeInMicroSec();
    uint32_t now = micros();
    jsonStatus[F("Now")] = now;
    jsonStatus[F("FrameStartDelta")] = now - GetFrameStartTimeInMicroSec();
    #endif // def USE_I2S_DEBUG_COUNTERS

    #ifdef WS2811_I2S_DEBUG_COUNTERS
    JsonObject JsonCounters = jsonStatus["JsonCounters"].to<JsonObject>();
    JsonWrite(JsonCounters, "GetNextBit",  I2SDebugCounters.GetNextBit);
    JsonWrite(JsonCounters, "FrameStarts", I2SDebugCounters.FrameStarts);
    JsonWrite(JsonCounters, "FrameEnds",   I2SDebugCounters.FrameEnds);
    JsonWrite(JsonCounters, "IfgBits",     I2SDebugCounters.IfgBits);
    JsonWrite(JsonCounters, "DataBits",    I2SDebugCounters.DataBits);
    JsonWrite(JsonCounters, "DataBytes",   I2SDebugCounters.DataBytes);
    JsonWrite(JsonCounters, "BitHigh",     I2SDebugCounters.BitHigh);
    JsonWrite(JsonCounters, "BitLow",      I2SDebugCounters.BitLow);
    #endif // def WS2811_I2S_DEBUG_COUNTERS

    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputWS2811I2S::Poll ()
{
    // DEBUG_START;

    // DEBUG_END;
    return ActualFrameDurationMicroSec;

} // Poll

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::StartNewDataFrame()
{
    // DEBUG_START;

    StartNewFrame();

    // DEBUG_V(String("frame started on ") + String(OutputPortDefinition.gpios.data));
    INC_WS2811_I2S_DEBUG_COUNTERS(FrameStarts);
    IfgBitCurrentCount = IfgBitCount;

    // set up for the next data byte
    INC_WS2811_I2S_DEBUG_COUNTERS(DataBytes);
    c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
    DataPatternMask = 0x80;

    if(DataPattern & DataPatternMask)
    {
        // send a one bit
        HighBitCurrentCount = OneHighBitCount;
        LowBitCurrentCount  = OneLowBitCount;
    }
    else // send a zero bit
    {
        HighBitCurrentCount = ZeroHighBitCount;
        LowBitCurrentCount  = ZeroLowBitCount;
    }

    // DEBUG_END;
} // StartNewDataFrame

//----------------------------------------------------------------------------
bool c_OutputWS2811I2S::GetNextBitToSend (I2S_item_t * pDataToSend, uint32_t numSlices)
{
    INC_WS2811_I2S_DEBUG_COUNTERS(GetNextBit);
    bool Response = true;

    for(int CurrentSliceId = 0; CurrentSliceId < numSlices; ++CurrentSliceId, ++pDataToSend)
    {
        if (IfgBitCurrentCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS(IfgBits);
            pDataToSend->data &= DataBitMask; // output low
            --IfgBitCurrentCount;
            continue;
        }

        INC_WS2811_I2S_DEBUG_COUNTERS(DataBits);
        if(HighBitCurrentCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS(BitHigh);

            --HighBitCurrentCount;
            pDataToSend->data |= DataBit; // output high
            continue;
        }

        if(LowBitCurrentCount)
        {
            INC_WS2811_I2S_DEBUG_COUNTERS(BitLow);
            pDataToSend->data &= DataBitMask; // output low
            --LowBitCurrentCount;
            if(LowBitCurrentCount)
            {
                // more low bits to send
                continue;
            }
        }

        // bit data has completed
        DataPatternMask = DataPatternMask >> 1;
        // do we need to set up the next data byte to send?
        if(0 == DataPatternMask)
        {
            if(c_OutputPixel::ISR_MoreDataToSend())
            {
                INC_WS2811_I2S_DEBUG_COUNTERS(DataBytes);
                c_OutputPixel::ISR_GetNextIntensityToSend(DataPattern);
                DataPatternMask = 0x80;
                if(DataPattern & DataPatternMask)
                {
                    HighBitCurrentCount = OneHighBitCount;
                    LowBitCurrentCount  = OneLowBitCount;
                }
                else
                {
                    HighBitCurrentCount = ZeroHighBitCount;
                    LowBitCurrentCount  = ZeroLowBitCount;
                }
            }
            else
            {
                INC_WS2811_I2S_DEBUG_COUNTERS(FrameEnds);
                StartNewDataFrame();
            }
            continue;
        }
        // more bits to send in the current data byte
        INC_WS2811_I2S_DEBUG_COUNTERS(DataBits);
        if(DataPattern & DataPatternMask)
        {
            // send a one bit
            HighBitCurrentCount = OneHighBitCount;
            LowBitCurrentCount  = OneLowBitCount;
        }
        else // send a zero bit
        {
            HighBitCurrentCount = ZeroHighBitCount;
            LowBitCurrentCount  = ZeroLowBitCount;
        }
    };

    return Response;
} // ISR_GetNextBitToSend

//----------------------------------------------------------------------------
void c_OutputWS2811I2S::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputWS2811::PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputProtocol_WS2811) && defined(ARDUINO_ARCH_ESP32)
