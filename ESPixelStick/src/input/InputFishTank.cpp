/*
* InputFishTank.cpp - Code to wrap ESPAsyncE131 for input
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
#include "../ESPixelStick.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "../utility/SaferStringConversion.hpp"
#include "InputFishTank.hpp"
#include "../network/NetworkMgr.hpp"

//-----------------------------------------------------------------------------
// Local Structure and Data Definitions
//-----------------------------------------------------------------------------
static fsftclouds_disabled  fsftclouds_disabled_imp;
static fsftwait_cloud_start fsftwait_cloud_start_imp;
static fsftwait_cloud_end   fsftwait_cloud_end_imp;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", 3600, 60000);

//-----------------------------------------------------------------------------
c_InputFishTank::c_InputFishTank (c_InputMgr::e_InputChannelIds NewInputChannelId,
                                          c_InputMgr::e_InputType       NewChannelType,
                                          uint32_t                        BufferSize) :
    c_InputCommon (NewInputChannelId, NewChannelType, BufferSize)
{
    // DEBUG_START;
    // set a default effect

    SetBufferInfo (BufferSize);

    // DEBUG_END;
} // c_InputFishTank


//-----------------------------------------------------------------------------
c_InputFishTank::c_InputFishTank () :
    c_InputCommon (c_InputMgr::e_InputChannelIds::InputPrimaryChannelId,
                   c_InputMgr::e_InputType::InputType_Effects, 0)
{
    // DEBUG_START;

    SetBufferInfo (0);

    // DEBUG_END;

} // c_InputFishTank

//-----------------------------------------------------------------------------
c_InputFishTank::~c_InputFishTank ()
{

} // ~c_InputFishTank

//-----------------------------------------------------------------------------
void c_InputFishTank::Begin ()
{
    // DEBUG_START;

    if (true == HasBeenInitialized)
    {
        return;
    }
    HasBeenInitialized = true;

    CurrentColorSet.red     = 0.0;
    CurrentColorSet.green   = 0.0;
    CurrentColorSet.blue    = 0.0;

    TargetColorSet    = CurrentColorSet;
    DeltaColorSet     = CurrentColorSet;

    fsftclouds_disabled_imp.Init (this);

    // DEBUG_END;
} // Begin

//-----------------------------------------------------------------------------
void c_InputFishTank::GetConfig (JsonObject& jsonConfig)
{
    // DEBUG_START;

    jsonConfig[FTSECONDS_TO_TARGET]            = (uint)SecondsToTarget;
    jsonConfig[FTMODE]                         = (uint)FishTankMode;
    jsonConfig[FTCLOUD_ENABLED]                = (uint)EnableClouds;
    jsonConfig[FTMIN_CLOUD_GAP]                = (uint)MinInterCloudGap;
    jsonConfig[FTMAX_CLOUD_GAP]                = (uint)MaxInterCloudGap;
    jsonConfig[FTMIN_CLOUD_LENGTH]             = (uint)MinCloudDuration;
    jsonConfig[FTMAX_CLOUD_LENGTH]             = (uint)MaxCloudDuration;
    jsonConfig[FTMIN_CLOUD_DENSITY]            = (uint)MinCloudDensityPercent;
    jsonConfig[FTMAX_CLOUD_DENSITY]            = (uint)MaxCloudDensityPercent;
    jsonConfig[FTMIN_CLOUD_SECONDS_TO_TARGET]  = (uint)CloudMinSecondsToTarget;
    jsonConfig[FTMAX_CLOUD_SECONDS_TO_TARGET]  = (uint)CloudMaxSecondsToTarget;
    jsonConfig[F("timeOffset")]                = TimeOffset;

    // JsonObject FishTankMgrModeOptions = jsonConfig.createNestedObject (FTMODE_TABLE);
    JsonArray FishTankMgrModeTable = jsonConfig.createNestedArray (FTMODE_TABLE);
    for (ColorSet color : ColorTargetTable)
    {
        JsonObject FishTankMgrColorData = FishTankMgrModeTable.createNestedObject ();

        FishTankMgrColorData[FTRED]         = (uint)color.red;
        FishTankMgrColorData[FTGREEN]       = (uint)color.green;
        FishTankMgrColorData[FTBLUE]        = (uint)color.blue;
        FishTankMgrColorData[FTMODE_NAME]   = color.name;
        FishTankMgrColorData[F("id")]       = color.id;
    }   // end build list of available colors

    JsonArray FishTankMgrTimeToColorDataArray = jsonConfig.createNestedArray (FTTIME_TO_COLOR);
    uint index = 0;
    for (FishTankModes mode : XlatTimeToTargetColor)
    {
        JsonObject FishTankMgrTimeToColorData = FishTankMgrTimeToColorDataArray.createNestedObject ();
        FishTankMgrTimeToColorData[F("id")] = index++;
        FishTankMgrTimeToColorData[F("mode")] = uint32_t(mode);
    }   // end build list of colors at specific times

    // DEBUG_END;

} // GetConfig

//-----------------------------------------------------------------------------
void c_InputFishTank::GetStatus (JsonObject& jsonStatus)
{
    // DEBUG_START;

    JsonObject Status = jsonStatus.createNestedObject (F ("FishTank"));
    Status[F("CurrentTime")] = timeClient.getFormattedTime();

    // DEBUG_END;

} // GetStatus

//-----------------------------------------------------------------------------
void c_InputFishTank::Process ()
{
    // DEBUG_START;

    // DEBUG_V (String ("HasBeenInitialized: ") + HasBeenInitialized);
    // DEBUG_V (String ("PixelCount: ") + PixelCount);

    do // once
    {
        if (!HasBeenInitialized)
        {
            // DEBUG_V("Not Initialized");
            break;
        }

        if(!TimeClientInitialized)
        {
            if(!NetworkMgr.IsConnected())
            {
                // DEBUG_V("Network is not up");
                break;
            }

            timeClient.begin();
            TimeClientInitialized = true;
        }

        timeClient.update();

        // DEBUG_V("are we fixed or cycleing?");
        if (FishTankModes::cycle == FishTankMode)
        {
            pCurrentFsmState->Poll (this);

            // can we move towards the target color.
            if (millis() >= StepTimerMS)
            {
                // move towards the target color.
                StepTimerMS += 20;
                CurrentColorSet.red     = UpdateColor (DeltaColorSet.red,   TargetColorSet.red,   CurrentColorSet.red);
                CurrentColorSet.green   = UpdateColor (DeltaColorSet.green, TargetColorSet.green, CurrentColorSet.green);
                CurrentColorSet.blue    = UpdateColor (DeltaColorSet.blue,  TargetColorSet.blue,  CurrentColorSet.blue);
                UpdateOutputBuffer(CurrentColorSet);

                // DEBUG_V(String ("TargetColorSet.red    = ") + TargetColorSet.red);
                // DEBUG_V(String ("TargetColorSet.green  = ") + TargetColorSet.green);
                // DEBUG_V(String ("TargetColorSet.blue   = ") + TargetColorSet.blue);

                // DEBUG_V(String ("CurrentColorSet.red   = ") + CurrentColorSet.red);
                // DEBUG_V(String ("CurrentColorSet.green = ") + CurrentColorSet.green);
                // DEBUG_V(String ("CurrentColorSet.blue  = ") + CurrentColorSet.blue);
            }
        }
        else
        {
            // DEBUG_V("fixed");
            // DEBUG_V(String("FishTankMode: ") + String(uint32_t(FishTankMode)));
            CurrentColorSet   = ColorTargetTable[FishTankMode];
            // DEBUG_V(String("ConfigHasChanged: ") + String(ConfigHasChanged));
            if(ConfigHasChanged)
            {
                // DEBUG_V("Update Output");
                TargetColorSet = CurrentColorSet;
                UpdateOutputBuffer(TargetColorSet);
                ConfigHasChanged = false;
            }
        }

        InputMgr.RestartBlankTimer (GetInputChannelId ());

    } while (false);

    // DEBUG_END;

} // process

//-----------------------------------------------------------------------------
void c_InputFishTank::SetBufferInfo (uint32_t BufferSize)
{
    // DEBUG_START;

    InputDataBufferSize = BufferSize;
    // DEBUG_V (String ("BufferSize: ") + String (BufferSize));

    // DEBUG_END;

} // SetBufferInfo

//-----------------------------------------------------------------------------
bool c_InputFishTank::SetConfig (ArduinoJson::JsonObject& jsonConfig)
{
    // DEBUG_START;

    // serializeJsonPretty(jsonConfig, Serial);

    SetBufferInfo (InputDataBufferSize);

    setFromJSON ( SecondsToTarget,          jsonConfig, FTSECONDS_TO_TARGET);
    uint temp = uint(FishTankMode);
    setFromJSON ( temp,                     jsonConfig, FTMODE);
    FishTankMode = FishTankModes(temp);
    setFromJSON ( EnableClouds,             jsonConfig, FTCLOUD_ENABLED);
    setFromJSON ( MinInterCloudGap,         jsonConfig, FTMIN_CLOUD_GAP);
    setFromJSON ( MaxInterCloudGap,         jsonConfig, FTMAX_CLOUD_GAP);
    setFromJSON ( MinCloudDuration,         jsonConfig, FTMIN_CLOUD_LENGTH);
    setFromJSON ( MaxCloudDuration,         jsonConfig, FTMAX_CLOUD_LENGTH);
    setFromJSON ( MinCloudDensityPercent,   jsonConfig, FTMIN_CLOUD_DENSITY);
    setFromJSON ( MaxCloudDensityPercent,   jsonConfig, FTMAX_CLOUD_DENSITY);
    setFromJSON ( CloudMinSecondsToTarget,  jsonConfig, FTMIN_CLOUD_SECONDS_TO_TARGET);
    setFromJSON ( CloudMaxSecondsToTarget,  jsonConfig, FTMAX_CLOUD_SECONDS_TO_TARGET);
    setFromJSON ( TimeOffset,               jsonConfig, F("timeOffset"));

    timeClient.setTimeOffset(TimeOffset);

    // get the time to color table?
    JsonArray JsonTimeToModeDataObject = jsonConfig[FTTIME_TO_COLOR];

    // String pretty;
    // JsonTimeToModeDataObject.prettyPrintTo (pretty);
    // DEBUG_V("Start of Parsed json data");
    // DEBUG_V(pretty.c_str ());
    // DEBUG_V("End of Parsed json data");

    for (auto currentTimeSlot : JsonTimeToModeDataObject)
    {
        uint32_t id = uint32_t(-1);
        setFromJSON ( id,  currentTimeSlot, F("id"));

        uint32_t TempMode = uint32_t(XlatTimeToTargetColor[id]);
        setFromJSON ( TempMode,  currentTimeSlot, F("mode"));
        XlatTimeToTargetColor[id] = FishTankModes(TempMode);

    }   // end build list of colors

    JsonArray FishTankMgrColorData = jsonConfig[FTMODE_TABLE];

    // FishTankMgrColorData.prettyPrintTo (pretty);
    // DEBUG_V("Start of Parsed json data");
    // DEBUG_V(pretty.c_str ());
    // DEBUG_V("End of Parsed json data");

    for (auto CurrentColorEntry : FishTankMgrColorData)
    {
        uint32_t id = uint32_t(-1);
        setFromJSON ( id,  CurrentColorEntry, F("id"));

        ColorSet color = ColorTargetTable[id];
        setFromJSON ( color.red,    CurrentColorEntry, FTRED);
        setFromJSON ( color.green,  CurrentColorEntry, FTGREEN);
        setFromJSON ( color.blue,   CurrentColorEntry, FTBLUE);
        setFromJSON ( color.name,   CurrentColorEntry, FTMODE_NAME);

        // save the data
        ColorTargetTable[id] = color;

    }   // end build list of colors

    // show that the config may have changed
    fsftclouds_disabled_imp.Init (this);

    randomSeed (analogRead (0));

    ConfigHasChanged = true;

    // DEBUG_END;

    return true;
} // SetConfig

/*****************************************************************************/
/*
  *	update the color to approach the target value.
  *
  *	needs
  *		amount to change by
  *		target value
  *		current value
  *	returns
  *		new current value
  */
