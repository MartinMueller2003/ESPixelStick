#pragma once
/*
* InputFishTank.cpp - Input Management class
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

#include "InputCommon.hpp"

class fsftstate;

class c_InputFishTank : public c_InputCommon
{
public:
    c_InputFishTank (c_InputMgr::e_InputChannelIds NewInputChannelId,
                         c_InputMgr::e_InputType       NewChannelType,
                         uint32_t                        BufferSize);
    virtual ~c_InputFishTank ();

    c_InputFishTank ();

    // functions to be provided by the derived class
    void Begin ();                             ///< set up the operating environment based on the current config (or defaults)
    bool SetConfig (JsonObject& jsonConfig);   ///< Set a new config in the driver
    void GetConfig (JsonObject& jsonConfig);   ///< Get the current config used by the driver
    void GetStatus (JsonObject& jsonStatus);
    void Process ();                           ///< Call from loop(),  renders Input data
    void GetDriverName (String  & sDriverName) { sDriverName = "Fish Tank"; } ///< get the name for the instantiated driver
    void SetBufferInfo (uint32_t BufferSize);

private:

    bool    HasBeenInitialized      = false;
    bool    TimeClientInitialized   = false;
    int32_t TimeOffset              = 0;
    bool    ConfigHasChanged        = false;

    // strings used for config management
    #define FishTankMGR_JSON_ROOT           F ("FishTank")
    #define FTSTEPS_TO_TARGET             F ("stepsToTarget")
    #define FTSECONDS_TO_TARGET           F ("TimeToTarget")
    #define FTMODE                        F ("mode")
    #define FTCLOUD_ENABLED               F ("cloudsEnabled")
    #define FTMIN_CLOUD_GAP               F ("minCloudGap")
    #define FTMAX_CLOUD_GAP               F ("maxCloudGap")
    #define FTMIN_CLOUD_LENGTH            F ("minCloudLength")
    #define FTMAX_CLOUD_LENGTH            F ("maxCloudLength")
    #define FTMIN_CLOUD_DENSITY           F ("minCloudDensity")
    #define FTMAX_CLOUD_DENSITY           F ("maxCloudDensity")
    #define FTCLOUD_STEPS_TO_TARGET       F ("cloudStepsToTarget")
    #define FTMIN_CLOUD_SECONDS_TO_TARGET F ("minCloudSecToTarget")
    #define FTMAX_CLOUD_SECONDS_TO_TARGET F ("maxCloudSecToTarget")
    #define FTRED                         F ("red")
    #define FTGREEN                       F ("green")
    #define FTBLUE                        F ("blue")
    #define FTMODE_NAME                   F ("name")
    #define FTMODE_LIST                   F ("modeList")
    #define FTMODE_TABLE                  F ("modeTable")
    #define FTTIME_TO_COLOR               F ("timeToColor")
    #define FTSTEPS_PER_SEC               50
    #define FTSTEP_PERIOD_MS              (uint32_t)((1.0 / FTSTEPS_PER_SEC) * 1000.0)

    typedef enum e_FishTankModes
    {
        clean = 0,
        sunrise,
        daytime,
        sunset,
        nightime,
        cycle,
        MustBeLast
    } FishTankModes;

    typedef struct s_ColorSet
    {
        double  red;
        double  green;
        double  blue;
        String  name;
        uint32_t id;
    } ColorSet;

    // return an adjusted value for the target color
    double  UpdateColor (double changeValue, double targetValue, double currentValue);
    void    SetTimedColor (void);
    void    SetColors (FishTankModes NewColorSet);
    double  SetUpColorStep (double targetValue, double currentValue);
    void    UpdateOutputBuffer(ColorSet & OutputColorSet);

    // current mode the lights are running in
    FishTankModes FishTankMode = FishTankModes::cycle;

    ColorSet CurrentColorSet;
    ColorSet TargetColorSet;
    ColorSet DeltaColorSet;   // Per step inc / dec values

    // number of steps to take to move from one color to another
    time_t SecondsToTarget = 5;

    // current hour
    int currentHour = -1;

    // flag to trigger the pixel driver reconfig
    uint32_t StepTimerMS    = 0;

    // create the definition for the color targets
    ColorSet ColorTargetTable[FishTankModes::MustBeLast] =  /* does not include cycle 'cycle' */
    {
        // this array must be in the same order as FishTankModes
        {255.0, 255.0, 255.0, "Clean",      FishTankModes::clean},
        {255.0, 100.0, 000.0, "Sunrise",    FishTankModes::sunrise},
        {200.0, 190.0, 190.0, "Daytime",    FishTankModes::daytime},
        {175.0, 100.0,  20.0, "Sunset",     FishTankModes::sunset},
        {000.0,  50.0, 255.0, "Night",      FishTankModes::nightime},
        {000.0, 000.0, 000.0, "Cycle",      FishTankModes::cycle}
    };

    // translate an "hour" into a target color
    FishTankModes XlatTimeToTargetColor[24] =
    {
        FishTankModes::nightime,    // 00::00
        FishTankModes::nightime,    // 01::00
        FishTankModes::nightime,    // 02::00
        FishTankModes::nightime,    // 03::00
        FishTankModes::nightime,    // 04::00
        FishTankModes::nightime,    // 05::00
        FishTankModes::nightime,    // 06::00
        FishTankModes::sunrise,     // 07::00
        FishTankModes::daytime,     // 08::00
        FishTankModes::daytime,     // 09::00
        FishTankModes::daytime,     // 10::00
        FishTankModes::daytime,     // 11::00
        FishTankModes::daytime,     // 12::00
        FishTankModes::daytime,     // 13::00
        FishTankModes::daytime,     // 14::00
        FishTankModes::daytime,     // 15::00
        FishTankModes::daytime,     // 16::00
        FishTankModes::daytime,     // 17::00
        FishTankModes::daytime,     // 18::00
        FishTankModes::daytime,     // 19::00
        FishTankModes::sunset,      // 20::00
        FishTankModes::nightime,    // 21::00
        FishTankModes::nightime,    // 22::00
        FishTankModes::nightime,    // 23::00
    };

    // do we have clouds
    bool EnableClouds = false;

    // number of seconds between clouds
    time_t MinInterCloudGap   = 90;
    time_t MaxInterCloudGap   = 900;

    // num seconds the cloud will be present
    time_t MinCloudDuration   = 10;
    time_t MaxCloudDuration   = 90;

    // impact of the cloud on the target color
    uint8_t MinCloudDensityPercent    = 20;
    uint8_t MaxCloudDensityPercent    = 75;
    double CurrentCloudDensity        = 1.0;  // no cloud
    time_t CloudMinSecondsToTarget    = 5;
    time_t CloudMaxSecondsToTarget    = 10;

    friend class fsftstate;
    friend class fsftclouds_disabled;
    friend class fsftwait_cloud_start;
    friend class fsftwait_cloud_end;

    // cloud support
    fsftstate * pCurrentFsmState  = nullptr;

    friend void StepTimeoutHandler ();
};

/*****************************************************************************/
/*
  *  Generic fsm base class.
  */
/*****************************************************************************/
class fsftstate
{
public:
    virtual void    Poll (c_InputFishTank * pParent) = 0;
    virtual void    Init (c_InputFishTank * pParent) = 0;
protected:
    uint32_t fsfttimer = 0;
};  // fsftstate

/*****************************************************************************/
// Clouds Disabled
class fsftclouds_disabled : public fsftstate
{
public:
    virtual void    Poll (c_InputFishTank * pParent);
    virtual void    Init (c_InputFishTank * pParent);
};  // fsftclouds_disabled

/*****************************************************************************/
// Wait for a cloud to start
class fsftwait_cloud_start : public fsftstate
{
public:
    virtual void    Poll (c_InputFishTank * pParent);
    virtual void    Init (c_InputFishTank * pParent);
};  // fsftwait_cloud_start

/*****************************************************************************/
// Wait for a cloud to end
class fsftwait_cloud_end : public fsftstate
{
public:
    virtual void    Poll (c_InputFishTank * pParent);
    virtual void    Init (c_InputFishTank * pParent);
};  // fsftwait_cloud_end
