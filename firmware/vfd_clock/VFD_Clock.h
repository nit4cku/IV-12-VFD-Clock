/*
 * Copyright (c) 2018 nitacku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * @file        VFD_Clock.h
 * @summary     Digital Clock for vacuum fluorescent display tubes
 * @version     5.0
 * @author      nitacku
 * @data        13 August 2018
 */

#ifndef _VFDCLOCK_H
#define _VFDCLOCK_H

#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <nCoder.h>
#include <DS323x.h>
#include <nDisplay.h>
#include <nAudio.h>
#include <nI2C.h>
#include "Music.h"

const uint8_t VERSION       = 13;
const uint8_t DISPLAY_COUNT = 6;
const char CONFIG_KEY       = '$';
const uint8_t ALARM_COUNT   = 3;

// Macros to simplify port manipulation without additional overhead
#define getPort(pin)    ((pin < 8) ? PORTD : ((pin < A0) ? PORTB : PORTC))
#define getMask(pin)    _BV((pin < 8) ? pin : ((pin < A0) ? pin - 8 : pin - A0))
#define setPinHigh(pin) (getPort(pin) |= getMask(pin))
#define setPinLow(pin)  (getPort(pin) &= ~getMask(pin))

/* === Digit Representation ===

 777777
 6    5
 6    5
 444444
 3    2
 3    2 00
 111111 00

 Where bit == (1 << #)
=============================*/

static const uint8_t BITMAP[96] PROGMEM =
{
    0x00, 0x21, 0x60, 0x58, 0xD6, 0x39, 0xC6, 0x20, //  !"#$%&'
    0x48, 0x24, 0xF0, 0x34, 0x03, 0x10, 0x01, 0x38, // ()*+,-./
    0xEE, 0x24, 0xBA, 0xB6, 0x74, 0xD6, 0xDE, 0xA4, // 01234567
    0xFE, 0xF6, 0x82, 0x86, 0x3A, 0x12, 0x56, 0xB9, // 89:;<=>?
    0xBE, 0xFC, 0x5E, 0xCA, 0x3E, 0xDA, 0xD8, 0xF6, // @ABCDEFG
    0x7C, 0x24, 0x2E, 0x7C, 0x4A, 0xEC, 0xEC, 0xEE, // HIJKLMNO
    0xF8, 0xF4, 0xC8, 0xD6, 0x5A, 0x6E, 0x6E, 0x6E, // PQRSTUVW
    0x7C, 0x76, 0xBA, 0xCA, 0x54, 0xA6, 0xE0, 0x02, // XYZ[\]^_
    0x40, 0xBE, 0x5E, 0x1A, 0x3E, 0xFA, 0xD8, 0xF6, // `abcdefg
    0x5C, 0x04, 0x2E, 0x7C, 0x4A, 0x1C, 0x1C, 0x1E, // hijklmno
    0xF8, 0xF4, 0x18, 0xD6, 0x5A, 0x0E, 0x0E, 0x0E, // pqrstuvw
    0x7C, 0x76, 0xBA, 0x08, 0x48, 0x04, 0x80, 0xFF, // xyz{|}~█
};

enum digital_pin_t : uint8_t
{
    DIGITAL_PIN_ENCODER_0 = 2,
    DIGITAL_PIN_ENCODER_1 = 3,
    DIGITAL_PIN_BUTTON = 4,
    DIGITAL_PIN_CLOCK = 10,
    DIGITAL_PIN_SDATA = 8,
    DIGITAL_PIN_LATCH = 9,
    DIGITAL_PIN_BLANK = 11,
    DIGITAL_PIN_HV_ENABLE = 5,
    DIGITAL_PIN_AC_ENABLE = 6,
    DIGITAL_PIN_AC_TOGGLE = 12,
    DIGITAL_PIN_TRANSDUCER_0 = 13,
    DIGITAL_PIN_TRANSDUCER_1 = A0,
    DIGITAL_PIN_TRANSDUCER_2 = A1,
};

enum analog_pin_t : uint8_t
{
    ANALOG_PIN_PHOTODIODE = A3,
    ANALOG_PIN_BATTERY = A2,
};

enum battery_t : uint16_t
{
    BATTERY_MIN = 2400,
    BATTERY_MAX = 3100,
};

