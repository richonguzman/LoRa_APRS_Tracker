#include "boards_pinout.h"
#include "audio_utils.h"
#ifdef HAS_I2S
#include "Audio.h"
#include <SPIFFS.h>
#include "es7210.h"
#include <driver/i2s.h>


    Audio audio;
    
    namespace AUDIO_Utils {

        void setup() {
            audio.setPinout(DAC_I2S_BCK, DAC_I2S_WS, DAC_I2S_DOUT);
            audio.setVolume(21);
        }

        bool setupCoder() {
            uint32_t ret_val = ESP_OK;

            Wire.beginTransmission(ES7210_ADDR);
            uint8_t error = Wire.endTransmission();
            if (error != 0) {
                Serial.println("ES7210 address not found"); return false;
            }

            audio_hal_codec_config_t cfg = {
                .adc_input = AUDIO_HAL_ADC_INPUT_ALL,
                .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,
                .i2s_iface =
                {
                    .mode = AUDIO_HAL_MODE_SLAVE,
                    .fmt = AUDIO_HAL_I2S_NORMAL,
                    .samples = AUDIO_HAL_16K_SAMPLES,
                    .bits = AUDIO_HAL_BIT_LENGTH_16BITS,
                },
            };

            ret_val |= es7210_adc_init(&Wire, &cfg);
            ret_val |= es7210_adc_config_i2s(cfg.codec_mode, &cfg.i2s_iface);
            ret_val |= es7210_adc_set_gain(
                        (es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2),
                        (es7210_gain_value_t)GAIN_6DB);
            ret_val |= es7210_adc_ctrl_state(cfg.codec_mode, AUDIO_HAL_CTRL_START);
            return ret_val == ESP_OK;

        }

        void playMP3(const String& filename) {
            bool findMp3 = false;
            findMp3 = audio.connecttoFS(SPIFFS, filename.c_str());
            if (findMp3) {
                audio.setVolume(21);
                while (audio.isRunning()) {
                    audio.loop();
                //    delay(3);
                }
                /*audio.setVolume(0);
                delay(10);
                audio.stopSong();
                delay(50);*/
                //audio.stop();
            }
        }

    }

#endif