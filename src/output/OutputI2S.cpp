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
#if defined(ARDUINO_ARCH_ESP32)

#include "output/OutputI2S.hpp"
#include "m_i2s.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "esp_rom_gpio.h"

#define I2S_DeviceID i2s_port_t::I2S_NUM_0
#define I2S_Device I2S0
#define I2S_DataStartSignal I2S0O_DATA_OUT8_IDX

struct I2S_ListOfRegisteredChannels_t
{
    void * pThis;
    void (*GetMoreDate)(void*, I2S_item_t*, uint32_t);
    bool IsActive;
};

static I2S_ListOfRegisteredChannels_t I2S_ListOfRegisteredChannels[I2S_NUM_SLOTA];
#define                               I2S_NumSendBufferItems 128
static I2S_item_t                     I2S_SendBuffer[I2S_NumSendBufferItems];

void FillBufferWithTestData(uint8_t testData)
{
    for (uint32_t i = 0; i < I2S_NumSendBufferItems; ++i)
    {
        I2S_SendBuffer[i].data = testData;
    }
}
//----------------------------------------------------------------------------
void I2S_Task(void*)
{
    size_t bytes_written = 0;
    DEBUG_V(String("Current CPU ID: ") + String(xPortGetCoreID()));

    int index = 0;
    while(index < I2S_NumSendBufferItems)
    {
        I2S_SendBuffer[index++].data = 0xFE;
        I2S_SendBuffer[index++].data = 0xFF;
    }
    while(true)
    {
        // test code to send data to the I2S driver
        // DEBUG_V("Sending data to I2S driver");
        FillBufferWithTestData(0xFF);
        m_i2s_write(I2S_DeviceID, I2S_SendBuffer, sizeof(I2S_SendBuffer), &bytes_written, portMAX_DELAY);
        FillBufferWithTestData(0xFE);
        m_i2s_write(I2S_DeviceID, I2S_SendBuffer, sizeof(I2S_SendBuffer), &bytes_written, portMAX_DELAY);
        // DEBUG_V(String("Bytes written: ") + String(bytes_written) + " bytes");

        // DEBUG_V("Processing all possible channels");
        for (I2S_ListOfRegisteredChannels_t & CurrentChannel : I2S_ListOfRegisteredChannels)
        {
            // DEBUG_V(String("Processing channel: 0x") + String(uint32_t(CurrentChannel.pThis), HEX));
            // do we have a driver on this channel?
            if(true == CurrentChannel.IsActive)
            {
                DEBUG_V(String("Getting more data for channel: 0x") + String(uint32_t(CurrentChannel.pThis), HEX));
                // digitalWrite(17, LOW);

                // invoke the channel
                CurrentChannel.GetMoreDate(CurrentChannel.pThis, I2S_SendBuffer, I2S_NumSendBufferItems);
                // send the data to the I2S driver
                size_t bytes_written = 0;
                m_i2s_write(I2S_DeviceID, &I2S_SendBuffer, I2S_NumSendBufferItems, &bytes_written, portMAX_DELAY);
                DEBUG_V(String("Bytes written: ") + String(bytes_written) + " bytes");
            }
        }
    }
}