enum interrupt_speed_t : uint8_t
{
    INTERRUPT_FAST = 43, // 16MHz / (60Hz * 6 tubes * 8 levels * 128 prescale),
    INTERRUPT_SLOW = 255,
};

enum class FormatDate : uint8_t
{
    YYMMDD,
    MMDDYY,
    DDMMYY,
};

enum class FormatTime : uint8_t
{
    H24,
    H12,
};

enum class Cycle : uint8_t
{
    AM,
    PM,
};

enum class RTCSelect : uint8_t
{
    TIME,
    DATE,
};

enum class Effect : uint8_t
{
    NONE,
    SPIRAL,
    DATE,
    PHRASE,
};

enum class State : bool
{
    DISABLE,
    ENABLE,
};

struct StateStruct
{
    StateStruct()
    : voltage(State::DISABLE)
    , display(State::DISABLE)
    , alarm(State::DISABLE)
    {
        // empty
    }

    State voltage;
    State display;
    State alarm;
};

struct AlarmStruct
{
    AlarmStruct()
    : state(State::DISABLE)
    , music(0)
    , days(0)
    , time(0)
    {
        // empty
    }
    
    State       state;
    uint8_t     music;
    uint8_t     days;
    uint32_t    time;
};

struct Config
{
    Config()
    : validate(CONFIG_KEY)
    , noise(State::ENABLE)
    , battery(State::ENABLE)
    , brightness(CDisplay::Brightness::AUTO)
    , gain(10)
    , offset(10)
    , date_format(FormatDate::DDMMYY)
    , time_format(FormatTime::H24)
    , temperature_unit(CRTC::Unit::F)
    , effect(Effect::NONE)
    , blank_begin(0)
    , blank_end(0)
    , music_timer(0)  
    , alarm() // Default initialization
    {
        memcpy_P(phrase, PSTR("Photon"), DISPLAY_COUNT + 1);
    }

    char                    validate;
    State                   noise;
    State                   battery;
    CDisplay::Brightness    brightness;
    uint8_t                 gain;
    uint8_t                 offset;
    FormatDate              date_format;
    FormatTime              time_format;
    CRTC::Unit              temperature_unit;
    Effect                  effect;
    uint32_t                blank_begin;
    uint32_t                blank_end;
    uint8_t                 music_timer;
    AlarmStruct             alarm[ALARM_COUNT];
    char                    phrase[DISPLAY_COUNT + 1];
};

// Return integral value of Enumeration
template<typename T> constexpr uint8_t getValue(const T e)
{
    return static_cast<uint8_t>(e);
}

//---------------------------------------------------------------------
// Implicit Function Prototypes
//---------------------------------------------------------------------

// delay            B7971
// digitalWrite     Wire
// analogRead       B7971
// pinMode          B7971
// attachInterrupt  CEncoder
// strlen           CDisplay
// strlen_P         CDisplay
// strncpy          CDisplay
// strcpy_P         CDisplay
// snprintf_P       B7971
// memcpy           B7971
// memset           B7971
// memcpy_P         B7971

//---------------------------------------------------------------------
// Function Prototypes
//---------------------------------------------------------------------

// Mode functions
void Timer(const uint8_t hour, const uint8_t minute, const uint8_t second);
void Detonate(void);
void PlayAlarm(const uint8_t song_index, const char* phrase);

// Automatic functions
void AutoAlarm(void);
void AutoBrightness(void);
void AutoBlanking(void);

// Update functions
void UpdateAlarmIndicator(void);

// Format functions
uint8_t FormatHour(const uint8_t hour);
void FormatRTCString(const CRTC::RTC& rtc, char* s, const RTCSelect type);
uint32_t GetSeconds(const uint8_t hour, const uint8_t minute, const uint8_t second);

// Analog functions
CDisplay::Brightness ReadLightIntensity(void);
uint32_t ReadBatteryMillivolts(void);

// EEPROM functions
void GetConfig(Config& g_config);
void SetConfig(const Config& g_config);

// State functions
void VoltageState(const State state);
void DisplayState(const State state);
bool GetBatteryState(void);

// Interrupt functions
void InterruptSpeed(const uint8_t speed);

// Callback functions
void EncoderCallback(void);
bool IsInputIncrement(void);
bool IsInputSelect(void);
bool IsInputUpdate(void);

#endif
