/**
 * @file ble_cmd.cpp
 * @brief BLE 指令处理实现
 */

#include "ble_cmd.hpp"
#include "ble_task.hpp"
#include "weather_task.hpp"
#include "wifi_task.hpp"
#include "../app/lamp.hpp"
#include "../system/storage.hpp"
#include "../ui/gui_task.hpp"

// Arduino Headers
#include <Arduino.h>
#include <WiFi.h>

// =================================================================================
// 内部辅助函数声明
// =================================================================================

static void handle_control_cmd(const char* str);
static void handle_config_cmd(const String& cmdStr);
static void send_status_report();

// =================================================================================
// 指令处理入口
// =================================================================================

void ble_handle_command(const std::string& cmd) {
    const char* str = cmd.c_str();

    // 1. 亮度控制 "bri:50"
    if (strncmp(str, "bri:", 4) == 0) {
        int val = atoi(str + 4);
        lamp.setBrightness(val, 500, DEST_BLE); 
        return;
    }
    
    // 2. 色温控制 "cct:3000"
    if (strncmp(str, "cct:", 4) == 0) {
        int val = atoi(str + 4);
        lamp.setCCT(val, 500, DEST_BLE); 
        return;
    }
    
    // 3. RGB控制 "rgb:255,0,0"
    if (strncmp(str, "rgb:", 4) == 0) {
        int r, g, b;
        if (sscanf(str + 4, "%d,%d,%d", &r, &g, &b) == 3) {
            lamp.setColor(r, g, b, 500, DEST_BLE); 
        }
        return;
    }

    // 4. 开关与状态 "cmd:on" / "cmd:off" / "cmd:status"
    if (strncmp(str, "cmd:", 4) == 0) {
        handle_control_cmd(str + 4);
        return;
    }

    // 5. 特效控制 "eff:rainbow"
    if (strncmp(str, "eff:", 4) == 0) {
        lamp.setEffect(str + 4);
        return;
    }

    // 6. 场景控制 "scn:reading"
    if (strncmp(str, "scn:", 4) == 0) {
        lamp.setScene(str + 4, DEST_BLE);
        return;
    }

    // 7. 配置指令 (WiFi/MQTT/Weather)
    // 使用 String 类处理较复杂的字符串操作
    handle_config_cmd(String(str));
}

// =================================================================================
// 辅助逻辑实现
// =================================================================================

static void handle_control_cmd(const char* action) {
    if (strcmp(action, "on") == 0) {
        lamp.setPower(true, 500, DEST_BLE);
    } else if (strcmp(action, "off") == 0) {
        lamp.setPower(false, 500, DEST_BLE);
    } else if (strcmp(action, "status") == 0) {
        send_status_report();
    }
}

static void send_status_report() {
    const TickType_t sendTimeout = pdMS_TO_TICKS(10);
    // 主动上报所有状态
    // 将状态上报请求放入队列，由 task_ble_event_handler 统一处理
    UIEvent evt;
    
    // 1. 开关状态
    evt.type = UI_EVENT_LIGHT;
    evt.value = lamp.isOn() ? 1 : 0;
    xQueueSend(bleEventQueue, &evt, sendTimeout);
    
    // 2. 亮度
    evt.type = UI_EVENT_BRIGHTNESS;
    evt.value = lamp.getBrightness();
    xQueueSend(bleEventQueue, &evt, sendTimeout);
    
    // 3. 模式与颜色
    if (lamp.isCCTMode()) {
        evt.type = UI_EVENT_CCT;
        evt.value = lamp.getCCT();
        xQueueSend(bleEventQueue, &evt, sendTimeout);
    } else {
        evt.type = UI_EVENT_RGB;
        CRGB rgb = lamp.getRGB();
        evt.value = ((uint32_t)rgb.r << 16) | ((uint32_t)rgb.g << 8) | rgb.b;
        xQueueSend(bleEventQueue, &evt, sendTimeout);
    }
    
    // 4. 特效
    evt.type = UI_EVENT_EFFECT;
    evt.value = (int)lamp.getEffect();
    xQueueSend(bleEventQueue, &evt, sendTimeout);
}

