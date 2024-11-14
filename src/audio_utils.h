#ifndef AUDIO_UTILS_H_
#define AUDIO_UTILS_H_

#include <Arduino.h>


namespace AUDIO_Utils {

    void setup();
    void playMP3(const String& filename);
        
}

#endif