#include "mqtt_ha.hpp"

/**
 * @brief 辅助函数：发送单个传感器的配置
 */
static void send_sensor_config(PubSubClient& client, const DeviceInfo& dev, 
                               const char* component, const char* object_id, 
                               const char* name, const char* dev_class, 
                               const char* unit, const String& state_topic, 
                               const char* value_tpl, const String& avail_topic) {
    
    String topic = "homeassistant/" + String(component) + "/" + dev.nodeId + "/" + object_id + "/config";
    
    String payload;
    payload.reserve(512);
    payload = "{";
    payload += "\"name\":\"" + String(name) + "\",";
    payload += "\"uniq_id\":\"" + dev.nodeId + "_" + object_id + "\",";
    payload += "\"stat_t\":\"" + state_topic + "\",";
    
    if (avail_topic.length() > 0) {
        payload += "\"avty_t\":\"" + avail_topic + "\",";
    }

    if (dev_class) { payload += "\"dev_cla\":\"" + String(dev_class) + "\","; }
    if (unit) { payload += "\"unit_of_meas\":\"" + String(unit) + "\","; }
    if (value_tpl) { payload += "\"val_tpl\":\"" + String(value_tpl) + "\","; }
    
    // 设备信息
    payload += "\"dev\":{";
    payload += "\"ids\":[\"" + dev.nodeId + "\"],";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"mdl\":\"" + dev.model + "\",";
    payload += "\"mf\":\"" + dev.manufacturer + "\"";
    payload += "}}";
    
    client.publish(topic.c_str(), payload.c_str(), true);
}

void ha_publish_sensor_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics) {
    if (!client.connected()) return;

    // 1. 光照传感器
    send_sensor_config(client, dev, "sensor", "lux", "Illuminance", "illuminance", "lx", 
                       topics.sensor_lux, nullptr, topics.availability);

    // 2. 温度传感器
    send_sensor_config(client, dev, "sensor", "temp", "Temperature", "temperature", "°C", 
                       topics.sensor_temp, nullptr, topics.availability);

    // 3. 湿度传感器
    send_sensor_config(client, dev, "sensor", "humi", "Humidity", "humidity", "%", 
                       topics.sensor_humi, nullptr, topics.availability);
}

void ha_publish_light_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics) {
    if (!client.connected()) return;

    String discovery_topic = "homeassistant/light/" + dev.nodeId + "/config";
    
    String payload;
    payload.reserve(1536);
    
    payload = "{";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"uniq_id\":\"" + dev.nodeId + "_light\",";
    
    payload += "\"avty_t\":\"" + topics.availability + "\",";
    payload += "\"cmd_t\":\"" + topics.switch_set + "\",";
    payload += "\"stat_t\":\"" + topics.switch_state + "\",";
    
    payload += "\"bri_cmd_t\":\"" + topics.brightness_set + "\",";
    payload += "\"bri_stat_t\":\"" + topics.state + "\",";
    payload += "\"bri_val_tpl\":\"{{ value_json.brightness }}\",";
    payload += "\"bri_scl\":100,";
    
    payload += "\"color_mode\":true,";
    payload += "\"supported_color_modes\":[\"color_temp\",\"rgb\"],";
    
    payload += "\"clrm_stat_t\":\"" + topics.state + "\",";
    payload += "\"clrm_val_tpl\":\"{{ value_json.color_mode }}\",";
    
    payload += "\"clr_temp_cmd_t\":\"" + topics.cct_set + "\",";
    payload += "\"clr_temp_stat_t\":\"" + topics.state + "\",";
    payload += "\"clr_temp_val_tpl\":\"{{ (1000000 / value_json.cct) | int }}\",";
    payload += "\"min_mireds\":153,"; 
    payload += "\"max_mireds\":370,"; 
    
    payload += "\"rgb_cmd_t\":\"" + topics.rgb_set + "\",";
    payload += "\"rgb_stat_t\":\"" + topics.state + "\",";
    payload += "\"rgb_val_tpl\":\"{{ value_json.rgb.r }},{{ value_json.rgb.g }},{{ value_json.rgb.b }}\",";
    
    // Device Registry
    payload += "\"dev\":{";
    payload += "\"ids\":[\"" + dev.nodeId + "\"],";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"mdl\":\"" + dev.model + "\",";
    payload += "\"mf\":\"" + dev.manufacturer + "\"";
    payload += "}}";
    
    if (client.publish(discovery_topic.c_str(), payload.c_str(), true)) {
        Serial.println("[MQTT] Light discovery config published.");
    } else {
        Serial.println("[MQTT] Failed to publish light discovery config.");
    }
}

