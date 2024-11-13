#include "boards_pinout.h"
#include "audio_utils.h"
#include "Audio.h"
//#include <FS.h>
//#include <SD_MMC.h>
//#include "SD_MMC.h"
//#include "FS.h"
//#include "SD.h"

#include <SPI.h>
#include <SD.h>
#include "es7210.h"

#ifdef HAS_I2S

    //#define MIC_I2S_SAMPLE_RATE         16000
    //#define MIC_I2S_PORT                I2S_NUM_1
    //#define SPK_I2S_PORT                I2S_NUM_0
    #define VAD_SAMPLE_RATE_HZ          16000
    #define VAD_FRAME_LENGTH_MS         30
    #define VAD_BUFFER_LENGTH           (VAD_FRAME_LENGTH_MS * VAD_SAMPLE_RATE_HZ / 1000)


    Audio audio;

    
    namespace AUDIO_Utils {

        void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
            Serial.printf("Listing directory: %s\n", dirname);

            File root = fs.open(dirname);
            if(!root){
                Serial.println("Failed to open directory");
                return;
            }
            if(!root.isDirectory()){
                Serial.println("Not a directory");
                return;
            }

            File file = root.openNextFile();
            while(file){
                if(file.isDirectory()){
                    Serial.print("  DIR : ");
                    Serial.println(file.name());
                    if(levels){
                        listDir(fs, file.path(), levels -1);
                    }
                } else {
                    Serial.print("  FILE: ");
                    Serial.print(file.name());
                    Serial.print("  SIZE: ");
                    Serial.println(file.size());
                    //v_audioContent.insert(v_audioContent.begin(), strdup(file.path()));
                }
                file = root.openNextFile();
            }
            //Serial.printf("num files %i", v_audioContent.size());
            root.close();
            file.close();
        }

        void setupAmpI2S(i2s_port_t i2s_ch) {
            //SD.begin(BOARD_SDCARD_CS);
            audio.setPinout(DAC_I2S_BCK, DAC_I2S_WS, DAC_I2S_DOUT);
            audio.setVolume(21);
        }


        /*void setupMicrophoneI2S(i2s_port_t  i2s_ch) {
            i2s_config_t i2s_config = {
                .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                .sample_rate = MIC_I2S_SAMPLE_RATE,
                .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
                .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                .dma_buf_count = 8,
                .dma_buf_len = 64,
                .use_apll = false,
                .tx_desc_auto_clear = true,
                .fixed_mclk = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                .bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
                .chan_mask = (i2s_channel_t)(I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1 |
                                            I2S_TDM_ACTIVE_CH2 | I2S_TDM_ACTIVE_CH3),
                .total_chan = 4,
            };
            i2s_pin_config_t pin_config = {0};
            pin_config.data_in_num = BOARD_ES7210_DIN;
            pin_config.mck_io_num = BOARD_ES7210_MCLK;
            pin_config.bck_io_num = BOARD_ES7210_SCK;
            pin_config.ws_io_num = BOARD_ES7210_LRCK;
            pin_config.data_out_num = -1;
            i2s_driver_install(i2s_ch, &i2s_config, 0, NULL);
            i2s_set_pin(i2s_ch, &pin_config);
            i2s_zero_dma_buffer(i2s_ch);

        #ifdef USE_ESP_VAD
            // Initialize esp-sr vad detected
        #if ESP_IDF_VERSION_VAL(4,4,1) == ESP_IDF_VERSION
            vad_inst = vad_create(VAD_MODE_0, MIC_I2S_SAMPLE_RATE, VAD_FRAME_LENGTH_MS);
        #elif  ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,4,1)
            vad_inst = vad_create(VAD_MODE_0);
        #else
        #error "No support this version."
        #endif
            vad_buff = (int16_t *)ps_malloc(vad_buffer_size);
            if (vad_buff == NULL) {
                while (1) {
                    Serial.println("Memory allocation failed!");
                    delay(1000);
                }
            }
            xTaskCreate(vadTask, "vad", 8 * 1024, NULL, 12, &vadTaskHandler);
        #else
            // xTaskCreate(audioLoopbackTask, "vad", 8 * 1024, NULL, 12, &vadTaskHandler);
        #endif

        }*/

        //setupAmpI2S(SPK_I2S_PORT);

        //setupMicrophoneI2S(MIC_I2S_PORT);


    }

#endif