double c_InputFishTank::UpdateColor (double changeValue, double targetValue, double currentValue)
{
    // DEBUG_START;
    // DEBUG_V(String ("targetValue  = ") + targetValue);
    // DEBUG_V(String ("currentValue = ") + currentValue);
    // DEBUG_V(String ("changeValue  = ") + changeValue);

    // abs function wants final numbers in local variables or it does strange things
    double diff = targetValue - currentValue;
    diff = abs (diff);
    double absChangeValue = abs (changeValue);

    // DEBUG_V(String ("changeValue = ") + changeValue);
    // DEBUG_V(String ("diff  = ") + diff);

    // is there enough headroom to make a change
    if (targetValue == currentValue)
    {
        // nothing to do. We have reached our target
        // DEBUG_V(F("==============Color at target ---------------"));
    }
    else if ((absChangeValue >= diff) || (0 == changeValue))
    {
        // we are close. Too close to apply the delta. Just set the color to the target
        currentValue = targetValue;
        // DEBUG_V(F("Updated a color to target"));
    }   // color needed to change
    else
    {
        // we are still more than 1 x delta away
        currentValue += changeValue;
        // DEBUG_V(F("Updated a color one step towards the target"));

        // check to make sure we are converging
        double diff2 = targetValue - currentValue;
        diff2 = abs (diff2);
        if (diff2 > diff)
        {
            // we are getting further apart. Time to fix to the target
            currentValue = targetValue;
            // DEBUG_V(F("Set current to target"));
        }
    }   // Not much else we can do

    // DEBUG_END;

    return currentValue;
}   // UpdateColor