static void handle_config_cmd(const String& cmdStr) {
    Serial.print("[BLE] 处理配置指令: ");
    Serial.println(cmdStr);

    // WiFi 配置: "wifi:SSID,PASSWORD"
    if (cmdStr.startsWith("wifi:")) {
        String data = cmdStr.substring(5);
        int commaIndex = data.indexOf(',');
        if (commaIndex != -1) {
            String ssid = data.substring(0, commaIndex);
            String pass = data.substring(commaIndex + 1);
            
            ssid.trim();
            pass.trim();

            if (ssid.length() > 0) {
                Serial.printf("[BLE] Adding WiFi Network: SSID=%s\n", ssid.c_str());
                AppConfig::instance().addWifi(ssid, pass);
                
                Serial.println("[BLE] Network added! Reloading WiFi...");
                wifi_reload_config();
            }
        } else {
            Serial.println("[BLE] WiFi 格式错误! 请发送: 'wifi:SSID,PASSWORD'");
        }
    }
    // WiFi 删除: "wifi_remove:SSID"
    else if (cmdStr.startsWith("wifi_remove:")) {
        String ssid = cmdStr.substring(12);
        ssid.trim();
        if (ssid.length() > 0) {
            Serial.printf("[BLE] Removing WiFi Network: SSID=%s\n", ssid.c_str());
            AppConfig::instance().removeWifi(ssid);
            Serial.println("[BLE] Network removed! Reloading WiFi...");
            wifi_reload_config();
        }
    }
    // WiFi 清空: "wifi_clear:"
    else if (cmdStr.startsWith("wifi_clear:")) {
        Serial.println("[BLE] Clearing all WiFi networks...");
        AppConfig::instance().clearWifiList();
        Serial.println("[BLE] All networks cleared! Reloading WiFi...");
        wifi_reload_config();
    }
    // MQTT 配置: "mqtt:HOST,PORT,USER,PASS"
    else if (cmdStr.startsWith("mqtt:")) {
        String data = cmdStr.substring(5);
        
        int c1 = data.indexOf(',');
        int c2 = data.indexOf(',', c1 + 1);
        int c3 = data.indexOf(',', c2 + 1);
        
        if (c1 != -1 && c2 != -1 && c3 != -1) {
            String host = data.substring(0, c1);
            String portStr = data.substring(c1 + 1, c2);
            String user = data.substring(c2 + 1, c3);
            String pass = data.substring(c3 + 1);
            
            host.trim();
            portStr.trim();
            user.trim();
            pass.trim();
            
            int port = portStr.toInt();
            
            if (host.length() > 0 && port > 0) {
                Serial.printf("[BLE] 保存 MQTT: %s:%d\n", host.c_str(), port);
                AppConfig::instance().saveMQTT(host, port, user, pass);
                Serial.println("[BLE] MQTT 配置已保存! 重启生效...");
                delay(2000);
                ESP.restart();
            }
        } else {
            Serial.println("[BLE] MQTT 格式错误! 请发送: 'mqtt:HOST,PORT,USER,PASS'");
        }
    }
    // 天气配置: "weather:LAT,LON,CITY"
    else if (cmdStr.startsWith("weather:")) {
        String params = cmdStr.substring(8);
        // 解析 lat,lon,city
        int firstComma = params.indexOf(',');
        int secondComma = params.indexOf(',', firstComma + 1);
        
        if (firstComma > 0 && secondComma > 0) {
            float lat = params.substring(0, firstComma).toFloat();
            float lon = params.substring(firstComma + 1, secondComma).toFloat();
            String city = params.substring(secondComma + 1);
            
            AppConfig::instance().saveWeatherConfig(lat, lon, city);
            // 触发天气更新
            weather_force_update();
        }
    }
    // 自动亮度: "autobr:1" or "autobr:0"
    else if (cmdStr.startsWith("autobr:")) {
        int val = cmdStr.substring(7).toInt();
        lamp.setAutoBrightness(val != 0);
        Serial.printf("[BLE] Auto Brightness: %d\n", val);
    }
    else {
        Serial.println("[BLE] 未知指令!");
    }
}
