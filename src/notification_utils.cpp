/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
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

#include "notification_utils.h"
#include "configuration.h"
#include "board_pinout.h"

#ifdef HAS_I2S
    #include "driver/i2s.h"
    #include <math.h>

    #define I2S_SAMPLE_RATE     44100
    #define I2S_SAMPLE_BITS     16
    #define I2S_CHANNELS        1
    #define MAX_AMPLITUDE       32767

    bool i2sInitialized = false;
#endif

uint8_t channel                 = 0;
uint8_t resolution              = 8;
uint8_t pauseDuration           = 20;

int     startUpSound[]          = {440, 880, 440, 1760};
uint8_t startUpSoundDuration[]  = {100, 100, 100, 200};

int     shutDownSound[]         = {1720, 880, 400};
uint8_t shutDownSoundDuration[] = {60, 60, 200};

extern Configuration    Config;
extern bool             digipeaterActive;

namespace NOTIFICATION_Utils {

#ifdef HAS_I2S
    void initI2S() {
        if (i2sInitialized) return;

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = I2S_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = false,
            .tx_desc_auto_clear = true,
            .fixed_mclk = 0
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = DAC_I2S_BCK,
            .ws_io_num = DAC_I2S_WS,
            .data_out_num = DAC_I2S_DOUT,
            .data_in_num = I2S_PIN_NO_CHANGE
        };

        i2s_driver_install(SPK_I2S_PORT, &i2s_config, 0, NULL);
        i2s_set_pin(SPK_I2S_PORT, &pin_config);
        i2sInitialized = true;
    }

    void playToneI2S(int frequency, uint8_t duration) {
        initI2S();

        // Calculate amplitude with logarithmic curve for natural volume perception
        // Quadratic curve: 50% slider = 25% amplitude, 70% = 49%, 100% = 100%
        float normalized = Config.notification.volume / 100.0f;
        int amplitude = (int)(MAX_AMPLITUDE * normalized * normalized);
        if (amplitude <= 0) return;  // Skip if volume is 0

        int numSamples = (I2S_SAMPLE_RATE * duration) / 1000;
        #ifdef BOARD_HAS_PSRAM
            int16_t* samples = (int16_t*)ps_malloc(numSamples * sizeof(int16_t));
        #else
            int16_t* samples = (int16_t*)malloc(numSamples * sizeof(int16_t));
        #endif

        if (samples == NULL) return;

        // Generate sine wave with volume-adjusted amplitude
        for (int i = 0; i < numSamples; i++) {
            double t = (double)i / I2S_SAMPLE_RATE;
            samples[i] = (int16_t)(amplitude * sin(2.0 * M_PI * frequency * t));
        }

        size_t bytesWritten;
        i2s_write(SPK_I2S_PORT, samples, numSamples * sizeof(int16_t), &bytesWritten, portMAX_DELAY);

        free(samples);
        delay(pauseDuration);
    }

    void playTone(int frequency, uint8_t duration) {
        playToneI2S(frequency, duration);
    }
#else
    void playTone(int frequency, uint8_t duration) {
        ledcSetup(channel, frequency, resolution);
        ledcAttachPin(Config.notification.buzzerPinTone, 0);
        ledcWrite(channel, 128);
        delay(duration);
        ledcWrite(channel, 0);
        delay(pauseDuration);
    }
#endif

    void beaconTxBeep() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        playTone(1320,100);
        if (digipeaterActive) {
            playTone(1560,100);
        }
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

    void messageBeep() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        playTone(1100,100);
        playTone(1100,100);
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

    void stationHeardBeep() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        playTone(1200,100);
        playTone(600,100);
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

    void shutDownBeep() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        for (int i = 0; i < sizeof(shutDownSound) / sizeof(shutDownSound[0]); i++) {
            playTone(shutDownSound[i], shutDownSoundDuration[i]);
        }
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

    void lowBatteryBeep() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        playTone(1550,100);
        playTone(650,100);
        playTone(1550,100);
        playTone(650,100);
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

    void start() {
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, HIGH);
        #endif
        for (int i = 0; i < sizeof(startUpSound) / sizeof(startUpSound[0]); i++) {
            playTone(startUpSound[i], startUpSoundDuration[i]);
        }
        #ifndef HAS_I2S
            digitalWrite(Config.notification.buzzerPinVcc, LOW);
        #endif
    }

}
