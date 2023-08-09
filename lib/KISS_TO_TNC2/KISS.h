// http://en.wikipedia.org/wiki/KISS_(TNC)#Special_Characters
// http://k4kpk.com/notes-on-aprs-kiss-and-setting-up-a-tnc-x-igate-and-digipeater



#include <stdint.h>
#include <stdlib.h>

#define KISS_H
#define DCD_ON            0x03            // starting decode

#define FEND              0xC0            // frame END
#define FESC              0xDB            // frame Escape
#define TFEND             0xDC            // Transposed Frame End
#define TFESC             0xDD            // Transposed Frame Escape

#define CMD_UNKNOWN       0xFE
#define CMD_DATA          0x00
#define CMD_HARDWARE      0x06

#define HW_RSSI           0x21

#define CMD_ERROR         0x90
#define ERROR_INITRADIO   0x01
#define ERROR_TXFAILED    0x02
#define ERROR_QUEUE_FULL  0x04