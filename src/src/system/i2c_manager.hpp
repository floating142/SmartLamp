#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Global I2C mutex for protecting access to TwoWire/ Wire across tasks
extern SemaphoreHandle_t i2c_mutex;

/**
 * @brief 创建 I2C 互斥锁
 * 
 * 必须在 Wire.begin() 之后调用。
 * 线程安全，可多次调用。
 */
void setup_i2c_manager();
