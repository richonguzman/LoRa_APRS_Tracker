// 简单的main.cpp文件，用于测试编译

// 在任何其他包含之前无条件解除min/max宏的定义
#undef min
#undef max

#include <Arduino.h>

// 简单的setup函数
void setup() {
    // 初始化串口
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待串口连接
    }
    Serial.println("Hello from NRF52840!");
}

// 简单的loop函数
void loop() {
    // 什么都不做
}