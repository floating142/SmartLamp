/**
 * @file gui_task.cpp
 * @brief GUI 任务实现
 * 
 * 负责初始化 LVGL，创建 UI，并运行主循环。
 * 同时处理来自 uiEventQueue 的事件并更新界面。
 */

#include "gui_task.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ui_manager.hpp"
#include "LGFX_ESP32.hpp" // 确保包含 LGFX 定义
#include "../network/mqtt_task.hpp"
#include "../network/ble_task.hpp"
#include "../network/weather_task.hpp"
#include "screens/screen_main.hpp"
#include "../app/lamp.hpp"

// =================================================================================
// 显示驱动配置 (Display Driver Configuration)
// =================================================================================

#define SCREEN_W 240
#define SCREEN_H 240
// 优化内存：将缓冲区高度从 20 降至 10
// 节省内存：240 * 10 * 2(bytes) * 2(buffers) = 9600 bytes
#define BUF_SIZE (SCREEN_W * 10) 

// 全局 LGFX 实例
LGFX tft; 

// LVGL 显示缓冲区 (双缓冲)
static lv_color_t buf1[BUF_SIZE];
static lv_color_t buf2[BUF_SIZE];

/**
 * @brief LVGL 显示刷新回调
 * 
 * 将 LVGL 的渲染缓冲区推送到 LGFX 驱动的屏幕上。
 */
