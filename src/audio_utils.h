#ifndef AUDIO_UTILS_H_
#define AUDIO_UTILS_H_

#include <Arduino.h>
#include <driver/i2s.h>

namespace AUDIO_Utils {

    //void setup();
    void setupAmpI2S(i2s_port_t i2s_ch);
    
}

#endif