/*****************************************************************************/
void c_InputFishTank::UpdateOutputBuffer(ColorSet & OutputColorSet)
{
    // DEBUG_START;

    uint32_t    ChannelsPerPixel = 3;
    uint8_t     PixelBuffer[ChannelsPerPixel];

    // DEBUG_V(String("   ChannelsPerPixel: ") + String(ChannelsPerPixel));
    // DEBUG_V(String("InputDataBufferSize: ") + String(InputDataBufferSize));

    PixelBuffer[0] = OutputColorSet.red;
    PixelBuffer[1] = OutputColorSet.green;
    PixelBuffer[2] = OutputColorSet.blue;

    for(uint32_t pTargetAddress = 0; pTargetAddress < InputDataBufferSize; pTargetAddress += ChannelsPerPixel)
    {
        // DEBUG_V(String("     pTargetAddress: ") + String(pTargetAddress));
        OutputMgr.WriteChannelData(pTargetAddress, ChannelsPerPixel, PixelBuffer);
    }

    // DEBUG_END;
}

/*****************************************************************************/
/*
  *	Check if it is time to update the target color and update as needed.
  *
  *	needs
  *		nothing
  *	returns
  *		nothing
  */
void c_InputFishTank::SetTimedColor (void)
{
    // DEBUG_START;

    // DateTime CurrentDateTime = g_RtcMgr.currentDateTime ();

    int hourNow = timeClient.getHours ();
    // DEBUG_V(String("hourNow: ") + String(hourNow))

    if (23 < hourNow)
    {
        // DEBUG_V(String ("Correcting unexpected hour value, hourNow = ") + hourNow);
        hourNow = 0;
    }

    // has the hour changed? aka do we need a new target color?
    if (hourNow != currentHour)
    {
        // DEBUG_V(String ("Timed Color Change, hourNow = ") + hourNow);
        // DEBUG_V(String("    hourNow: ") + String(hourNow))
        // DEBUG_V(String("currentHour: ") + String(currentHour))

        // get the target color
        currentHour = hourNow;

        // set the target color and the steps needed to reach it from the current color
        SetColors (XlatTimeToTargetColor[currentHour]);

        // DEBUG_V(String ("CurrentColorSet.red   = ") + CurrentColorSet.red);
        // DEBUG_V(String ("CurrentColorSet.green = ") + CurrentColorSet.green);
        // DEBUG_V(String ("CurrentColorSet.blue  = ") + CurrentColorSet.blue);
        // DEBUG_V(String ("TargetColorSet.red    = ") + TargetColorSet.red);
        // DEBUG_V(String ("TargetColorSet.green  = ") + TargetColorSet.green);
        // DEBUG_V(String ("TargetColorSet.blue   = ") + TargetColorSet.blue);
        // DEBUG_V(String ("DeltaColorSet.red     = ") + DeltaColorSet.red);
        // DEBUG_V(String ("DeltaColorSet.green   = ") + DeltaColorSet.green);
        // DEBUG_V(String ("DeltaColorSet.blue    = ") + DeltaColorSet.blue);
    }   // end time has changed

    // DEBUG_END;
}   // SetTimedColor

