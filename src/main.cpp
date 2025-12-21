/**
 * @file deng.ino
 * @brief 主程序入口
 * 
 * 负责系统初始化、硬件配置以及启动各个功能任务。
 * 基于 FreeRTOS 多任务架构。
 */

#include <Arduino.h>
#include <Wire.h>

// ---- System Modules ----
#include "src/system/i2c_manager.hpp"
#include "src/system/storage.hpp"
#include "src/system/rtc_task.hpp"

// ---- App Modules ----
#include "src/app/lamp.hpp"

// ---- UI Modules ----
#include "src/ui/gui_task.hpp"

// ---- Input Modules ----
#include "src/input/gpio_button.hpp"
#include "src/input/adc_keypad.hpp"

// ---- Sensor Modules ----
#include "src/sensors/sensor_manager.hpp"

// ---- Network Modules ----
#include "src/network/network_manager.hpp"
#include "src/ui/screens/screen_main.hpp"

void setup() {
    // 1. 基础系统初始化
    Serial.begin(115200);
    
    // I2C 初始化 (SDA=6, SCL=7)
    Wire.begin(6, 7);
    setup_i2c_manager(); // 创建 I2C 互斥锁
    
    // 2. 硬件驱动初始化
    // 显示屏 (LGFX)
    tft.init();
    tft.setRotation(0);
    tft.setBrightness(255);

    // 3. 启动 UI 任务 (最优先启动，显示开机动画)
    // 注意：GUI 任务会接管屏幕刷新
    setup_gui_task();
    
    // 4. 智能等待串口连接 (Debug 模式)
    // 检查 NVS 配置中的 Debug 模式开关
    bool debugMode = false;
    AppConfig::instance().loadDebugMode(debugMode);

    if (debugMode) {
        Serial.println("\n[Boot] Debug Mode Enabled (NVS). Waiting for Serial...");
        
        unsigned long startWait = millis();
        while (!Serial && (millis() - startWait < 5000)) { // 最多等 5 秒
            delay(10);
        }
        if (Serial) Serial.println("[Boot] Serial Connected");
    }

    Serial.println("\n\n[Boot] System Starting...");
    Serial.println("[Boot] I2C Initialized");
    Serial.println("[Boot] Display Initialized");
    Serial.println("[Boot] GUI Task Started");

    // 灯光控制 (FastLED)
    lamp.init();
    lamp.startTask();
    Serial.println("[Boot] Lamp Control Started");

    // 5. 启动传感器任务 (低优先级)
    setup_sensor_manager_task(); // 统一传感器管理
    Serial.println("[Boot] Sensor Manager Started");

    // 6. 启动系统服务任务
    setup_rtc_task();    // RTC 时间同步

    // 7. 启动输入任务
    setup_button_task();       // GPIO 按键
    setup_analog_input_task(); // ADC 键盘
    Serial.println("[Boot] Input Tasks Started");

    // 8. 启动网络任务 (统一管理)
    // NetworkManager 负责处理 WiFi/BLE 共存逻辑和启动顺序
    setup_network_manager();
    Serial.println("[Boot] Network Tasks Started");

    Serial.println("[Boot] Setup Complete. Entering Loop.");
    
    // 10. 通知 UI 启动完成，可以结束开机动画
    UIEvent bootEvt;
    bootEvt.type = UI_EVENT_BOOT_COMPLETE;
    bootEvt.value = 0;
    send_ui_event(bootEvt);
}

void loop() {
    vTaskDelete(NULL); 
}
