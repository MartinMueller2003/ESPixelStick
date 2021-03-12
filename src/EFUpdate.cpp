/*
* EFUpdate.cpp
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2016 Shelby Merrick
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

#include <Arduino.h>
#include <FS.h>
#include <lwip/def.h>
#include "EFUpdate.h"

#ifdef ARDUINO_ARCH_ESP32
#   include <LITTLEFS.h>
#   include <Update.h>
#else
#   include <LittleFS.h>
#   define LITTLEFS LittleFS
#endif

#ifndef U_SPIFFS
/*
 * Arduino 8266 libraries removed U_SPIFFS on master, replacing it with U_FS to allow for other FS types -
 * See https://github.com/esp8266/Arduino/commit/a389a995fb12459819e33970ec80695f1eaecc58#diff-6c6d762c616bd0b92156f152d128ad51
 *
 * Substitute the value here, while not breaking things for people using older SDKs.
 */
#	define U_SPIFFS U_FS
#endif


void EFUpdate::begin() {
    _maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    _state = State::HEADER;
    _loc = 0;
    _error = EFUPDATE_ERROR_OK;
}

bool EFUpdate::process(uint8_t *data, size_t len) {
    size_t index = 0;
    bool ConfigChanged = true;

    while (index < len) {
        switch (_state) {
            case State::HEADER:
                _header.raw[_loc++] = data[index++];
                if (_loc == sizeof(efuheader_t)) {
                    if (_header.signature == EFU_ID) {
                        _header.version = ntohs(_header.version);
                        memset(&_record, 0, sizeof(efurecord_t));
                        _loc = 0;
                        _state = State::RECORD;
                    } else {
                        _state = State::FAIL;
                        _error = EFUPDATE_ERROR_SIG;
                    }
                }
                break;
            case State::RECORD:
                _record.raw[_loc++] = data[index++];
                if (_loc == sizeof(efurecord_t)) {
                    _record.type = RecordType(ntohs((uint16_t)_record.type));
                    _record.size = ntohl(_record.size);
                    _loc = 0;
                    if (_record.type == RecordType::SKETCH_IMAGE) {
                        // Begin sketch update
                        if (!Update.begin(_record.size, U_FLASH)) {
                            _state = State::FAIL;
                            _error = Update.getError();
                        } else {
                            _state = State::DATA;
                        }
                    } else if (_record.type == RecordType::FS_IMAGE) {
                        // Begin file system update
#ifdef ARDUINO_ARCH_ESP8266
                        LITTLEFS.end();
#endif
                        if (!Update.begin(_record.size, U_SPIFFS)) {
                            _state = State::FAIL;
                            _error = Update.getError();
                        } else {
                            _state = State::DATA;
                        }
                    } else {
                        _state = State::FAIL;
                        _error = EFUPDATE_ERROR_REC;
                    }
                }
                break;
            case State::DATA:
                size_t toWrite;

                toWrite = (_record.size - _loc < len) ? _record.size - _loc : len - index;
                Update.write(data + index, toWrite);
                index = index + toWrite;
                _loc = _loc + toWrite;

                if (_record.size == _loc) {
                    Update.end(true);
                    memset(&_record, 0, sizeof(efurecord_t));
                    _loc = 0;
                    _state = State::RECORD;
                }
                break;
            case State::FAIL:
                index = len;
                ConfigChanged = false;
                break;
        }
    }

    return ConfigChanged;
}

bool EFUpdate::hasError() {
    return _error != EFUPDATE_ERROR_OK;
}

uint8_t EFUpdate::getError() {
    return _error;
}

bool EFUpdate::end() {
    if (_state == State::FAIL)
        return false;
    else
        return true;
}