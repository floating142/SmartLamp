/**
 * @file wifi_task.cpp
 * @brief WiFi 连接任务实现
 * 
 * 负责处理 WiFi 连接、断线重连以及 NTP 时间同步。
 * 启动时尝试从 NVS 读取配置，如果无配置则进入等待状态。
 */

#include "wifi_task.hpp"

// Project Headers
#include "../system/storage.hpp"
#include "../ui/gui_task.hpp"

// Arduino & System Headers
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "time.h"
#include "esp_sntp.h"

// =================================================================================
// 配置与常量 (Configuration & Constants)
// =================================================================================

// NTP 服务器配置
#define NTP_SERVER1 "time.windows.com"
#define NTP_SERVER2 "ntp.ntsc.ac.cn"
#define GMT_OFFSET_SEC 8 * 3600  // 东八区 (北京时间)
#define DAYLIGHT_OFFSET_SEC 0

// =================================================================================
// 全局变量 (Global Variables)
// =================================================================================

static TaskHandle_t s_wifiTaskHandle = nullptr;
static volatile bool s_reloadConfig = false;

void wifi_reload_config() {
    s_reloadConfig = true;
}

// =================================================================================
// 任务实现 (Task Implementation)
// =================================================================================

/**
 * @brief WiFi 主任务
 * 
 * 1. 加载 WiFi 配置
 * 2. 连接 WiFi
 * 3. 维护连接状态 (自动重连)
 * 4. 同步 NTP 时间
 */
static void task_wifi(void *pvParameters) {
    Serial.println("[WiFi] Task started");

    WiFiMulti *wifiMulti = new WiFiMulti();
    bool hasConfig = false;
    bool isConnected = false;

    // Helper lambda to load config
    auto loadConfig = [&]() {
        if (wifiMulti) delete wifiMulti;
        wifiMulti = new WiFiMulti();
        
        std::vector<AppConfig::WifiCred> wifiList;
        AppConfig::instance().loadWifiList(wifiList);
        
        Serial.printf("[WiFi] Loaded %d networks.\n", wifiList.size());
        for (const auto& cred : wifiList) {
            wifiMulti->addAP(cred.ssid.c_str(), cred.pass.c_str());
            Serial.printf("[WiFi] Added AP: %s\n", cred.ssid.c_str());
        }
        return !wifiList.empty();
    };

    // Initial load
    hasConfig = loadConfig();
    if (!hasConfig) {
        Serial.println("[WiFi] No credentials found. Waiting for BLE config...");
    }

    WiFi.mode(WIFI_STA);
    
    // 优化：降低 WiFi 发射功率 (约 11dBm)，减少电流峰值，缓解与 BLE 共存时的电源压力
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // 开启 WiFi 节能模式 (Modem Sleep)
    WiFi.setSleep(true);

    for (;;) {
        // Check for reload request
        if (s_reloadConfig) {
            s_reloadConfig = false;
            Serial.println("[WiFi] Reloading config requested...");
            WiFi.disconnect();
            isConnected = false;
            hasConfig = loadConfig();
            
            // Update UI state to disconnected immediately
            UIEvent evtState = {UI_EVENT_WIFI_STATE, 0, 0.0f};
            send_ui_event(evtState);
        }

        if (!hasConfig) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // 3. 检查连接状态 (使用 WiFiMulti)
        // wifiMulti.run() 会自动扫描并连接信号最强的已知网络
        if (wifiMulti->run() == WL_CONNECTED) {
            if (!isConnected) {
                isConnected = true;
                Serial.println("[WiFi] Connected!");
                Serial.print("[WiFi] SSID: ");
                Serial.println(WiFi.SSID());
                Serial.print("[WiFi] IP: ");
                Serial.println(WiFi.localIP());

                // 更新 UI 显示 IP
                String ip = WiFi.localIP().toString();
                strncpy(s_ipBuffer, ip.c_str(), 15);
                s_ipBuffer[15] = '\0';
                
                UIEvent evt = {UI_EVENT_WIFI_IP, 0};
                send_ui_event(evt);

                // 发送 WiFi 连接状态
                UIEvent evtState = {UI_EVENT_WIFI_STATE, 1, (float)WiFi.RSSI()};
                send_ui_event(evtState);

                // 4. 配置 NTP 同步间隔
                sntp_set_sync_interval(10 * 60 * 1000);

                // 5. 连接成功后配置时间同步
                configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
                Serial.println("[WiFi] NTP Sync started (Interval: 10min)...");
            }
            
            // 周期性更新 RSSI (每 10 秒)
            static unsigned long lastRssiUpdate = 0;
            if (millis() - lastRssiUpdate > 10000) {
                lastRssiUpdate = millis();
                UIEvent evtState = {UI_EVENT_WIFI_STATE, 1, (float)WiFi.RSSI()};
                send_ui_event(evtState, DEST_GUI); // 仅更新 GUI
            }

            // 已连接，每 1 秒检查一次
            vTaskDelay(pdMS_TO_TICKS(1000));

        } else {
            if (isConnected) {
                isConnected = false;
                Serial.println("[WiFi] Disconnected! WiFiMulti will try to reconnect...");
                
                UIEvent evtState = {UI_EVENT_WIFI_STATE, 0, 0.0f};
                send_ui_event(evtState);
            }
            // 未连接时，run() 会尝试连接，这里只需延时
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/**
 * @brief 启动 WiFi 任务
 */
void setup_wifi_task() {
    xTaskCreate(
        task_wifi,
        "WiFi Task",
        3072, // WiFi 栈需要较大内存
        NULL,
        1,    // 优先级较低，不影响 UI 和灯光
        &s_wifiTaskHandle
    );
}

/**
 * @brief 停止 WiFi 任务并关闭 WiFi 硬件
 * 
 * 通常在启动 BLE 配网前调用，以避免无线电冲突。
 */
void stop_wifi_task() {
    if (s_wifiTaskHandle != nullptr) {
        vTaskDelete(s_wifiTaskHandle);
        s_wifiTaskHandle = nullptr;
        Serial.println("[WiFi] Task stopped");
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
