#include "weather_task.hpp"
#include "network_manager.hpp"
#include "../ui/gui_task.hpp"
#include "../system/storage.hpp"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// =================================================================================
// 全局变量 (Global Variables)
// =================================================================================

static TaskHandle_t s_weatherTaskHandle = nullptr;
static WeatherData s_currentData;
static bool s_forceUpdate = false;
static unsigned long s_lastUpdateTime = 0;

// =================================================================================
// 配置与常量 (Configuration & Constants)
// =================================================================================

static const unsigned long INTERVAL_NORMAL = 10 * 60 * 1000; // 10分钟
static const unsigned long INTERVAL_POWERSAVE = 60 * 60 * 1000; // 1小时
static const int MAX_RETRIES = 3;
static const unsigned long RETRY_DELAY = 5000; // 5秒

// =================================================================================
// 内部函数 (Internal Functions)
// =================================================================================

static bool fetch_weather() {
    WiFiClient client;
    HTTPClient http;
    
    float lat, lon;
    String city;
    AppConfig::instance().loadWeatherConfig(lat, lon, city);

    // 使用 Open-Meteo API (无需 Key)
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) + 
                 "&longitude=" + String(lon, 4) + 
                 "&current=temperature_2m,weather_code&timezone=auto";
    
    // Serial.println("[Weather] Fetching: " + url);
    
    http.begin(client, url);
    int httpCode = http.GET();
    bool success = false;

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                float temp = doc["current"]["temperature_2m"];
                int code = doc["current"]["weather_code"];
                
                s_currentData.temp = String(temp, 1);
                s_currentData.valid = true;
                
                if (code == 0) s_currentData.weather_text = "Sunny";
                else if (code >= 1 && code <= 3) s_currentData.weather_text = "Cloudy";
                else if (code >= 45 && code <= 48) s_currentData.weather_text = "Foggy";
                else if (code >= 51 && code <= 67) s_currentData.weather_text = "Rainy";
                else if (code >= 71 && code <= 77) s_currentData.weather_text = "Snowy";
                else s_currentData.weather_text = "Unknown";

                s_currentData.city = city; 
                success = true;
                Serial.println("[Weather] Update success: " + s_currentData.weather_text + ", " + s_currentData.temp + "C");
            } else {
                Serial.print("[Weather] JSON Error: ");
                Serial.println(error.c_str());
            }
        } else {
            Serial.print("[Weather] HTTP Error: ");
            Serial.println(httpCode);
        }
    } else {
        Serial.print("[Weather] Connection Failed: ");
        Serial.println(http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}

static void task_weather(void *pvParameters) {
    Serial.println("[Weather] Task started");
    
    // 初始配置加载
    float lat, lon;
    String city;
    if (!AppConfig::instance().loadWeatherConfig(lat, lon, city)) {
        AppConfig::instance().saveWeatherConfig(39.9042, 116.4074, "Beijing");
    }

    while (true) {
        // 1. 检查网络状态
        if (!NetworkManager::instance().isWifiConnected()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 2. 检查屏幕状态 (屏幕熄灭时暂停更新)
        if (!gui_is_screen_on()) {
            // 暂停期间每 5 秒检查一次屏幕状态
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 3. 确定更新间隔
        unsigned long interval = gui_is_power_save_mode() ? INTERVAL_POWERSAVE : INTERVAL_NORMAL;
        
        // 4. 检查是否需要更新
        bool timeToUpdate = (millis() - s_lastUpdateTime > interval) || (s_lastUpdateTime == 0);
        
        if (s_forceUpdate || timeToUpdate) {
            int retries = 0;
            bool success = false;
            
            while (retries < MAX_RETRIES && !success) {
                if (retries > 0) {
                    Serial.printf("[Weather] Retry %d/%d...\n", retries, MAX_RETRIES);
                    vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY));
                }
                
                success = fetch_weather();
                retries++;
            }

            if (success) {
                s_lastUpdateTime = millis();
                s_forceUpdate = false;
                
                // 发送更新事件
                UIEvent evt;
                evt.type = UI_EVENT_WEATHER;
                send_ui_event(evt);
            } else {
                Serial.println("[Weather] Update failed after retries");
                // 失败后，如果是强制更新，则重置标志，避免死循环重试
                s_forceUpdate = false; 
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// =================================================================================
// 外部接口 (External Interface)
// =================================================================================

void setup_weather_task() {
    xTaskCreatePinnedToCore(
        task_weather,
        "Weather Task",
        2048,
        NULL,
        1,
        &s_weatherTaskHandle,
        0
    );
}

WeatherData get_current_weather() {
    return s_currentData;
}

void weather_force_update() {
    s_forceUpdate = true;
}
