#pragma once
/*
* OutputSerialI2S.h - Serial driver code for ESPixelStick I2S Channel
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
*   This is a derived class that converts data in the output buffer into
*   pixel intensities and then transmits them through the configured serial
*   interface.
*
*/
#include "ESPixelStick.h"
#if (defined(SUPPORT_OutputProtocol_FireGod) || defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(SUPPORT_I2S)

#include "OutputSerial.hpp"
#include "OutputI2S.hpp"

class c_OutputSerialI2S : public c_OutputSerial
{
public:
    // These functions are inherited from c_OutputCommon
    c_OutputSerialI2S (OM_OutputPortDefinition_t & OutputPortDefinition,
        c_OutputMgr::e_OutputProtocolType outputType);
    virtual ~c_OutputSerialI2S ();

    // functions to be provided by the derived class
    // functions to be provided by the derived class
    void    Begin ();                                        ///< set up the operating environment based on the current config (or defaults)
    bool    SetConfig (ArduinoJson::JsonObject& jsonConfig); ///< Set a new config in the driver
    uint32_t Poll () {return 0;}                             ///< Call from loop (),  renders output data
    void    GetStatus (ArduinoJson::JsonObject& jsonStatus);
    void    SetOutputBufferSize (uint32_t NumChannelsAvailable);
    void    PauseOutput (bool State);
    bool    ISR_GetNextDataSlicesToSend (c_OutputI2S::I2S_Item_t * DataToSend, uint32_t & numSlices, c_OutputI2S::I2S_Item_t DefaultIntensitySliceData);
    void    ISR_StartNewDataFrame ();
    void    SetDefaultDataValue (c_OutputI2S::I2S_Item_t DefaultIntensitySliceData);

private:
    void    SetUpI2SBitTimes();

    c_OutputI2S     *I2Sdriver;

    union DataAsUint_t
    {
        c_OutputI2S::I2S_Item_t DataAsBytes[4];
        uint32_t AsUint;
    };

    DataAsUint_t    OneBit;
    DataAsUint_t    ZeroBit;
    DataAsUint_t    idle_level;

    uint32_t        DataPattern;
    uint32_t        DataPatternMask;

    uint32_t        BreakBitCount = 0;
    uint32_t        MabBitCount = 0;
    uint32_t        StartBitCount = 0;
    uint32_t        StopBitCount = 0;

    // #define SERIAL_I2S_DEBUG_COUNTERS
    #ifdef SERIAL_I2S_DEBUG_COUNTERS
    #define INC_SERIAL_I2S_DEBUG_COUNTERS(c) (++SerialI2SDebugCounters.c)
    struct
    {
        uint32_t GetNextBit = 0;
        uint32_t FrameStarts = 0;
        uint32_t FrameEnds = 0;
        uint32_t BreakBits = 0;
        uint32_t MabBits = 0;
        uint32_t StartBits = 0;
        uint32_t DataBits = 0;
        uint32_t StopBits = 0;
        uint32_t Underrun = 0;
    } SerialI2SDebugCounters;
    #else
    #define INC_SERIAL_I2S_DEBUG_COUNTERS(c)
    #endif // def SERIAL_I2S_DEBUG_COUNTERS

}; // c_OutputSerialI2S

#endif // defined(SUPPORT_OutputProtocol_FireGod) || (defined(SUPPORT_OutputProtocol_DMX) || defined(SUPPORT_OutputProtocol_Serial) || defined(SUPPORT_OutputProtocol_Renard)) && defined(SUPPORT_I2S)