static void my_disp_flush(lv_display_t *disp,
                          const lv_area_t *area,
                          uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // 使用 writePixels 进行 DMA 传输 (如果支持)
    tft.writePixels((uint16_t*)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

/**
 * @brief 初始化 LVGL 与显示驱动
 */
static void init_lvgl_display()
{
    lv_init();

    // 关联 LVGL 心跳与 Arduino millis
    lv_tick_set_cb(millis);

    // 创建显示对象
    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // 设置双缓冲区
    lv_display_set_buffers(
        disp,
        buf1, buf2,
        sizeof(buf1),
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );
}

// =================================================================================
// 任务实现 (Task Implementation)
// =================================================================================

#include "../system/storage.hpp"

// ---- 全局变量 ----
QueueHandle_t uiEventQueue = nullptr;

// 全局变量用于传递 IP 字符串 (简单处理)
char s_ipBuffer[16] = {0};

// ---- 自动息屏相关 ----
static uint32_t s_lastActivityTime = 0;
static uint32_t s_wakeupTime = 0; // 唤醒时间，用于防抖
static bool s_isScreenOn = true;
static bool s_powerSaveMode = false; // 默认关闭省电模式

// 超时设置
static constexpr uint32_t TIMEOUT_NORMAL = 2 * 60 * 1000; // 2分钟
static constexpr uint32_t TIMEOUT_POWERSAVE = 30 * 1000;  // 30秒

// 辅助函数：渐变背光
static void fade_backlight(uint8_t start, uint8_t end, int duration_ms) {
    int steps = 20;
    int delay_per_step = duration_ms / steps;
    float step_val = (float)(end - start) / steps;
    
    for (int i = 1; i <= steps; i++) {
        uint8_t val = (uint8_t)(start + step_val * i);
        tft.setBrightness(val);
        vTaskDelay(pdMS_TO_TICKS(delay_per_step));
    }
    tft.setBrightness(end);
}

void gui_report_activity() {
    s_lastActivityTime = millis();
    
    // 如果处于息屏显示模式，则唤醒
    if (ui_is_screensaver() || !s_isScreenOn) {
        // 如果之前是省电模式(屏幕关闭)，不需要退出 screensaver (因为没进入)
        // 只需要恢复背光
        if (ui_is_screensaver()) {
            ui_exit_screensaver(true);
        }
        
        // 渐变唤醒
        fade_backlight(s_isScreenOn ? 10 : 0, 255, 300);
        
        setCpuFrequencyMhz(160); // 恢复高性能模式
        s_isScreenOn = true;
        s_wakeupTime = millis(); // 记录唤醒时间
        Serial.println("[GUI] Exit Screensaver / Wakeup (CPU 160MHz)");
    }
}

bool gui_is_screen_on() {
    return s_isScreenOn;
}

bool gui_is_power_save_mode() {
    return s_powerSaveMode;
}

void gui_set_power_save_mode(bool enabled) {
    s_powerSaveMode = enabled;
    AppConfig::instance().savePowerSaveMode(enabled);
    Serial.printf("[GUI] Power Save Mode: %s\n", enabled ? "ON" : "OFF");
}

// ---- 任务句柄 ----
static TaskHandle_t s_guiTaskHandle = nullptr;

/**
 * @brief GUI 主任务
 */
static void task_gui(void *pvParameters) {
    // 1. 初始化显示驱动和 LVGL
    init_lvgl_display();
    
    // 2. 构建 UI 界面
    ui_init();

    // 加载省电模式配置
    AppConfig::instance().loadPowerSaveMode(s_powerSaveMode);

    Serial.println("[GUI] Interface initialized");
    
    // 初始化活动时间
    s_lastActivityTime = millis();

    uint32_t lastTimeUpdate = 0;

    for (;;) {
        uint32_t now_ms = millis();

        // 自动息屏逻辑
        uint32_t timeout = s_powerSaveMode ? TIMEOUT_POWERSAVE : TIMEOUT_NORMAL;
        
        if (s_isScreenOn && (now_ms - s_lastActivityTime > timeout)) {
            // 进入息屏显示模式 (Screensaver)
            if (!ui_is_screensaver()) {
                if (s_powerSaveMode) {
                    // 省电模式：渐变关闭背光，不显示 Screensaver 内容
                    fade_backlight(255, 0, 500);
                    s_isScreenOn = false; // 标记为屏幕关闭
                    ui_enter_sleep();     // 重置状态并回主页
                    Serial.println("[GUI] Power Save Sleep (Backlight OFF)");
                } else {
                    // 普通模式：渐变变暗
                    fade_backlight(255, 10, 500);
                    ui_enter_screensaver();
                    Serial.println("[GUI] Enter Screensaver (Clock Mode)");
                }
                setCpuFrequencyMhz(80); // 降低 CPU 频率
            }
        }

        // 时钟更新 (每秒)
        if (now_ms - lastTimeUpdate >= 1000) {
            lastTimeUpdate = now_ms;
            
            // 获取当前系统时间戳 (秒)
            time_t now_sec;
            time(&now_sec);
            
            struct tm timeinfo;
            localtime_r(&now_sec, &timeinfo);

            // 判断时间是否有效 (例如年份 > 2020 说明 NTP 已同步)
            if (timeinfo.tm_year > 120) {
                // NTP 时间有效：直接显示
                ui_update_time(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                
                // 更新日期 (仅在息屏模式下需要，或者主界面有日期显示)
                if (ui_is_screensaver()) {
                    // 格式化日期字符串: YYYY-MM-DD Weekday
                    char dateStr[32];
                    const char* weekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d %s", 
                             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                             weekDays[timeinfo.tm_wday]);
                    ui_update_date(dateStr);
                }
            } else {
                // NTP 未同步：显示系统运行时间 (Uptime) 作为 fallback
                unsigned long totalSeconds = now_ms / 1000;
                int h = (totalSeconds / 3600) % 24;
                int m = (totalSeconds / 60) % 60;
                int s = totalSeconds % 60;
                ui_update_time(h, m, s);
                
                if (ui_is_screensaver()) {
                     ui_update_date("Connecting WiFi...");
                }
            }

            // 刷新系统状态页 (如果当前处于该页面)
            ui_update_status_page();
        }

        // 处理来自其他任务的事件（在 GUI 线程上下文中安全执行）
        UIEvent evt;
        // 非阻塞读取队列中的所有待处理事件
        while (uiEventQueue && xQueueReceive(uiEventQueue, &evt, 0) == pdTRUE) {
            switch (evt.type) {
                case UI_EVENT_STATE:
                    ui_update_state(evt.value);
                    break;
                case UI_EVENT_LIGHT:
                    ui_update_light_state(evt.value != 0);
                    break;
                case UI_EVENT_NAV:
                    // value: -1 (左/减) / +1 (右/加)

                    // 如果处于息屏模式，导航操作仅用于唤醒，不执行实际导航
                    if (ui_is_screensaver() || !s_isScreenOn) {
                        gui_report_activity(); // 唤醒屏幕
                    } else {
                        // 防抖：唤醒后 500ms 内忽略导航事件，防止误触
                        if (millis() - s_wakeupTime > 500) {
                            ui_nav(evt.value);
                        }
                    }
                    break;

                case UI_EVENT_BOOT_COMPLETE:
                    // 强制结束开机动画，切换到主屏幕
                    extern void ui_boot_complete(); // 在 ui_manager.cpp 中实现
                    ui_boot_complete();
                    break;
                case UI_EVENT_ENTER_MENU:
                    if (ui_is_screensaver() || !s_isScreenOn) {
                        gui_report_activity(); // 唤醒屏幕
                    } else {
                        if (millis() - s_wakeupTime > 500) {
                            ui_enter_menu();
                        }
                    }
                    break;
                case UI_EVENT_EXIT_MENU:
                    if (ui_is_screensaver() || !s_isScreenOn) {
                        gui_report_activity(); // 唤醒屏幕
                    } else {
                        if (millis() - s_wakeupTime > 500) {
                            ui_exit_menu();
                        }
                    }
                    break;
                case UI_EVENT_BRIGHTNESS:
                    ui_update_brightness((uint8_t)evt.value);
                    break;
                case UI_EVENT_CCT:
                    ui_update_cct((uint16_t)evt.value);
                    break;
                case UI_EVENT_WIFI_IP:
                    ui_update_ip(s_ipBuffer);
                    break;
                case UI_EVENT_MQTT_STATE:
                    ui_update_mqtt_status(evt.value != 0);
                    break;
                case UI_EVENT_BATTERY:
                    ui_update_battery(evt.value);
                    // 自动省电逻辑：电池低于 30% 自动开启省电模式
                    // evt.value > 100 表示充电中，不触发
                    if (evt.value <= 30 && !s_powerSaveMode) {
                        gui_set_power_save_mode(true);
                        Serial.println("[Power] Battery < 30%, Auto-enabling Power Save Mode");
                    }
                    break;
                case UI_EVENT_TEMPERATURE:
                    ui_update_temperature(evt.fvalue);
                    break;
                case UI_EVENT_HUMIDITY:
                    ui_update_humidity(evt.fvalue);
                    break;
                case UI_EVENT_LUX:
                    ui_update_lux(evt.fvalue);
                    // 自动亮度逻辑
                    if (lamp.isAutoBrightness()) {
                        float lux = evt.fvalue;
                        uint8_t targetBr = 0;
                        
                        // 反向逻辑：环境越亮，灯光越暗；环境越暗，灯光越亮
                        if (lux <= 10) {
                            targetBr = 100; // 极暗环境 -> 最亮
                        } else if (lux >= 300) {
                            targetBr = 10;  // 极亮环境 -> 最暗 (保留一点亮度)
                        } else {
                            // 10-300 lux -> 100-10%
                            targetBr = map((long)lux, 10, 300, 100, 10);
                        }
                        
                        // 只有当变化超过一定阈值时才调整，避免频繁闪烁
                        if (abs(targetBr - lamp.getBrightness()) > 2) {
                            lamp.setBrightness(targetBr, 2000); // 2秒平滑过渡
                        }
                    }
                    break;
                case UI_EVENT_RADAR_DIST:
                    ui_update_radar_dist(evt.value);
                    break;
                case UI_EVENT_RADAR_STATE:
                    ui_update_radar_state(evt.value);
                    break;
                case UI_EVENT_WIFI_STATE:
                    // value: connected, fvalue: rssi (optional, or just use WiFi.RSSI() inside)
                    // For simplicity, let's assume value is connected state. 
                    // We can get RSSI inside ui_update_wifi_state if needed, or pass it.
                    // Let's pass RSSI via fvalue or just read it.
                    ui_update_wifi_state(evt.value != 0, (int)evt.fvalue); 
                    break;
                case UI_EVENT_BLE_STATE:
                    ui_update_ble_state(evt.value != 0);
                    break;
#include "screens/screen_lamp.hpp"

// ... inside switch ...
                case UI_EVENT_WEATHER: {
                    auto data = get_current_weather();
                    if (data.valid) {
                        ui_main_update_weather(data.city.c_str(), data.weather_text.c_str(), data.temp.c_str());
                    }
                    break;
                }
                case UI_EVENT_AUTO_BR:
                    ui_lamp_update_auto_brightness(evt.value != 0);
                    break;
                default:
                    break;
            }
        }

        // LVGL 核心处理（渲染、动画、定时器）
        lv_timer_handler();
        
        // 释放 CPU 给其他任务（LVGL 建议 5ms 左右）
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup_gui_task() {
    // 1. 创建事件队列 (深度 8)
    uiEventQueue = xQueueCreate(8, sizeof(UIEvent));
    if (!uiEventQueue) {
        Serial.println("[GUI] Failed to create event queue!");
    }

    // 2. 创建 GUI 任务
    // 优先级稍高 (2) 以保证界面流畅，栈空间较大 (6144) 以容纳 LVGL
    // 绑定到核心 0 (ESP32-C3 只有一个核心，但在双核 ESP32 上通常 GUI 跑在核心 1)
    xTaskCreatePinnedToCore(
        task_gui,
        "GUI Task",    
        8192, // Use larger stack for LVGL-intensive UI to avoid stack overflow
        NULL,
        2,
        &s_guiTaskHandle,
        0 // Core ID
    );
    
    Serial.println("[System] GUI task started");
}

void send_ui_event(const UIEvent& evt, uint8_t excludeMask) {
    // 发送到 GUI 队列
    if (uiEventQueue && !(excludeMask & DEST_GUI)) {
        xQueueSend(uiEventQueue, &evt, 0);
    }
    
    // 发送到 MQTT 队列
    if (mqttEventQueue && !(excludeMask & DEST_MQTT)) {
        xQueueSend(mqttEventQueue, &evt, 0);
    }
    
    // 发送到 BLE 队列
    if (bleEventQueue && !(excludeMask & DEST_BLE)) {
        xQueueSend(bleEventQueue, &evt, 0);
    }
}

