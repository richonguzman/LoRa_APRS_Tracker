// 首先解除任何可能存在的min/max宏定义，以避免与STL冲突
#undef min
#undef max

/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 * 
 * This file is part of LoRa APRS Tracker.
 * 
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>

// 最小化的joystick_utils.cpp文件，只包含最基本的功能

// 定义一些基本的常量
#define JOYSTICK_UP 1
#define JOYSTICK_DOWN 2
#define JOYSTICK_LEFT 3
#define JOYSTICK_RIGHT 4

// 声明中断处理函数
void joystickUp() {}
void joystickDown() {}
void joystickLeft() {}
void joystickRight() {}

// 简单的初始化函数
void initJoystick() {
    Serial.println("Initializing joystick (minimal)");
}