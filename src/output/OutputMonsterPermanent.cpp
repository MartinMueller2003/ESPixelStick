/*
* OutputMonsterPermanent.cpp - MonsterPermanent driver code for ESPixelStick UART
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
#include "output/OutputMonsterPermanent.hpp"
#if defined(SUPPORT_OutputProtocol_MonsterPermanent)

//----------------------------------------------------------------------------
c_OutputMonsterPermanent::c_OutputMonsterPermanent (OM_OutputPortDefinition_t & OutputPortDefinition,
                                c_OutputMgr::e_OutputProtocolType outputType) :
    c_OutputPixel (OutputPortDefinition, outputType)
{
    // DEBUG_START;

    InterFrameGapInMicroSec = MonsterPermanent_PIXEL_IDLE_TIME_US;
    EnableMonsterPermanentRgb();

    // DEBUG_END;
} // c_OutputMonsterPermanent

//----------------------------------------------------------------------------
c_OutputMonsterPermanent::~c_OutputMonsterPermanent ()
{
    // DEBUG_START;

    // DEBUG_END;
} // ~c_OutputMonsterPermanent

//----------------------------------------------------------------------------
void c_OutputMonsterPermanent::Begin ()
{
    // DEBUG_START;

    c_OutputPixel::Begin ();
    HasBeenInitialized = true;

    // DEBUG_END;
} // Begin

//----------------------------------------------------------------------------
void c_OutputMonsterPermanent::GetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    c_OutputPixel::GetConfig (jsonConfig);

    // DEBUG_END;
} // GetConfig

//----------------------------------------------------------------------------
void c_OutputMonsterPermanent::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;
    c_OutputPixel::GetStatus (jsonStatus);

    // jsonStatus["canRefresh"] = canRefresh();
    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
/* Process the config
*
*   needs
*       reference to string to process
*   returns
*       true - config has been accepted
*       false - Config rejected. Using defaults for invalid settings
*/
bool c_OutputMonsterPermanent::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // Keep the original software's three-channel RGB input.
    jsonConfig[CN_color_order] = "rgb";
    uint32_t gap = MonsterPermanent_PIXEL_IDLE_TIME_US;
    setFromJSON(gap, jsonConfig, CN_interframetime);
    jsonConfig[CN_interframetime] = max(uint32_t(MonsterPermanent_PIXEL_IDLE_TIME_US), min(gap, uint32_t(10000)));
    bool response = c_OutputPixel::SetConfig (jsonConfig);

    // Calculate our refresh time

    // DEBUG_V(String("   MonsterPermanent_PIXEL_DATA_RATE: ") + String(MonsterPermanent_PIXEL_DATA_RATE));
    // DEBUG_V(String("MonsterPermanent_PIXEL_NS_BIT_TOTAL: ") + String(MonsterPermanent_PIXEL_NS_BIT_TOTAL));
    SetFrameDurration(float(MonsterPermanent_PIXEL_NS_BIT_TOTAL) / float(NanoSecondsInAMicroSecond));

    // DEBUG_END;
    return response;

} // SetConfig
#endif // defined(SUPPORT_OutputProtocol_MonsterPermanent)