//----------------------------------------------------------------------------
c_OutputI2S::c_OutputI2S()
{
    // DEBUG_START;

    for(auto &CurrGpio : GpioDataPins)
    {
        // DEBUG_V(String("Configuring GPIO: ") + String(CurrGpio));
        CurrGpio = gpio_num_t(-1);
    }

    for (I2S_ListOfRegisteredChannels_t & CurrentChannel : I2S_ListOfRegisteredChannels)
    {
        CurrentChannel.pThis = nullptr;
        CurrentChannel.GetMoreDate = nullptr;
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
            i2s_config.dma_buf_count = 4;
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
        // I2S_Device.conf2.val = 0;
        I2S_Device.conf2.lcd_en = true;
//        I2S_Device.conf2.data_enable = false;
        I2S_Device.conf2.camera_en = false;
//        I2S_Device.conf2.lcd_tx_sdx2_en = true;
//        I2S_Device.conf2.lcd_tx_wrx2_en = true;

        // Setup i2s clock
        // I2S_Device.sample_rate_conf.val = 0;
        // I2S_Device.sample_rate_conf.rx_bits_mod = i2s_bits_per_chan_t::I2S_NUM_SLOTA;
//        I2S_Device.sample_rate_conf.tx_bits_mod = i2s_bits_per_chan_t::I2S_NUM_SLOTA;
        // I2S_Device.sample_rate_conf.rx_bck_div_num = 1;
        I2S_Device.sample_rate_conf.tx_bck_div_num = 1;

        // I2S_Device.clkm_conf.val = 0;
        I2S_Device.clkm_conf.clka_en = false;
        I2S_Device.clkm_conf.clk_en = false; // Use PLL_D2_CLK (160MHz)
        I2S_Device.clkm_conf.clkm_div_num = 24; // 160MH / 800kh / 3 slots per bit
        I2S_Device.clkm_conf.clkm_div_a = 8; // Adjust based on source
        I2S_Device.clkm_conf.clkm_div_b = 7; // Adjust based on source
        I2S_Device.sample_rate_conf.tx_bck_div_num = 4;

        // I2S_Device.fifo_conf.val = 0;
        // Dictated by datasheet
        I2S_Device.fifo_conf.tx_fifo_mod_force_en = true;
        I2S_Device.fifo_conf.tx_fifo_mod = 2; // I2S_TX_CHAN_MODE_MONO;
//        I2S_Device.fifo_conf.tx_fifo_mod = 1; // twice
        // I2S_Device.fifo_conf.tx_data_num = 32; 
        // I2S_Device.fifo_conf.rx_data_num = 32; //Thresholds. 
        // I2S_Device.fifo_conf.rx_fifo_mod_force_en = 1;

        // I2S_Device.conf1.val = 0;
        I2S_Device.conf1.tx_stop_en = false;
        I2S_Device.conf1.tx_zeros_rm_en = false;
//        I2S_Device.conf1.tx_pcm_conf = 0;
        I2S_Device.conf1.tx_pcm_bypass = true;

        // I2S_Device.conf_chan.val = 0;
        // Tx in mono mode, read 32 bit per sample from fifo
//        I2S_Device.conf_chan.tx_chan_mod = 1; // Both slots transmit the left channel data.
        // I2S_Device.conf_chan.rx_chan_mod = 1;

        // DEBUG_V("3. Enable LCD mode specific register flags");
        I2S_Device.conf.val = 0;
//        I2S_Device.conf.tx_right_first = true;
//        I2S_Device.conf.tx_msb_right = false;
        I2S_Device.conf.tx_slave_mod = false;

        // 4. Clear standard serial pin allocations 

        // DEBUG_V("5. Route parallel data lines manually via GPIO matrix");
        // I2S0 data output signals start at I2S_DataStartSignal 
        int GpioTableIndex = 0;
        for (auto gpioValue : GpioDataPins)
        {
            // DEBUG_V(String("Configuring GPIO pin: ") + String(GpioDataPins[i]));
            m_gpio_matrix_out_check_and_set(gpioValue, I2S_DataStartSignal + GpioTableIndex, false, false);
            GpioTableIndex++;
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
        GpioDataPins[0] = gpio_num_t(0);
        GpioDataPins[1] = gpio_num_t(1);
        GpioDataPins[2] = gpio_num_t(2);
        GpioDataPins[3] = gpio_num_t(3);
        GpioDataPins[4] = gpio_num_t(4);
        GpioDataPins[5] = gpio_num_t(5);
        GpioDataPins[6] = gpio_num_t(12);
        GpioDataPins[7] = gpio_num_t(13);

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
void IRAM_ATTR c_OutputI2S::ISR_ProcessDma ()
{

} // ISR_ProcessDma

#endif // defined(ARDUINO_ARCH_ESP32)
