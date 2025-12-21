/**
 * @file ble_task.cpp
 * @brief BLE 配网任务实现
 * 
 * 负责启动 BLE 服务，接收手机端发送的 WiFi 和 MQTT 配置信息。
 * 收到配置后会保存到 NVS 并重启设备。
 */

#include "ble_task.hpp"
#include "ble_cmd.hpp"

// Project Headers
#include "../system/storage.hpp"
#include "../app/lamp.hpp"
#include "mqtt_task.hpp"
#include "wifi_task.hpp"
#include "../ui/gui_task.hpp"

// Arduino & System Headers
#include <Arduino.h>
#include <WiFi.h>
#include <NimBLEDevice.h>

QueueHandle_t bleEventQueue = nullptr;
static TaskHandle_t s_bleEventTaskHandle = nullptr;

// =================================================================================
// 配置与常量 (Configuration & Constants)
// =================================================================================

// 定义 BLE UUID
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_ENERGY_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a9" // 新增能量值特征

static NimBLECharacteristic* pEnergyCharacteristic = nullptr;
static NimBLECharacteristic* pConfigCharacteristic = nullptr;
static bool s_bleActive = false;

// =================================================================================
// 回调类定义 (Callback Classes)
// =================================================================================

/**
 * @brief BLE 服务器连接回调
 * 
 * 处理客户端连接和断开事件。
 * 断开连接后自动重新开始广播。
 */
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        (void)pServer;
        Serial.printf("[BLE] 客户端已连接: handle=%u, addr=%s\n",
                      connInfo.getConnHandle(), connInfo.getAddress().toString().c_str());
        
        // 发送 BLE 连接状态
        UIEvent evt = {UI_EVENT_BLE_STATE, 1, 0.0f};
        send_ui_event(evt);

        // 连接后不立即发送状态，等待客户端订阅后发送查询指令 "cmd:status"
        // 避免在客户端未订阅 Notify 时发送数据导致丢失
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("[BLE] 客户端断开: handle=%u, reason=0x%02X\n",
                      connInfo.getConnHandle(), reason);
        
        // 发送 BLE 断开状态
        UIEvent evt = {UI_EVENT_BLE_STATE, 0, 0.0f};
        send_ui_event(evt);

        // 重要：断开后必须重新开始广播，否则其他设备无法搜索到
        // 加上延时防止某些情况下协议栈未复位导致的崩溃
        vTaskDelay(pdMS_TO_TICKS(500));
        pServer->startAdvertising();
        Serial.println("[BLE] 重新开始广播 (Advertising restarted)");
    }
};

/**
 * @brief BLE 特征值写入回调
 * 
 * 处理客户端写入的数据 (增加缓冲区和结束符处理)。
 * 支持分包接收，以 '\n' 为指令结束符。
 */
class ConfigCallbacks: public NimBLECharacteristicCallbacks {
    std::string _rxBuffer; // 使用 std::string 提高效率

    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override {
        // 获取写入的数据 (std::string)
        std::string rxValue = pCharacteristic->getValue();
        
        if (rxValue.length() > 0) {
            _rxBuffer += rxValue;
            
            size_t pos = 0;
            while ((pos = _rxBuffer.find('\n')) != std::string::npos) {
                // 提取一条完整指令
                std::string cmd = _rxBuffer.substr(0, pos);
                // 从缓冲区移除已处理的部分
                _rxBuffer.erase(0, pos + 1);
                
                // 去除回车符 \r (兼容 \r\n)
                if (!cmd.empty() && cmd.back() == '\r') {
                    cmd.pop_back();
                }
                
                if (!cmd.empty()) {
                    ble_handle_command(cmd);
                }
            }
        }
    }
};

// =================================================================================
// 任务接口 (Task Interface)
// =================================================================================

bool is_ble_config_active() {
    return s_bleActive;
}