/*****************************************************************************/
void c_InputFishTank::SetColors (FishTankModes TargetColor)
{
    // DEBUG_START;

    // get the target color
    TargetColorSet = ColorTargetTable[TargetColor];
    // DEBUG_V(String ("TargetColorSet.red            = ") + TargetColorSet.red);
    // DEBUG_V(String ("TargetColorSet.green          = ") + TargetColorSet.green);
    // DEBUG_V(String ("TargetColorSet.blue           = ") + TargetColorSet.blue);

    // adjust for cloud density
    // DEBUG_V(String ("CurrentCloudDensity             = ") + CurrentCloudDensity);
    TargetColorSet.red      = TargetColorSet.red * CurrentCloudDensity;
    TargetColorSet.green    = TargetColorSet.green * CurrentCloudDensity;
    TargetColorSet.blue     = TargetColorSet.blue * CurrentCloudDensity;

    // DEBUG_V(String ("Adjusted TargetColorSet.red   = ") + TargetColorSet.red);
    // DEBUG_V(String ("Adjusted TargetColorSet.green = ") + TargetColorSet.green);
    // DEBUG_V(String ("Adjusted TargetColorSet.blue  = ") + TargetColorSet.blue);

    // DEBUG_V(String ("CurrentColorSet.red           = ") + CurrentColorSet.red);
    // DEBUG_V(String ("CurrentColorSet.green         = ") + CurrentColorSet.green);
    // DEBUG_V(String ("CurrentColorSet.blue          = ") + CurrentColorSet.blue);

    // Calculate steps to get to the target
    DeltaColorSet.red   = SetUpColorStep (TargetColorSet.red,   CurrentColorSet.red);
    DeltaColorSet.green = SetUpColorStep (TargetColorSet.green, CurrentColorSet.green);
    DeltaColorSet.blue  = SetUpColorStep (TargetColorSet.blue,  CurrentColorSet.blue);

    // DEBUG_V(String ("DeltaColorSet.red             = ") + DeltaColorSet.red);
    // DEBUG_V(String ("DeltaColorSet.green           = ") + DeltaColorSet.green);
    // DEBUG_V(String ("DeltaColorSet.blue            = ") + DeltaColorSet.blue);

    // DEBUG_END;
}   // SetColors