/**
 * @brief 辅助函数：发送按钮配置
 */
static void send_button_config(PubSubClient& client, const DeviceInfo& dev, 
                               const char* object_id, const char* name, 
                               const char* icon, const char* entity_category,
                               const String& command_topic, const char* payload_press,
                               const String& avail_topic) {
    
    String topic = "homeassistant/button/" + dev.nodeId + "/" + object_id + "/config";
    
    String payload;
    payload.reserve(512);
    payload = "{";
    payload += "\"name\":\"" + String(name) + "\",";
    payload += "\"uniq_id\":\"" + dev.nodeId + "_" + object_id + "\",";
    payload += "\"cmd_t\":\"" + command_topic + "\",";
    payload += "\"pl_prs\":\"" + String(payload_press) + "\",";
    
    if (avail_topic.length() > 0) {
        payload += "\"avty_t\":\"" + avail_topic + "\",";
    }
    if (icon) { payload += "\"icon\":\"" + String(icon) + "\","; }
    if (entity_category) { payload += "\"ent_cat\":\"" + String(entity_category) + "\","; }
    
    // 设备信息
    payload += "\"dev\":{";
    payload += "\"ids\":[\"" + dev.nodeId + "\"],";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"mdl\":\"" + dev.model + "\",";
    payload += "\"mf\":\"" + dev.manufacturer + "\"";
    payload += "}}";
    
    client.publish(topic.c_str(), payload.c_str(), true);
}

/**
 * @brief 辅助函数：发送诊断传感器配置
 */
static void send_diagnostic_config(PubSubClient& client, const DeviceInfo& dev, 
                                   const char* object_id, const char* name, 
                                   const char* icon, const char* dev_class, const char* unit,
                                   const String& state_topic, const char* value_tpl,
                                   const String& avail_topic) {
    
    String topic = "homeassistant/sensor/" + dev.nodeId + "/" + object_id + "/config";
    
    String payload;
    payload.reserve(512);
    payload = "{";
    payload += "\"name\":\"" + String(name) + "\",";
    payload += "\"uniq_id\":\"" + dev.nodeId + "_" + object_id + "\",";
    payload += "\"stat_t\":\"" + state_topic + "\",";
    payload += "\"ent_cat\":\"diagnostic\",";
    
    if (avail_topic.length() > 0) {
        payload += "\"avty_t\":\"" + avail_topic + "\",";
    }
    if (icon) { payload += "\"icon\":\"" + String(icon) + "\","; }
    if (dev_class) { payload += "\"dev_cla\":\"" + String(dev_class) + "\","; }
    if (unit) { payload += "\"unit_of_meas\":\"" + String(unit) + "\","; }
    if (value_tpl) { payload += "\"val_tpl\":\"" + String(value_tpl) + "\","; }
    
    // 设备信息
    payload += "\"dev\":{";
    payload += "\"ids\":[\"" + dev.nodeId + "\"],";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"mdl\":\"" + dev.model + "\",";
    payload += "\"mf\":\"" + dev.manufacturer + "\"";
    payload += "}}";
    
    client.publish(topic.c_str(), payload.c_str(), true);
}

/**
 * @brief 辅助函数：发送 Select 实体配置
 */
