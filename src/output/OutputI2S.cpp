/*
* OutputI2S.cpp - driver code for ESPixelStick I2S Channel
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2026, 2026 Shelby Merrick
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
#if defined(SUPPORT_I2S)

#include "output/OutputI2S.hpp"
#include "m_i2s.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "esp_rom_gpio.h"
#include <numeric>

#define I2S_DeviceID i2s_port_t::I2S_NUM_0
#define I2S_Device I2S0
#define I2S_DataStartSignal I2S0O_DATA_OUT8_IDX

static c_OutputI2S::OutputI2SChannelConfig_t OutputI2SSlotConfigs[I2S_MAX_NUM_PORTS];
#define                         I2S_NumSendBufferItems 512
static I2S_item_t               I2S_SendBuffer[I2S_NumSendBufferItems];

//----------------------------------------------------------------------------
void I2S_Task(void*)
{
    size_t bytes_written = 0;
    DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));

    while(true)
    {
        bool FoundActiveChannel = false;
        // DEBUG_V("Processing all possible channels");
        for (auto & CurrentChannel : OutputI2SSlotConfigs)
        {
            // DEBUG_V(String("Processing channel: 0x") + String(uint32_t(CurrentChannel.pThis), HEX));
            // do we have a driver on this channel?
            if(true == CurrentChannel.IsActive)
            {
                FoundActiveChannel = true;

                // DEBUG_V(String("Getting more data for channel: 0x") + String(uint32_t(CurrentChannel.arg), HEX));

                // invoke the channel
                CurrentChannel.GetNextIntensityBit(CurrentChannel.arg, I2S_SendBuffer, I2S_NumSendBufferItems);
            }
        }

        if(FoundActiveChannel)
        {
            // send the data to the I2S driver
            size_t bytes_written = 0;
            m_i2s_write(I2S_DeviceID, I2S_SendBuffer, sizeof(I2S_SendBuffer), &bytes_written, portMAX_DELAY);
            // DEBUG_V(String("Bytes written: ") + String(bytes_written) + " bytes");
        }
        else
        {
            // DEBUG_V("No active channels, pausing task");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }
} // I2S_Task

//----------------------------------------------------------------------------
c_OutputI2S::c_OutputI2S()
{
    // DEBUG_START;

    int id = 0;
    for (auto & CurrentChannel : OutputI2SSlotConfigs)
    {
        CurrentChannel.I2SChannelId = id++;
        CurrentChannel.DataPin = gpio_num_t(I2S_PIN_NO_CHANGE);
        CurrentChannel.arg = nullptr;
        CurrentChannel.GetNextIntensityBit = nullptr;
        CurrentChannel.IsActive = false;
    }

    // DEBUG_END;
} // c_OutputI2S

//----------------------------------------------------------------------------
c_OutputI2S::~c_OutputI2S ()
{
    // DEBUG_START;

    if (HasBeenInitialized)
    {

    }

    // DEBUG_END;
} // ~c_OutputI2S

//----------------------------------------------------------------------------
static uint32_t gcd(uint32_t a, uint32_t b)
{
    while (b != 0)
    {
        uint32_t temp = b;
        b = a % b;
        a = temp;
    }
  return a;
}

//----------------------------------------------------------------------------
void c_OutputI2S::InitI2Sdriver ()
{
        // 1. Configure the core I2S settings
        i2s_config_t i2s_config;
            i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
            i2s_config.sample_rate = 10; // This is over written later
            i2s_config.bits_per_chan = i2s_bits_per_chan_t::I2S_BITS_PER_CHAN_DEFAULT; // follow bits_per_sample
            i2s_config.bits_per_sample = i2s_bits_per_sample_t::I2S_BITS_PER_SAMPLE_16BIT;
            i2s_config.channel_format = i2s_channel_fmt_t::I2S_CHANNEL_FMT_RIGHT_LEFT;
            i2s_config.communication_format = i2s_comm_format_t::I2S_COMM_FORMAT_STAND_I2S;
            i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
            i2s_config.dma_buf_count = 2;
            i2s_config.dma_buf_len = sizeof(I2S_SendBuffer);
            i2s_config.use_apll = false;
            i2s_config.fixed_mclk = 0;
            i2s_config.tx_desc_auto_clear = true;

        // DEBUG_V(String("Install driver onto I2S Port ") + String(I2S_DeviceID));
        if(ESP_OK != m_i2s_driver_install(I2S_DeviceID, &i2s_config, 0, NULL))
        {
            logcon(F("Failed to install I2S driver"));
        }

        //DEBUG_V("Stop I2S Driver");
        m_i2s_stop(I2S_DeviceID);

        // DEBUG_V("Set clock pins");
        i2s_pin_config_t pin_config =
        {
            .mck_io_num = I2S_PIN_NO_CHANGE,
            .bck_io_num = I2S_PIN_NO_CHANGE,
            .ws_io_num = I2S_PIN_NO_CHANGE,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = I2S_PIN_NO_CHANGE
        };
        if(ESP_OK != m_i2s_set_pin(I2S_DeviceID, &pin_config))
        {
            logcon(F("Failed to set I2S pin configuration"));
        }

        // DEBUG_V("Set i2s mode to LCD mode");
        I2S_Device.conf2.lcd_en = true;
        I2S_Device.conf2.camera_en = false;

        double ClockFrequency = 80000000.0;
        double TargetTickNS = I2STargetBitSliceTimeNS;
        double ClkNS = (1.0 / ClockFrequency) * 1000000000;
        double ClkDiv = TargetTickNS / ClkNS;
        double integralPart;
        double fractionalPart = std::modf(ClkDiv, &integralPart);
        fractionalPart = std::abs(fractionalPart);
        double denominator = std::pow(10,9);
        double numerator = std::round(fractionalPart * denominator);
        double commonDivisor = gcd(numerator, denominator);
        numerator = numerator / commonDivisor;
        denominator = denominator / commonDivisor;
        I2S_TickTimeInNS = TargetTickNS;

        if(int(denominator) == 0 || int(numerator) == 0)
        {
            denominator = 0.0;
            numerator = 0.0;
        }
        // Setup i2s clock
        I2S_Device.clkm_conf.clka_en = false;
        I2S_Device.clkm_conf.clk_en = false; // Use PLL_D2_CLK (160MHz)
        I2S_Device.clkm_conf.clkm_div_num = int(ClkDiv);
        I2S_Device.clkm_conf.clkm_div_a = int(denominator);
        I2S_Device.clkm_conf.clkm_div_b = int(numerator);
        I2S_Device.sample_rate_conf.tx_bck_div_num = 4;
        I2S_Device.sample_rate_conf.tx_bits_mod = i2s_bits_per_chan_t::I2S_BITS_PER_CHAN_8BIT;

        // DEBUG_V(String("  ClockFrequency: ") + String(ClockFrequency));
        // DEBUG_V(String("           ClkNS: ") + String(ClkNS, 10));
        // DEBUG_V(String("          ClkDiv: ") + String(ClkDiv, 10));
        // DEBUG_V(String("    TargetTickNS: ") + String(TargetTickNS, 10));
        // DEBUG_V(String("  fractionalPart: ") + String(fractionalPart, 10));
        // DEBUG_V(String("     denominator: ") + String(denominator, 10));
        // DEBUG_V(String("       numerator: ") + String(numerator, 10));
        // DEBUG_V(String("   commonDivisor: ") + String(commonDivisor, 10));
        // DEBUG_V(String("    clkm_div_num: ") + String(I2S_Device.clkm_conf.clkm_div_num));
        // DEBUG_V(String("      clkm_div_b: ") + String(I2S_Device.clkm_conf.clkm_div_b));
        // DEBUG_V(String("      clkm_div_a: ") + String(I2S_Device.clkm_conf.clkm_div_a));

        // Dictated by datasheet
        I2S_Device.fifo_conf.tx_fifo_mod_force_en = true;
        I2S_Device.fifo_conf.tx_fifo_mod = 2; // I2S_TX_CHAN_MODE_MONO;

        I2S_Device.conf1.tx_stop_en = false;
        I2S_Device.conf1.tx_zeros_rm_en = false;
        I2S_Device.conf1.tx_pcm_conf = 0;
        I2S_Device.conf1.tx_pcm_bypass = true;

        // DEBUG_V("3. Enable LCD mode specific register flags");
        I2S_Device.conf.val = 0;
        I2S_Device.conf.tx_slave_mod = false;

        // DEBUG_V("4. disconnect all of the I2S connections from the matrix");
        for (auto currentConfig : OutputI2SSlotConfigs)
        {
            m_gpio_matrix_out_check_and_set(currentConfig.DataPin, I2S_DataStartSignal + currentConfig.I2SChannelId, false, false);
        }

        // set the reserved bits to 1 to avoid sending garbage data
        memset(&I2S_SendBuffer, 0xFF, sizeof(I2S_SendBuffer));

        // DEBUG_V("Start the peripheral clocking");
        if(ESP_OK != m_i2s_start(I2S_DeviceID))
        {
            logcon(F("Failed to start I2S peripheral"));
        }

} // InitI2Sdriver

//----------------------------------------------------------------------------
void c_OutputI2S::Begin ()
{
    // DEBUG_START;

    do // once
    {
        InitI2Sdriver();

        // DEBUG_V();
        xTaskCreatePinnedToCore(I2S_Task, "I2S_Task", 4096, NULL, 5, &I2sTaskHandle, 1);
        vTaskPrioritySet(I2sTaskHandle, 5);

        HasBeenInitialized = true;
    } while (false);

    // DEBUG_END;

} // Begin

//----------------------------------------------------------------------------
void c_OutputI2S::GetStatus (ArduinoJson::JsonObject& jsonStatus)
{
    // // DEBUG_START;

#ifdef USE_I2S_DEBUG_COUNTERS
    jsonStatus[F("OutputIsPaused")] = OutputIsPaused;
    JsonObject debugStatus = jsonStatus["I2S Debug"].to<JsonObject>();
    debugStatus["I2SChannelId"]                 = OutputI2SConfig.I2SChannelId;
    debugStatus["GPIO"]                         = OutputI2SConfig.DataPin;

    debugStatus["IntensityBitsSent"]            = I2S_DEBUG_COUNTER(IntensityBitsSent);
    debugStatus["I2SEntriesTransfered"]         = I2S_DEBUG_COUNTER(I2SEntriesTransfered);
    debugStatus["I2SXmtFills"]                  = I2S_DEBUG_COUNTER(I2SXmtFills);
#endif // def USE_I2S_DEBUG_COUNTERS
    // // DEBUG_END;
} // GetStatus

//----------------------------------------------------------------------------
uint32_t c_OutputI2S::SetBitSliceLen (uint8_t ChanId, double BitSliceLenNs)
{
    // DEBUG_START;
    uint32_t Result = 0;

    // DEBUG_END;
    return Result;
} // SetBitDuration

//----------------------------------------------------------------------------
void c_OutputI2S::PauseOutput(bool PauseOutput)
{
    /// DEBUG_START;

    if (OutputIsPaused == PauseOutput)
    {
        ///DEBUG_V("no change. Ignore the call");
    }
    else if (PauseOutput)
    {
        ///DEBUG_V("stop the output");
        // Stop the transmitter
        m_i2s_stop(I2S_DeviceID);
    }

    OutputIsPaused = PauseOutput;

    ///DEBUG_END;
} // PauseOutput

//----------------------------------------------------------------------------
void c_OutputI2S::StopSendingData ()
{
    // DEBUG_START;

    do // once
    {
    } while(false);

    // DEBUG_END;
} // StopSendingData

//----------------------------------------------------------------------------
void c_OutputI2S::StartSendingData ()
{
    // DEBUG_START;

    do // once
    {
        if(OutputIsPaused)
        {
            // DEBUG_V("Paused");
            break;
        }

        // DEBUG_V();

    } while(false);

    // DEBUG_END;
} // StartSendingData

//----------------------------------------------------------------------------
uint8_t c_OutputI2S::RegisterSlotDevice  (OutputI2SChannelConfig_t config)
{
    // DEBUG_START;

    uint8_t DataBit = 0;

    do // once
    {
        if (config.I2SChannelId >= I2S_MAX_NUM_PORTS)
        {
            logcon(F("Invalid I2S channel ID"));
            break;
        }
        if (nullptr == config.GetNextIntensityBit)
        {
            logcon(F("Invalid GetNextIntensityBit function pointer"));
            break;
        }

        auto & currentConfig = OutputI2SSlotConfigs[config.I2SChannelId];
        currentConfig.arg = config.arg;
        currentConfig.GetNextIntensityBit = config.GetNextIntensityBit;
        currentConfig.IsActive = config.IsActive;
        
        DataBit = 1 << config.I2SChannelId;

        SetGpio(config.I2SChannelId, config.DataPin);

    } while(false);

    // DEBUG_END;
    return DataBit;
} // RegisterSlotDevice

//----------------------------------------------------------------------------
void c_OutputI2S::SetGpio (uint32_t I2SChannelId, gpio_num_t NewGpio)
{
    // DEBUG_START;

    do // once
    {
        if (I2SChannelId >= I2S_MAX_NUM_PORTS)
        {
            logcon(F("ERROR: Invalid I2S channel ID"));
            break;
        }

        // DEBUG_V(String("I2SChannelId: ") + String(I2SChannelId));
        // DEBUG_V(String("     NewGpio: ") + String(NewGpio));
        // DEBUG_V(String("     DataPin: ") + String(OutputI2SSlotConfigs[I2SChannelId].DataPin));

        if(NewGpio == OutputI2SSlotConfigs[I2SChannelId].DataPin)
        {
            // DEBUG_V("GPIO did not change. Ignore the request");
            break;
        }

        if(I2S_PIN_NO_CHANGE != OutputI2SSlotConfigs[I2SChannelId].DataPin)
        {
            // DEBUG_V("release the old GPIO from the matrix");
            ResetGpio(OutputI2SSlotConfigs[I2SChannelId].DataPin);
        }

        // DEBUG_V("attach the new GPIO");
        OutputI2SSlotConfigs[I2SChannelId].DataPin = NewGpio;
        m_gpio_matrix_out_check_and_set(NewGpio, I2S_DataStartSignal + I2SChannelId, false, false);

    } while(false);

    // DEBUG_END;
} // SetGpio

//----------------------------------------------------------------------------
void c_OutputI2S::SetOutputState (uint32_t I2SChannelId, bool NewState)
{
    DEBUG_START;

    OutputI2SSlotConfigs[I2SChannelId].IsActive = NewState;

    DEBUG_END;
} // SetOutputState

//----------------------------------------------------------------------------
uint32_t c_OutputI2S::GetBitTimeSlices    (uint32_t BitTimeInNanoSec)
{
    // DEBUG_START;

    uint32_t Result = 0;

    do // once
    {
        if (BitTimeInNanoSec < 200)
        {
            logcon(F("Invalid bit time"));
            break;
        }

        // round up
        Result = ((BitTimeInNanoSec + int(I2S_TickTimeInNS)) - 1) / int(I2S_TickTimeInNS);

    } while(false);

    // DEBUG_END;
    return Result;
}

#endif // defined(SUPPORT_I2S)