/*****************************************************************************/
/*
  *	update the expected output to the tank.
  *
  *	needs
  *		target color value
  *		current color value
  *	returns
  *		step towards target value
  */
double c_InputFishTank::SetUpColorStep (double targetValue, double currentValue)
{
    // DEBUG_START;

    double response         = 0.0;
    uint32_t StepsToTarget  = SecondsToTarget * FTSTEPS_PER_SEC;

    do  // once
    {
        double diff = targetValue - currentValue;
        if (0.0 == diff)
        {
            // at target already
            response = 0.0;
            break;
        }   // at target

        response = diff / StepsToTarget;
    } while (false);
    // DEBUG_END;

    return response;
}   // SetUpColorStep

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*
  *	Init:: Clouds are disabled
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Sets target to current time color. Resets steps to configured count.
  */
void fsftclouds_disabled::Init (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // make sure the target gets updated
    pParent->currentHour = 24;

    // update the output to the proper value
    pParent->CurrentCloudDensity = 1.0;
    pParent->SetTimedColor ();

    // set up the state machine
    pParent->pCurrentFsmState = & fsftclouds_disabled_imp;

    // DEBUG_END;
}   // fsftclouds_disabled::Init

/*****************************************************************************/
/*
  *	Init:: Clouds are disabled
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Sets target to current time color. Resets steps to configured count.
  */
void fsftclouds_disabled::Poll (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // update the color as needed
    pParent->SetTimedColor ();

    // are clouds still disabled?
    if (true == pParent->EnableClouds)
    {
        // clouds are enabled. Enter the cloud start state
        fsftwait_cloud_start_imp.Init (pParent);
    }   // end clouds have become enabled

    // DEBUG_END;
}   // fsftclouds_disabled::Init

