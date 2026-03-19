// mock_esp_log.h
#ifndef MOCK_ESP_LOG_H
#define MOCK_ESP_LOG_H

#include <cstdio>

#define ESP_LOGE(tag, format, ...) printf("E %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("W %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) printf("I %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("D %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("V %s: " format "\n", tag, ##__VA_ARGS__)

#endif // MOCK_ESP_LOG_H