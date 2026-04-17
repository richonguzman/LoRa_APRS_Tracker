idf_component_register(
    SRCS "main.c" "gps_uart.c" "proto_uart.c" "smartbeacon.c"
         "aprs_packet.c" "lora_radio.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer heap uart_proto radiolib
)
