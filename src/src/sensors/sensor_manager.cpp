#include "sensor_manager.hpp"
#include "sht4x.hpp"
#include "bh1750.hpp"
#include "cw2015.hpp"
#include "ld2410d.hpp"
#include "../ui/gui_task.hpp"
#include "../network/ble_task.hpp"
#include "../system/storage.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Radar definitions
#define RADAR_RX_PIN 20
#define RADAR_TX_PIN 21
HardwareSerial RadarSerial(1);
Sensor::LD2410D radar(RadarSerial);
static TaskHandle_t s_radarTaskHandle = NULL;

void sensor_set_radar_enable(bool enable) {
    if (s_radarTaskHandle == NULL) return;

    if (enable) {
        vTaskResume(s_radarTaskHandle);
    } else {
        vTaskSuspend(s_radarTaskHandle);
    }
    
    // Save to NVS
    AppConfig::instance().saveRadarEnable(enable);
}

static void task_radar(void *pvParameters) {
    (void)pvParameters;
    
    // Load initial state
    bool radarEnabled = true;
    AppConfig::instance().loadRadarEnable(radarEnabled);
    
    if (!radarEnabled) {
        // If disabled, suspend self immediately
        // Note: We must do this carefully. If we suspend here, we might not initialize serial.
        // But initialization is good to have. Let's initialize first.
    }

    RadarSerial.begin(115200, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
    radar.begin();
    
    // Optional: Enable engineering mode if needed, or just use basic mode
    if (radar.enableConfiguration()) {
        if (radar.setEngineeringMode(true)) {
            Serial.println("[Radar] Engineering Mode Set: ON");
        }
        radar.endConfiguration();
    } else {
        Serial.println("[Radar] Failed to enable configuration mode");
    }
    
    if (!radarEnabled) {
        Serial.println("[Radar] Disabled by settings, suspending task...");
        vTaskSuspend(NULL); // Suspend self
    }

    //radar.setDebugStream(&Serial); // Enable debug output if needed

    TickType_t lastReportTime = 0;
    TickType_t lastWakeTick = xTaskGetTickCount();
    const TickType_t pollInterval = pdMS_TO_TICKS(10);

    for (;;) {
        radar.update();
        
        // Report data every 200ms
        if (xTaskGetTickCount() - lastReportTime > pdMS_TO_TICKS(200)) {
            lastReportTime = xTaskGetTickCount();
            
            if (radar.hasTarget()) {
                const auto& data = radar.getData();

                // Send Distance
                UIEvent evtDist;
                evtDist.type = UI_EVENT_RADAR_DIST;
                evtDist.value = data.distance_cm;
                send_ui_event(evtDist);

                // Send State
                UIEvent evtState;
                evtState.type = UI_EVENT_RADAR_STATE;
                evtState.value = (int)data.state;
                send_ui_event(evtState);

                // Send Energy to BLE
                ble_update_radar_energy(data.gate_energy);
            } else {
                // Send No Target State
                UIEvent evtState;
                evtState.type = UI_EVENT_RADAR_STATE;
                evtState.value = 0; // No Target
                send_ui_event(evtState);
            }
        }

        vTaskDelayUntil(&lastWakeTick, pollInterval);
    }
}

static void task_sensor_manager(void *pvParameters) {
    (void)pvParameters;

    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));

    Serial.println("[Sensor] Initializing sensors...");

    // 初始化各个传感器
    bool sht4x_ok = sht4x_init();
    bool bh1750_ok = bh1750_init();
    bool cw2015_ok = cw2015_init();

    Serial.printf("[Sensor] Init Results: SHT4x=%d, BH1750=%d, CW2015=%d\n", sht4x_ok, bh1750_ok, cw2015_ok);

    TickType_t lastWakeTick = xTaskGetTickCount();

    for (;;) {
        // 依次读取传感器，读取完成后由此处统一上报 UI 事件
        if (sht4x_ok) {
            sht4x_read();
            if (sht4x_has_reading()) {
                UIEvent evtT{};
                evtT.type = UI_EVENT_TEMPERATURE;
                evtT.fvalue = sht4x_get_temperature();
                send_ui_event(evtT);

                UIEvent evtH{};
                evtH.type = UI_EVENT_HUMIDITY;
                evtH.fvalue = sht4x_get_humidity();
                send_ui_event(evtH);
            }
        }
        // 传感器之间稍微间隔一下，避免瞬间占用 I2C 总线太久
        vTaskDelay(pdMS_TO_TICKS(50)); 

        if (bh1750_ok) {
            bh1750_read();
            if (bh1750_has_reading()) {
                UIEvent evt{};
                evt.type = UI_EVENT_LUX;
                evt.fvalue = bh1750_get_lux();
                send_ui_event(evt);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));

        if (cw2015_ok) {
            cw2015_read();
            int batt = cw2015_take_ui_value_if_changed();
            if (batt >= 0) {
                UIEvent evt{};
                evt.type = UI_EVENT_BATTERY;
                evt.value = batt;
                send_ui_event(evt);
            }
        }

        // 采样间隔控制
        // 默认 2 秒 (比之前的 1s/5s 综合一下)
        uint32_t delay_ms = 2000;
        
        // 如果处于省电模式且屏幕关闭，大幅降低采样频率 (例如 1分钟)
        if (gui_is_power_save_mode() && !gui_is_screen_on()) {
            delay_ms = 60000;
        }

        if (delay_ms < 10) delay_ms = 10;

        vTaskDelayUntil(&lastWakeTick, pdMS_TO_TICKS(delay_ms));
    }
}

void setup_sensor_manager_task() {
    xTaskCreate(
        task_sensor_manager,
        "Sensor Manager",
        2048, 
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    xTaskCreate(
        task_radar,
        "Radar Task",
        2560,
        NULL,
        tskIDLE_PRIORITY + 2, // Higher priority for serial handling
        &s_radarTaskHandle
    );
}