/*****************************************************************************/
/*
  *	Init:: Calculate a delay before the start of the next cloud
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Sets target to current time color. Resets steps to configured count.
  */
void fsftwait_cloud_start::Init (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // calculate how long to wait until the start of the next cloud
    fsfttimer = time_t (random (long(pParent->MinInterCloudGap), long(pParent->MaxInterCloudGap)));
    // DEBUG_V(String ("fsfttimer = ") + pParent->fsfttimer);
    fsfttimer += timeClient.getEpochTime();

    // set up the state machine
    pParent->pCurrentFsmState = & fsftwait_cloud_start_imp;

    // DEBUG_END;
}   // fsftclouds_disabled::Init

/*****************************************************************************/
/*
  *	Poll::Sit and wait for the cloud to start
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Sets target to current time color. Resets steps to configured count.
  */
void fsftwait_cloud_start::Poll (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // update the color as needed
    pParent->SetTimedColor ();

    // are clouds disabled?
    if (false == pParent->EnableClouds)
    {
        // clouds are disabled. Enter the cloud disabled state
        fsftclouds_disabled_imp.Init (pParent);
    }   // end clouds are turned off

    // have we waited long enough?
    else if (fsfttimer < timeClient.getEpochTime ())
    {
        // start the cloud and wait for it to end
        fsftwait_cloud_end_imp.Init (pParent);
    }   // End turn on a cloud

    // DEBUG_END;
}   // fsftwait_cloud_start::Poll

/*****************************************************************************/
/*
  *	Init:: Calculate cloud density and duration. set the cloud target value.
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Sets target to current time color. Resets steps to configured count.
  */
void fsftwait_cloud_end::Init (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // calculate how long to wait until the start of the end of the cloud
    fsfttimer = time_t (random (long(pParent->MinCloudDuration), long(pParent->MaxCloudDuration)));
    // DEBUG_V(String ("fsfttimer = ") + pParent->fsfttimer);
    fsfttimer += timeClient.getEpochTime ();

    // set a cloud density value
    double TargetCloudDensity = double(random (long(pParent->MinCloudDensityPercent), long(pParent->MaxCloudDensityPercent)));
    // DEBUG_V(String ("TargetCloudDensity = ") + TargetCloudDensity);

    pParent->CurrentCloudDensity = 1.0 - ((TargetCloudDensity) / 100.0);

    // adjust the target color by the density
    time_t SavedSecondsToTarget = pParent->SecondsToTarget;
    pParent->SecondsToTarget = time_t (random (long(pParent->CloudMinSecondsToTarget), long(pParent->CloudMaxSecondsToTarget)));
    pParent->currentHour     = 24;
    pParent->SetTimedColor ();
    pParent->SecondsToTarget = SavedSecondsToTarget;

    // set up the state machine
    pParent->pCurrentFsmState = & fsftwait_cloud_end_imp;

    // DEBUG_END;
}   // fsftwait_cloud_end::Init

/*****************************************************************************/
/*
  *	Poll:: Waiting for the current cloud to end
  *
  *	needs
  *		pointer to the fish tank manager control structure
  *	returns
  *		nothing
  *	modifies
  *		Just sits and waits.
  */
void fsftwait_cloud_end::Poll (c_InputFishTank * pParent)
{
    // DEBUG_START;

    // update the color as needed
    time_t SavedSecondsToTarget = pParent->SecondsToTarget;
    pParent->SecondsToTarget = time_t (random (long(pParent->CloudMinSecondsToTarget), long(pParent->CloudMaxSecondsToTarget)));
    pParent->SetTimedColor ();
    pParent->SecondsToTarget = SavedSecondsToTarget;

    // have we waited long enough?
    if ((fsfttimer < timeClient.getEpochTime ()) ||
        (false == pParent->EnableClouds))
    {
        // Reset the target to the current hourly value
        fsftclouds_disabled_imp.Init (pParent);
    }   // End turn off a cloud

    // DEBUG_END;
}   // fsftwait_cloud_end::Poll
