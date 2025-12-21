/**
 * @file screen_main.hpp
 * @brief 主仪表盘屏幕 (Main Dashboard Screen)
 * 
 * 显示时间、传感器数据 (温度、湿度、光照、雷达) 和系统状态 (电池、MQTT、灯光)。
 */

#pragma once
#include <lvgl.h>

/**
 * @brief 创建主屏幕
 * @param parent 父对象 (通常是活动屏幕)
 */
lv_obj_t* ui_create_main_screen(lv_obj_t *parent);

/**
 * @brief 设置主屏幕的可见性
 * @param visible true 显示, false 隐藏
 */
void ui_main_set_visible(bool visible);

// =================================================================================
// 更新函数 (Update Functions)
// =================================================================================

void ui_main_update_time(int hour, int minute, int second);
void ui_main_update_state(int state);
// void ui_main_update_light(bool on); // Removed
// void ui_main_update_mqtt(bool connected); // Moved to status
void ui_main_update_wifi_state(bool connected, int rssi); // Added
void ui_main_update_ble_state(bool connected); // Added
void ui_main_update_battery(int value);
void ui_main_update_temp(float t);
void ui_main_update_humi(float h);
void ui_main_update_lux(float lux);
void ui_main_update_radar_dist(int dist);
void ui_main_update_radar_state(int state);
void ui_main_update_ip(const char* ip);
void ui_main_update_weather(const char* city, const char* weather, const char* temp);
