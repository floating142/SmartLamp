#include "network_manager.hpp"
#include "wifi_task.hpp"
#include "ble_task.hpp"
#include "mqtt_task.hpp"
#include "weather_task.hpp"
#include <WiFi.h>

NetworkManager& NetworkManager::instance() {
    static NetworkManager instance;
    return instance;
}

void NetworkManager::setup() {
    Serial.println("[Network] Starting Network Manager...");

    // 1. 启动 WiFi 任务
    // ESP32-C3 的 WiFi 和 BLE 共享 2.4GHz 射频模块。
    // 必须先初始化 WiFi 驱动，确保底层 RF 资源分配正确。
    setup_wifi_task();

    // 2. 关键延时：等待 WiFi 硬件初始化
    // 如果不加延时直接启动 BLE，可能会导致 RF 资源竞争，出现 error 0x3001 或 Malloc failed。
    // 给予 WiFi 驱动足够的时间进入稳定状态 (STA 模式)。
    Serial.println("[Network] Waiting for WiFi hardware initialization...");
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    // 3. 启动 BLE 服务
    // 使用 NimBLE 协议栈，内存占用更低。
    // 此时 WiFi 应该已经处于 STA 模式（即使未连接），RF 资源已就绪。
    setup_ble_service();

    // 4. 启动 MQTT 任务
    // MQTT 依赖 WiFi 连接，任务内部会处理连接等待逻辑。
    setup_mqtt_task();

    // 5. 启动天气任务
    setup_weather_task();

    Serial.println("[Network] All network services started.");
}

bool NetworkManager::isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void setup_network_manager() {
    NetworkManager::instance().setup();
}
