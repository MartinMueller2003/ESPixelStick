/*
* MonsterPermanentUart.cpp - MonsterPermanent driver code for ESPixelStick UART
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2015, 2022 Shelby Merrick
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
#if defined(SUPPORT_OutputProtocol_MonsterPermanent) && defined(SUPPORT_UART)

#include "output/OutputMonsterPermanentUart.hpp"

#define MonsterPermanent_PIXEL_UART_BITS_PER_PIXEL_BIT 9
#define MonsterPermanent_PIXEL_UART_BAUDRATE (MonsterPermanent_PIXEL_DATA_RATE * MonsterPermanent_PIXEL_UART_BITS_PER_PIXEL_BIT)

/* Inverted 7N1: one NRZ bit per UART character, 9 ticks at 7.2 Mbaud.
 * Quantized HIGH times: 416.7 ns (0), 833.3 ns (1); period 1250 ns.
 * UART start/stop bits form part of the waveform; data is sent MSB first.
 */
static const c_OutputUart::ConvertIntensityToUartDataStreamEntry_t ConvertIntensityToUartDataStream[] =
{
    {0b01111100, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_00_ID},
    {0b01100000, c_OutputUart::UartDataBitTranslationId_t::Uart_DATA_BIT_01_ID},
    {0, c_OutputUart::UartDataBitTranslationId_t::Uart_LIST_END}
};

//----------------------------------------------------------------------------
c_OutputMonsterPermanentUart::c_OutputMonsterPermanentUart (OM_OutputPortDefinition_t & OutputPortDefinition,
                                        c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputMonsterPermanent (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    // DEBUG_END;
} // c_OutputMonsterPermanentUart

//----------------------------------------------------------------------------
c_OutputMonsterPermanentUart::~c_OutputMonsterPermanentUart ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputMonsterPermanentUart

//----------------------------------------------------------------------------
void c_OutputMonsterPermanentUart::Begin ()
{
    // DEBUG_START;

    c_OutputMonsterPermanent::Begin();
    // DEBUG_V();

    // DEBUG_V(String("MonsterPermanent_PIXEL_UART_BAUDRATE: ") + String(MonsterPermanent_PIXEL_UART_BAUDRATE));

    SetIntensityBitTimeInUS(float(MonsterPermanent_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    c_OutputUart::OutputUartConfig_t OutputUartConfig;
    OutputUartConfig.OutputPortId           = OutputPortDefinition.PortId;
    OutputUartConfig.UartId                 = uart_port_t(OutputPortDefinition.DeviceId);
    OutputUartConfig.DataPin                = OutputPortDefinition.gpios.data;
    OutputUartConfig.IntensityDataWidth     = MonsterPermanent_PIXEL_BITS_PER_INTENSITY;
    OutputUartConfig.UartDataSize           = c_OutputUart::UartDatauint32_t::OUTPUT_UART_7N1;
    OutputUartConfig.TranslateIntensityData = c_OutputUart::TranslateIntensityData_t::OneToOne;
    OutputUartConfig.pPixelDataSource       = this;
    OutputUartConfig.Baudrate               = MonsterPermanent_PIXEL_UART_BAUDRATE;
    OutputUartConfig.InvertOutputPolarity   = true;
    OutputUartConfig.CitudsArray            = ConvertIntensityToUartDataStream;
    Uart.Begin(OutputUartConfig);

    HasBeenInitialized = true;

    // DEBUG_END;
} // init

//----------------------------------------------------------------------------
bool c_OutputMonsterPermanentUart::SetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    bool response = c_OutputMonsterPermanent::SetConfig(jsonConfig);
    response |= Uart.SetConfig(jsonConfig);

    // DEBUG_END;
    return response;

} // SetConfig

//----------------------------------------------------------------------------
void c_OutputMonsterPermanentUart::GetConfig(ArduinoJson::JsonObject &jsonConfig)
{
    // DEBUG_START;

    c_OutputMonsterPermanent::GetConfig(jsonConfig);
    Uart.GetConfig(jsonConfig);

    // DEBUG_END;

} // GetConfig

//----------------------------------------------------------------------------
void c_OutputMonsterPermanentUart::GetStatus(ArduinoJson::JsonObject &jsonStatus)
{
    // DEBUG_START;

    c_OutputMonsterPermanent::GetStatus(jsonStatus);
    Uart.GetStatus(jsonStatus);

#ifdef MonsterPermanent_UART_DEBUG_COUNTERS
    JsonObject debugStatus = jsonStatus["MonsterPermanent UART Debug"].to<JsonObject>();
    debugStatus["NewFrameCounter"]        = NewFrameCounter;
    debugStatus["TimeSinceLastFrameMS"]   = TimeSinceLastFrameMS;
    debugStatus["TimeLastFrameStartedMS"] = TimeLastFrameStartedMS;
#endif // def MonsterPermanent_UART_DEBUG_COUNTERS

    // DEBUG_END;

} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputMonsterPermanentUart::Poll ()
{
    // DEBUG_START;
    uint32_t FrameLen = ActualFrameDurationMicroSec;

    do // Once
    {
        if (gpio_num_t(-1) == OutputPortDefinition.gpios.data)
        {
            FrameLen = 0;
            break;
        }

        if (!canRefresh())
        {
            FrameLen = 0;
            break;
        }

        // DEBUG_V("get the next frame started");
#ifdef MonsterPermanent_UART_DEBUG_COUNTERS
        NewFrameCounter++;
        TimeSinceLastFrameMS = millis() - TimeLastFrameStartedMS;
        TimeLastFrameStartedMS = millis();
#endif // def MonsterPermanent_UART_DEBUG_COUNTERS

        Uart.StartNewFrame();

        // DEBUG_V();

    } while (false);

    // DEBUG_END;

    return FrameLen;
} // render

//----------------------------------------------------------------------------
void c_OutputMonsterPermanentUart::PauseOutput (bool State)
{
    // DEBUG_START;

    c_OutputMonsterPermanent::PauseOutput(State);
    Uart.PauseOutput(State);

    // DEBUG_END;
} // PauseOutput

#endif // defined(SUPPORT_OutputProtocol_MonsterPermanent) && defined(SUPPORT_UART)
