#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

/**
 * @brief MQTT 主题集合
 */
struct MqttTopics {
    String prefix;
    String availability;
    String state;           // JSON 全量状态
    String switch_set;
    String switch_state;    // 独立开关状态
    String brightness_set;
    String cct_set;
    String rgb_set;
    String effect_set;
    String scene_set;       // 场景设置
    
    String system_set;      // 系统控制
    String system_info;     // 系统信息 (JSON)

    String sensor_lux;
    String sensor_temp;
    String sensor_humi;
};

/**
 * @brief 设备信息
 */
struct DeviceInfo {
    String chipId;
    String nodeId;      // unique_id
    String name;        // Friendly name
    String model;
    String manufacturer;
};

/**
 * @brief 发布 Home Assistant 自动发现配置 (传感器)
 */
void ha_publish_sensor_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics);

/**
 * @brief 发布 Home Assistant 自动发现配置 (灯光)
 */
void ha_publish_light_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics);

/**
 * @brief 发布 Home Assistant 自动发现配置 (系统实体: 按钮、诊断信息)
 */
void ha_publish_system_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics);
