/* RadioLib HAL for ESP32-C3 — uses spi_master.h (portable, not raw registers) */

#ifndef ESP_C3_HAL_H
#define ESP_C3_HAL_H

#include <RadioLib.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOW     0
#define HIGH    1
#define INPUT   0x01
#define OUTPUT  0x03
#define RISING  0x01
#define FALLING 0x02

class EspC3Hal : public RadioLibHal {
public:
    EspC3Hal(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          spiSCK(sck), spiMISO(miso), spiMOSI(mosi), spiHandle(NULL) {}

    void init() override {
        spiBegin();
    }

    void term() override {
        spiEnd();
    }

    void pinMode(uint32_t pin, uint32_t mode) override {
        if (pin == RADIOLIB_NC) return;
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << pin;
        cfg.mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&cfg);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == RADIOLIB_NC) return;
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == RADIOLIB_NC) return 0;
        return gpio_get_level((gpio_num_t)pin);
    }

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override {
        if (interruptNum == RADIOLIB_NC) return;

        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << interruptNum;
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = (mode == RISING) ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
        gpio_config(&cfg);

        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void*))interruptCb, NULL);
    }

    void detachInterrupt(uint32_t interruptNum) override {
        if (interruptNum == RADIOLIB_NC) return;
        gpio_isr_handler_remove((gpio_num_t)interruptNum);
        gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
    }

    void delay(RadioLibTime_t ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(RadioLibTime_t us) override {
        uint64_t end = esp_timer_get_time() + us;
        while ((uint64_t)esp_timer_get_time() < end) {
            asm volatile("nop");
        }
    }

    RadioLibTime_t millis() override {
        return (RadioLibTime_t)(esp_timer_get_time() / 1000ULL);
    }

    RadioLibTime_t micros() override {
        return (RadioLibTime_t)esp_timer_get_time();
    }

    long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override {
        if (pin == RADIOLIB_NC) return 0;
        this->pinMode(pin, INPUT);
        RadioLibTime_t start = this->micros();
        while (this->digitalRead(pin) == state) {
            if (this->micros() - start > timeout) return 0;
        }
        return this->micros() - start;
    }

    void spiBegin() override {
        spi_bus_config_t busCfg = {};
        busCfg.mosi_io_num = spiMOSI;
        busCfg.miso_io_num = spiMISO;
        busCfg.sclk_io_num = spiSCK;
        busCfg.quadwp_io_num = -1;
        busCfg.quadhd_io_num = -1;
        busCfg.max_transfer_sz = 256;

        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &busCfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) return;

        spi_device_interface_config_t devCfg = {};
        devCfg.mode = 0;                       /* SPI mode 0 (CPOL=0, CPHA=0) */
        devCfg.clock_speed_hz = 2 * 1000 * 1000; /* 2 MHz — safe for SX1262 */
        devCfg.spics_io_num = -1;              /* CS managed by RadioLib */
        devCfg.queue_size = 1;

        spi_bus_add_device(SPI2_HOST, &devCfg, &spiHandle);
    }

    void spiBeginTransaction() override {
        /* Nothing — transaction managed per-transfer */
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        spi_transaction_t trans = {};
        trans.length = len * 8;
        trans.tx_buffer = out;
        trans.rx_buffer = in;
        spi_device_transmit(spiHandle, &trans);
    }

    void spiEndTransaction() override {
        /* Nothing */
    }

    void spiEnd() override {
        if (spiHandle) {
            spi_bus_remove_device(spiHandle);
            spiHandle = NULL;
        }
        spi_bus_free(SPI2_HOST);
    }

private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_device_handle_t spiHandle;
};

#endif /* ESP_C3_HAL_H */
