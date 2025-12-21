#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "LGFX_ESP32.hpp"

// Global LGFX instance (defined in gui_task.cpp)
extern LGFX tft;

// ---- UI 事件定义 ----

enum UIEventType : uint8_t {
    // ---- 通用 / 导航 ----
    UI_EVENT_STATE = 1,            // 通用状态码
    UI_EVENT_NAV   = 2,            // value: -1 (left) / +1 (right)
    UI_EVENT_ENTER_MENU = 3,
    UI_EVENT_EXIT_MENU  = 4,

    // ---- 系统事件 ----
    /** @brief 系统启动完成，GUI 可结束开机动画并刷新状态 */
    UI_EVENT_BOOT_COMPLETE = 100,  // 保留高位避免与常用事件冲突

    // ---- 灯光 / 灯控相关 ----
    UI_EVENT_LIGHT = 5,            // value: 0=off, 1=on
    UI_EVENT_BRIGHTNESS = 6,       // value: 0-100
    UI_EVENT_CCT        = 7,       // value: 2700-6500 (Kelvin)
    UI_EVENT_RGB        = 8,       // value: 0x00RRGGBB
    UI_EVENT_EFFECT     = 9,       // value: EffectMode enum
    UI_EVENT_AUTO_BR    = 10,      // value: 0=Off, 1=On

    // ---- 网络 / 连接状态 ----
    UI_EVENT_WIFI_IP    = 11,      // IP string handled via s_ipBuffer
    UI_EVENT_WIFI_STATE = 12,      // value: 0=Disconnected, 1=Connected
    UI_EVENT_MQTT_STATE = 13,      // value: 0=Disconnected, 1=Connected
    UI_EVENT_BLE_STATE  = 14,      // value: 0=Disconnected, 1=Connected

    // ---- 传感器 / 环境数据 ----
    UI_EVENT_BATTERY    = 15,      // value: 0-100 (SOC %)
    UI_EVENT_TEMPERATURE = 16,     // fvalue: degrees Celsius
    UI_EVENT_HUMIDITY    = 17,     // fvalue: percent RH
    UI_EVENT_LUX         = 18,     // fvalue: lux
    UI_EVENT_RADAR_DIST  = 19,     // value: distance in cm
    UI_EVENT_RADAR_STATE = 20,     // value: 0=No Target, 1=Moving, 2=Stationary
    UI_EVENT_WEATHER     = 21      // value: weather update event
};

struct UIEvent { 
    UIEventType type; 
    int value; 
    float fvalue; // optional floating value for sensors (temperature, humidity, lux)
};

// 全局事件队列句柄 (由 setup_gui_task 创建)
extern QueueHandle_t uiEventQueue;

// 事件目标掩码
enum EventDestination {
    DEST_GUI  = 1 << 0,
    DEST_MQTT = 1 << 1,
    DEST_BLE  = 1 << 2,
    DEST_ALL  = 0xFF
};

/**
 * @brief 发送 UI 事件到所有注册的队列 (GUI, MQTT, BLE)
 * @param evt 要发送的事件
 * @param excludeMask 要排除的目标掩码 (EventDestination)
 */
void send_ui_event(const UIEvent& evt, uint8_t excludeMask = 0);

/**
 * @brief 初始化并启动 GUI 任务。
 * 
 * 包括：
 * 1. 创建 uiEventQueue
 * 2. 初始化显示硬件
 * 3. 启动 LVGL 渲染循环任务
 */
void setup_gui_task();

// 声明外部变量 (定义在 gui_task.cpp)
extern char s_ipBuffer[16]; 

/**
 * @brief 报告用户活动
 * 
 * 调用此函数会重置自动息屏计时器，并唤醒屏幕（如果已息屏）。
 * 应在检测到按键、触摸或旋钮操作时调用。
 */
void gui_report_activity();

// 状态查询接口
bool gui_is_screen_on();
bool gui_is_power_save_mode();
void gui_set_power_save_mode(bool enabled);
 
