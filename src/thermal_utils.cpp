/* Copyright (C) 2026 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS Tracker.
 *
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

// Fan / thermal control adapted from KJ7NYE's LoRa_FieldOps_APRS_Tracker (GPLv3)
// NTC circuit (T-Beam 1W V1.0 schematic): 3V3 -> NCP18XH103F03RB -> TEMP_PIN -> 10K -> GND

#include "board_pinout.h"

#ifdef FAN_CTRL_PIN

#include <Arduino.h>
#include <math.h>
#include "thermal_utils.h"
#include "power_utils.h"
#include "logger.h"

extern logging::Logger  logger;

#define NTC_B               3380.0f     // B25/50 (K)
#define NTC_R25             10000.0f    // NTC resistance at 25C (ohm)
#define NTC_T0              298.15f     // 25C in Kelvin
#define NTC_R_FIXED         10000.0f    // pull-down resistor (ohm)
#define NTC_VCC_MV          3300.0f

#define FAN_ON_TEMP         50.0f       // fan on at or above
#define FAN_OFF_TEMP        42.0f       // fan off below (hysteresis)
#define TEMP_WARNING        75.0f
#define TEMP_SHUTDOWN       85.0f
#define TX_COOLDOWN_MS      30000       // fan kept on after TX
#define TEMP_SAMPLE_MS      30000

static float       currentTemperature  = 25.0f;
static bool        fanOn               = false;
static bool        txCooldownActive    = false;
static uint32_t    txEndTime           = 0;
static uint32_t    lastTempSample      = 0;
static bool        firstTempSample     = true;


namespace THERMAL_Utils {

    float readTemperature() {
        analogReadMilliVolts(TEMP_PIN);     // dummy read to settle the ADC
        delay(1);
        uint32_t sum = 0;
        for (int i = 0; i < 5; i++) {
            sum += analogReadMilliVolts(TEMP_PIN);
            delay(3);
        }
        float mv = sum / 5.0f;
        if (mv <= 0.0f || mv >= NTC_VCC_MV) return NAN;
        float ntcResistance = NTC_R_FIXED * (NTC_VCC_MV - mv) / mv;
        float kelvin = 1.0f / (1.0f / NTC_T0 + (1.0f / NTC_B) * logf(ntcResistance / NTC_R25));
        return kelvin - 273.15f;
    }

    void setFan(bool on, const char* reason) {
        if (fanOn == on) return;
        fanOn = on;
        digitalWrite(FAN_CTRL_PIN, on ? HIGH : LOW);
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_INFO, "Thermal", "Fan %s (%s, %.1f C)", on ? "ON" : "OFF", reason, currentTemperature);
    }

    void updateFan() {
        if (!fanOn) {
            if (txCooldownActive) {
                setFan(true, "TX cooldown");
            } else if (currentTemperature >= FAN_ON_TEMP) {
                setFan(true, "temperature");
            }
        } else if (!txCooldownActive && currentTemperature < FAN_OFF_TEMP) {
            setFan(false, "cooled down");
        }
    }

    void setup() {
        pinMode(TEMP_PIN, INPUT);
        analogSetPinAttenuation(TEMP_PIN, ADC_11db);
        pinMode(FAN_CTRL_PIN, OUTPUT);
        digitalWrite(FAN_CTRL_PIN, LOW);
        fanOn           = false;
        firstTempSample = true;     // first monitor() call samples immediately
    }

    void monitor() {
        uint32_t now = millis();

        if (txCooldownActive && (now - txEndTime >= TX_COOLDOWN_MS)) {
            txCooldownActive = false;
            updateFan();
        }

        if (!firstTempSample && (now - lastTempSample < TEMP_SAMPLE_MS)) return;
        firstTempSample = false;
        lastTempSample  = now;

        float temperature = readTemperature();
        if (!isnan(temperature)) currentTemperature = temperature;
        logger.log(logging::LoggerLevel::LOGGER_LEVEL_DEBUG, "Thermal", "%.1f C | fan %s", currentTemperature, fanOn ? "ON" : "OFF");

        if (currentTemperature >= TEMP_SHUTDOWN) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_ERROR, "Thermal", "Over-temperature shutdown at %.1f C", currentTemperature);
            POWER_Utils::shutdown();
            return;
        }
        if (currentTemperature >= TEMP_WARNING) {
            logger.log(logging::LoggerLevel::LOGGER_LEVEL_WARN, "Thermal", "High temperature: %.1f C", currentTemperature);
        }
        updateFan();
    }

    void onTxStart() {
        txCooldownActive = false;
        setFan(true, "TX");
    }

    void onTxEnd() {
        txCooldownActive = true;
        txEndTime        = millis();
    }

    float getTemperature() {
        return currentTemperature;
    }

}

#endif
