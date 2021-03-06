# ESPixelStick Firmware

[![Join the chat at https://gitter.im/forkineye/ESPixelStick](https://badges.gitter.im/forkineye/ESPixelStick.svg)](https://gitter.im/forkineye/ESPixelStick)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://paypal.me/ShelbyMerrick)
[![Build Status](https://travis-ci.org/forkineye/ESPixelStick.svg?branch=master)](https://travis-ci.org/forkineye/ESPixelStick)

This is the Arduino firmware for the ESP8266 and ESP32 based ESPixelStick.  The ESPixelStick is a small wireless E1.31 sACN pixel controller designed to control a single strand of pixels.  Pixel limitations are mostly based upon your desired refresh rate, around 680 pixels (4 universes) for a 25ms E1.31 source rate.  MQTT support is provided as well for integration into home automation systems where an E1.31 source may not be present.

Since this project began, the firmware has moved beyond just pixel support for those with other ESP8266/ESP32 based devices.  The ESPixelStick firmware now supports outputting E1.31 streams to serial links as well.  Note this is not supported on the ESPixelStick hardware, but intended for other ESP8266/ESP32 devices such as Bill's RenardESP or the LoLin ESP based devices.

## Hardware

Being open source, you are free to use the ESPixelStick firmware on the device of your choice.  The code however is written specifically for the [ESPixelStick](http://forkineye.com/espixelstick). The ESPixelStick V2 utilizes an ESP-01 module and provides high current connectors, fusing, power filtering, a programming interface and proper logic level buffering.  If you're in the US and would like to purchase an ESPixelStick, they are available via [Amazon](http://amzn.to/2uqBFuX).  The proceeds go towards things like keeping my wife happy so I can work on this project :)
This code has been ported to work on an ESP32 based LoLin mini. This requires the user to add their own buffer for the WS281x output.
## Requirements

Along with the Arduino IDE, you'll need the following software to build this project:

- [Adruino for ESP8266](https://github.com/esp8266/Arduino) - Arduino core for ESP8266
- [Arduino ESP8266 Filesystem Uploader](https://github.com/esp8266/arduino-esp8266fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [Adruino for ESP32](https://github.com/espressif/arduino-esp32) - Arduino core for ESP32
- [Arduino ESP32 Filesystem Uploader](https://github.com/me-no-dev/arduino-esp32fs-plugin) - Arduino plugin for uploading files to SPIFFS
- [gulp](http://gulpjs.com/) - Build system required to process web sources.  Refer to the html [README](html/README.md) for more information.

The following libraries are required:

Extract the folder in each of these zip files and place it in the "library" folder under your arduino environment

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncE131](https://github.com/forkineye/ESPAsyncE131) - Asynchronous E1.31 (sACN) library
- [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Asynchronous TCP Library
- [ESPAsyncUDP](https://github.com/me-no-dev/ESPAsyncUDP) - Asynchronous UDP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library
- [async-mqtt-client](https://github.com/marvinroger/async-mqtt-client) - Asynchronous MQTT Client

The ESP32 build will require the following software to build this project:

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) - Arduino JSON Library
- [ESPAsyncE131](https://github.com/forkineye/ESPAsyncE131) - Asynchronous E1.31 (sACN) library
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) - Asynchronous TCP Library
- [ESPAsyncUDP](https://github.com/me-no-dev/ESPAsyncUDP) - Asynchronous UDP Library
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Asynchronous Web Server Library
- [async-mqtt-client](https://github.com/marvinroger/async-mqtt-client) - Asynchronous MQTT Client

## Important Notes on Compiling and Flashing

- In order to upload your code to the ESP8266 you must put it in flash mode and then take it out of flash mode to run the code. To place your ESP8266 in flash mode your GPIO-0 pin must be connected to ground.
- Device mode is now a compile time option to set your device type and is configured in the top of the main sketch file.  Current options are ```ESPS_MODE_PIXEL``` and ```ESPS_MODE_SERIAL```.  The default is ```ESPS_MODE_PIXEL``` for the ESPixelStick hardware.
- Web pages **must** be processed, placed into ```data/www```, and uploaded with the upload plugin. Gulp will process the pages and put them in ```data/www``` for you. Refer to the html [README](html/README.md) for more information.
- In order to use the upload plugin, the ESP8266 **must** be placed into programming mode and the Arduino serial monitor **must** be closed.
- ESP-01 modules **must** be configured for 1M flash and 128k SPIFFS within the Arduino IDE for OTA updates to work.
- For best performance, set the CPU frequency to 160MHz (Tools->CPU Frequency).  You may experience lag and other issues if running at 80MHz.
- The upload must be redone each time after you rebuild and upload the software

## Supported Outputs

The ESPixelStick firmware can generate the following outputs from incoming E1.31 streams, however your hardware must support the physical interface.

### Pixel Protocols

- WS2811 / WS2812 / WS2812b
- GE Color Effects

### Serial Protocols

- DMX512
- Renard

## MQTT Support

MQTT can be configured via the web interface.  When enabled, a payload of "ON" will tell the ESPixelStick to override any incoming E1.31 data with MQTT data.  When a payload of "OFF" is received, E1.31 processing will resume.  The configured topic is used for state, and the command topic will be the state topic appended with ```/set```.

For example, if you enter ```porch/esps``` as the topic, the state can be queried from ```porch/esps``` and commands can be sent to ```porch/esps/set```

If using [Home Assistant](https://home-assistant.io/), it is recommended to enable Home Assistant Discovery in the MQTT configuration.  Your ESPixelStick along with all effects will be automatically imported as an entity within Home Assistant utilzing "Device ID" as the friendly name.  For manual configuration, you can use the following as an example.  When disabling Home Assistant Discovery, ESPixelStick will attempt to remove its configuration entry from your MQTTT broker.

```yaml
light:
  - platform: mqtt
    schema: json
    name: "Front Porch ESPixelStick"
    state_topic: "porch/esps"
    command_topic: "porch/esps/set"
    brightness: true
    rgb: true
    effect: true
    effect_list:
      - Solid
      - Blink
      - Flash
      - Rainbow
      - Chase
      - Fire flicker
      - Lightning
      - Breathe
```

Here's an example using the mosquitto_pub command line tool:

```bash
mosquitto_pub -t porch/esps/set -m '{"state":"ON","color":{"r":255,"g":128,"b":64},"brightness":255,"effect":"solid","reverse":false,"mirror":false}'
```

## Resources

- Firmware: [http://github.com/forkineye/ESPixelStick](http://github.com/forkineye/ESPixelStick)
- Hardware: [http://forkineye.com/ESPixelStick](http://forkineye.com/ESPixelStick)

## Credits

- The great people at [diychristmas.org](http://diychristmas.org) and [doityourselfchristmas.com](http://doityourselfchristmas.com) for inspiration and support.
- [Bill Porter](https://github.com/madsci1016) for initial Renard and SoftAP support.
- Bill Porter and [Grayson Lough](https://github.com/GraysonLough) for initial DMX support.
- [Rich Danby](https://github.com/cinoan) for fixes and helping polish the front-end.
- [penfold42](https://github.com/penfold42) for fixes, brightness, gamma support, and zig-zag / grouping.
  - penfold42 also maintains PWM support in their fork located [here](https://github.com/penfold42/ESPixelBoard).
- [Austin Hodges](https://github.com/ahodges9) for effects support and MQTT cleanup.
- [Matthias C. Hormann](https://github.com/Moonbase59) — some MQTT & effects cleanup.
- [Martin Mueller](https://github.com/MartinMueller2003) — Port to ESP32.