static void send_select_config(PubSubClient& client, const DeviceInfo& dev, 
                               const char* object_id, const char* name, 
                               const char* icon, const char* entity_category,
                               const String& command_topic, const String& state_topic,
                               const char* value_tpl, const char* options_json,
                               const String& avail_topic) {
    
    String topic = "homeassistant/select/" + dev.nodeId + "/" + object_id + "/config";
    
    String payload;
    payload.reserve(1024);
    payload = "{";
    payload += "\"name\":\"" + String(name) + "\",";
    payload += "\"uniq_id\":\"" + dev.nodeId + "_" + object_id + "\",";
    payload += "\"cmd_t\":\"" + command_topic + "\",";
    payload += "\"stat_t\":\"" + state_topic + "\",";
    payload += "\"options\":" + String(options_json) + ",";
    
    if (avail_topic.length() > 0) {
        payload += "\"avty_t\":\"" + avail_topic + "\",";
    }
    if (icon) { payload += "\"icon\":\"" + String(icon) + "\","; }
    if (entity_category) { payload += "\"ent_cat\":\"" + String(entity_category) + "\","; }
    if (value_tpl) { payload += "\"val_tpl\":\"" + String(value_tpl) + "\","; }
    
    // 设备信息
    payload += "\"dev\":{";
    payload += "\"ids\":[\"" + dev.nodeId + "\"],";
    payload += "\"name\":\"" + dev.name + "\",";
    payload += "\"mdl\":\"" + dev.model + "\",";
    payload += "\"mf\":\"" + dev.manufacturer + "\"";
    payload += "}}";
    
    client.publish(topic.c_str(), payload.c_str(), true);
}

void ha_publish_system_discovery(PubSubClient& client, const DeviceInfo& dev, const MqttTopics& topics) {
    if (!client.connected()) return;

    // 1. 重启按钮
    send_button_config(client, dev, "restart", "Restart Device", "mdi:restart", "config", 
                       topics.system_set, "restart", topics.availability);

    // 2. 强制发现按钮
    send_button_config(client, dev, "discovery", "Force Discovery", "mdi:refresh", "diagnostic", 
                       topics.system_set, "discovery", topics.availability);

    // 3. IP 地址传感器
    send_diagnostic_config(client, dev, "ip", "IP Address", "mdi:ip-network", nullptr, nullptr,
                           topics.system_info, "{{ value_json.ip }}", topics.availability);

    // 4. RSSI 传感器
    send_diagnostic_config(client, dev, "rssi", "WiFi Signal", nullptr, "signal_strength", "dBm",
                           topics.system_info, "{{ value_json.rssi }}", topics.availability);

    // 5. 运行时间传感器
    send_diagnostic_config(client, dev, "uptime", "Uptime", nullptr, "duration", "s",
                           topics.system_info, "{{ value_json.uptime }}", topics.availability);

    // 6. 灯光效果选择器
    const char* effect_options = "[\"None\",\"Rainbow\",\"Breathing\",\"Police\",\"Spin\",\"Meteor\"]";
    send_select_config(client, dev, "effect", "Light Effect", "mdi:palette", "config",
                       topics.effect_set, topics.state, "{{ value_json.effect }}", effect_options, topics.availability);

    // 7. 场景模式选择器
    const char* scene_options = "[\"None\",\"Reading\",\"Night\",\"Cozy\",\"Bright\"]";
    // 注意：这里我们没有专门的 scene 状态字段，通常场景是触发式的。
    // 但为了让 Select 实体能显示当前状态，我们可以假定如果当前没有特效且符合某个场景的参数，就显示该场景。
    // 简化起见，我们暂时不回显场景状态，或者复用 effect 字段？不，effect 字段只显示特效。
    // 如果用户选择了场景，Select 会短暂显示该场景，然后可能变回 None (如果状态没更新)。
    // 为了更好的体验，我们可以在 state JSON 中增加 scene 字段，或者只是让它作为触发器。
    // 这里我们暂时让它作为触发器，状态回显可能不准确，除非我们在 lamp_core 中维护 currentScene 状态。
    // 鉴于 lamp_core 中 setScene 只是设置参数，没有维护状态，我们这里 state_topic 留空或者使用一个假的。
    // 但 HA Select 需要 state_topic 来显示当前值。
    // 让我们暂时使用 topics.state 和一个不存在的 json 路径，这样它可能始终显示 Unknown 或第一个值？
    // 或者更好的做法是：在 lamp_core 中增加 m_currentScene 变量。
    // 考虑到修改量，我们暂时让它共用 topics.state，但在 JSON 中我们需要增加 "scene" 字段。
    // 我会在 mqtt_task.cpp 的 publish_state 中增加 scene 字段。
    send_select_config(client, dev, "scene", "Light Scene", "mdi:home-lightbulb", "config",
                       topics.scene_set, topics.state, "{{ value_json.scene }}", scene_options, topics.availability);
}
