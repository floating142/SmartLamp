#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "../system/storage.hpp"

struct WeatherData {
    String city;
    String weather_text;
    String temp;
    String icon_code;
    bool valid = false;
};

/**
 * @brief 初始化并启动天气更新任务
 */
void setup_weather_task();

/**
 * @brief 获取当前缓存的天气数据
 */
WeatherData get_current_weather();

/**
 * @brief 强制立即更新天气
 */
void weather_force_update();
