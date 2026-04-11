#ifndef LGFX_CROWPANEL_35_H_
#define LGFX_CROWPANEL_35_H_

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Custom LGFX class for CrowPanel Advance 3.5"
class LGFX_CrowPanel_35 : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488     _panel_instance;
    lgfx::Bus_SPI           _bus_instance;
    lgfx::Light_PWM         _light_instance;
    lgfx::Touch_GT911       _touch_instance;

public:
    LGFX_CrowPanel_35(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = 42;
            cfg.pin_mosi = 39;
            cfg.pin_miso = -1;
            cfg.pin_dc   = 41;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 40;
            cfg.pin_rst          = -1;  // IO2 shared with LoRa NRESET — managed manually at boot
            cfg.pin_busy         = -1;
            cfg.memory_width     = 320;
            cfg.memory_height    = 480;
            cfg.panel_width      = 320;
            cfg.panel_height     = 480;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 38;
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.x_min      = 0;
            cfg.x_max      = 319;
            cfg.y_min      = 0;
            cfg.y_max      = 479;
            cfg.pin_int    = -1;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;

            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;
            cfg.pin_sda  = 15;
            cfg.pin_scl  = 16;
            cfg.freq = 400000;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

#endif // LGFX_CROWPANEL_35_H_