void task_ble_event_handler(void *pvParameters) {
    UIEvent evt;
    while (true) {
        if (bleEventQueue && xQueueReceive(bleEventQueue, &evt, portMAX_DELAY) == pdTRUE) {
            if (!s_bleActive) continue;
            
            char buf[64];
            switch (evt.type) {
                case UI_EVENT_LIGHT:
                    snprintf(buf, sizeof(buf), "st:%s", evt.value ? "on" : "off");
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_BRIGHTNESS:
                    snprintf(buf, sizeof(buf), "bri:%d", evt.value);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_CCT:
                    snprintf(buf, sizeof(buf), "cct:%d", evt.value);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_RGB:
                    snprintf(buf, sizeof(buf), "rgb:%d,%d,%d", 
                        (evt.value >> 16) & 0xFF, 
                        (evt.value >> 8) & 0xFF, 
                        evt.value & 0xFF);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_LUX:
                    snprintf(buf, sizeof(buf), "lux:%.1f", evt.fvalue);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_TEMPERATURE:
                    snprintf(buf, sizeof(buf), "tmp:%.1f", evt.fvalue);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_HUMIDITY:
                    snprintf(buf, sizeof(buf), "hum:%.1f", evt.fvalue);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_RADAR_DIST:
                    snprintf(buf, sizeof(buf), "dist:%d", evt.value);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_RADAR_STATE:
                    snprintf(buf, sizeof(buf), "mov:%d", evt.value);
                    ble_send_notify(buf);
                    break;
                case UI_EVENT_EFFECT:
                    {
                        const char* effStr = "none";
                        switch((EffectMode)evt.value) {
                            case EffectMode::Rainbow: effStr = "rainbow"; break;
                            case EffectMode::Breathing: effStr = "breathing"; break;
                            case EffectMode::Police: effStr = "police"; break;
                            default: effStr = "none"; break;
                        }
                        snprintf(buf, sizeof(buf), "eff:%s", effStr);
                        ble_send_notify(buf);
                    }
                    break;
                default:
                    break;
            }
            // 增加一个小延时，确保连续发送时不会拥塞 BLE 协议栈
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

/**
 * @brief 启动 BLE 服务
 * 
 * 初始化 NimBLE 协议栈，创建 GATT 服务和特征值，并开始广播。
 * 
 * 注意：
 * - 本函数应在 WiFi 初始化之后调用 (由 NetworkManager 保证顺序)。
 * - 采用 Coexistence 模式，WiFi 和 BLE 同时运行。
 */
void start_ble_config() {
    if (s_bleActive) return;

    if (!bleEventQueue) {
        bleEventQueue = xQueueCreate(8, sizeof(UIEvent));
    }
    
    if (!s_bleEventTaskHandle) {
        xTaskCreate(
            task_ble_event_handler,
            "BLE Event Task",
            2048,
            NULL,
            1,
            &s_bleEventTaskHandle
        );
    }

    // 简单的延时，确保之前的 RF 操作已稳定
    vTaskDelay(pdMS_TO_TICKS(100)); 

    Serial.println("[BLE] 正在初始化蓝牙协议栈...");

    uint64_t chipid = ESP.getEfuseMac(); // 获取芯片唯一 ID
    char idBuf[32];
    snprintf(idBuf, sizeof(idBuf), "deng_%08X", (uint32_t)chipid);
    std::string unique_id = idBuf;
    
    // 2. 初始化 BLE 设备
    // 检查是否已经初始化，避免重复初始化导致崩溃
    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init(unique_id); 
        NimBLEDevice::setDeviceName(unique_id);
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); 
    }

    // 调整 MTU 以支持雷达能量数据的批量传输
    NimBLEDevice::setMTU(247);
    
    // 创建服务器
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks()); // 设置连接/断开回调

    // 创建服务
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    
    // 创建特征值 (用于接收 WiFi 账号密码)
    // PROPERTY_WRITE_NR (No Response): 允许手机“盲发”数据，提高兼容性
    // PROPERTY_NOTIFY: 允许设备主动上报状态
    pConfigCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         NIMBLE_PROPERTY::READ | 
                                         NIMBLE_PROPERTY::WRITE |
                                         NIMBLE_PROPERTY::WRITE_NR |
                                         NIMBLE_PROPERTY::NOTIFY
                                       );
    
    pConfigCharacteristic->setCallbacks(new ConfigCallbacks()); // 设置写入回调

    // 创建能量值特征值 (Notify)
    pEnergyCharacteristic = pService->createCharacteristic(
                                         CHAR_ENERGY_UUID,
                                         NIMBLE_PROPERTY::READ | 
                                         NIMBLE_PROPERTY::NOTIFY
                                       );
    
    // 启动服务
    pService->start();
    
    // 3. 配置广播参数
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    
    // 针对 iOS 设备的连接优化参数 (0x06 = 7.5ms, 0x12 = 22.5ms)
    pAdvertising->setPreferredParams(0x06, 0x12);
    
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.addServiceUUID(SERVICE_UUID);
    pAdvertising->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setName(unique_id, true);
    pAdvertising->setScanResponseData(scanData);
    
    // 开始广播
    pAdvertising->start();
    
    s_bleActive = true;

    Serial.println("[BLE] 蓝牙服务已启动 (Coexistence Mode)");
}

void setup_ble_service() {
    start_ble_config();
}

void ble_update_radar_energy(const uint32_t* energy) {
    if (pEnergyCharacteristic && s_bleActive) {
        // NimBLE 的 notify() 内部会自动检查是否有订阅者。
        // 如果没有订阅者，它不会发送数据，因此这里直接调用是安全的且高效的。
        pEnergyCharacteristic->setValue((uint8_t*)energy, 32 * sizeof(uint32_t));
        pEnergyCharacteristic->notify();
    }
}

void ble_send_notify(const char* msg) {
    if (pConfigCharacteristic && s_bleActive) {
        pConfigCharacteristic->setValue(msg);
        pConfigCharacteristic->notify();
    }
}

void toggle_ble_config() {
    // Deprecated: BLE is now always on (kept for compatibility)
    Serial.println("[BLE] toggle_ble_config() is deprecated; BLE stays enabled.");
}
