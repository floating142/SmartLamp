#pragma once

#include <Arduino.h>

/**
 * @brief 网络管理器
 * 
 * 统一管理 WiFi, BLE, MQTT 等网络任务的启动顺序和生命周期。
 * 解决 ESP32-C3 单天线共存问题，确保 WiFi 初始化完成后再启动 BLE。
 */
class NetworkManager {
public:
    static NetworkManager& instance();

    /**
     * @brief 初始化并启动所有网络服务
     * 
     * 启动顺序:
     * 1. WiFi (STA 模式, 节能配置)
     * 2. 等待 WiFi 硬件初始化
     * 3. BLE (NimBLE 协议栈)
     * 4. MQTT (连接云平台)
     */
    void setup();

    /**
     * @brief 检查网络是否就绪
     */
    bool isWifiConnected();

private:
    NetworkManager() = default;
    ~NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
};

// 全局便捷访问函数
void setup_network_manager();
