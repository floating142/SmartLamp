/**
 * @file mqtt_task.cpp
 * @brief MQTT 客户端任务实现
 * 
 * 负责连接 MQTT Broker，订阅控制主题，并定期上报设备状态。
 * 支持开关、亮度、色温、RGB 控制及系统指令。
 */

#include "mqtt_task.hpp"
#include "mqtt_ha.hpp"

// Project Headers
#include "../system/storage.hpp"
#include "../app/lamp.hpp"
#include "../ui/gui_task.hpp"
#include "../sensors/bh1750.hpp"
#include "../sensors/sht4x.hpp"
#include "../sensors/cw2015.hpp"

// Arduino & System Headers
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// =================================================================================
// 全局变量 (Global Variables)
// =================================================================================

static WiFiClient espClient;
static PubSubClient client(espClient);
static TaskHandle_t s_mqttTaskHandle = nullptr;
QueueHandle_t mqttEventQueue = nullptr;

// MQTT 配置信息
static String mqtt_host;
static int mqtt_port;
static String mqtt_user;
static String mqtt_pass;

// 结构化主题与设备信息
static MqttTopics g_topics;
static DeviceInfo g_deviceInfo;

// 状态变更标志
static volatile bool g_state_changed = false;

// =================================================================================
// 内部辅助函数声明
// =================================================================================

static void publish_state();
static void publish_sensors();
static void publish_system_info(bool retain);
static void handle_switch(char* msg);
static void handle_brightness(char* msg);
static void handle_cct(char* msg);
static void handle_rgb(char* msg);
static void handle_effect(char* msg);
static void handle_scene(char* msg);
static void handle_system(char* msg);

// =================================================================================
// 消息处理逻辑
// =================================================================================

/**
 * @brief MQTT 消息回调函数
 */
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    if (length >= 256) {
        Serial.println("[MQTT] Payload too long, ignored");
        return;
    }
    
    char msgBuf[256];
    memcpy(msgBuf, payload, length);
    msgBuf[length] = '\0';
    
    // Trim
    char* msgPtr = msgBuf;
    while(isspace((unsigned char)*msgPtr)) msgPtr++;
    char* end = msgPtr + strlen(msgPtr) - 1;
    while(end > msgPtr && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    // 路由分发
    if (strcmp(topic, g_topics.switch_set.c_str()) == 0) {
        handle_switch(msgPtr);
    } 
    else if (strcmp(topic, g_topics.brightness_set.c_str()) == 0) {
        handle_brightness(msgPtr);
    }
    else if (strcmp(topic, g_topics.cct_set.c_str()) == 0) {
        handle_cct(msgPtr);
    }
    else if (strcmp(topic, g_topics.rgb_set.c_str()) == 0) {
        handle_rgb(msgPtr);
    }
    else if (strcmp(topic, g_topics.effect_set.c_str()) == 0) {
        handle_effect(msgPtr);
    }
    else if (strcmp(topic, g_topics.scene_set.c_str()) == 0) {
        handle_scene(msgPtr);
    }
    else if (strcmp(topic, g_topics.system_set.c_str()) == 0) {
        handle_system(msgPtr);
    }
}

static void handle_switch(char* msg) {
    String s = String(msg);
    bool on = false;
    bool valid = false;
    
    if (s.equalsIgnoreCase("ON") || s == "1" || s.equalsIgnoreCase("true")) {
        lamp.setPower(true, 500);
        on = true;
        valid = true;
    } else if (s.equalsIgnoreCase("OFF") || s == "0" || s.equalsIgnoreCase("false")) {
        lamp.setPower(false, 500);
        on = false;
        valid = true;
    }
    
    if (valid) {
        UIEvent evt = {UI_EVENT_LIGHT, on ? 1 : 0};
        send_ui_event(evt, DEST_MQTT);
        g_state_changed = true;
    }
}

static void handle_brightness(char* msg) {
    int val = atoi(msg);
    if (val >= 0 && val <= 100) {
        lamp.setBrightness(val, 500, DEST_MQTT); 
        UIEvent evt = {UI_EVENT_BRIGHTNESS, val};
        send_ui_event(evt, DEST_MQTT);
        g_state_changed = true;
    }
}

static void handle_cct(char* msg) {
    int val = atoi(msg);
    // Mireds 转换 (HA 通常发送 Mireds)
    if (val > 0 && val < 1000) {
        val = 1000000 / val;
    }

    if (val >= 2700 && val <= 6500) {
        lamp.setCCT(val, 500, DEST_MQTT);
        UIEvent evt = {UI_EVENT_CCT, val};
        send_ui_event(evt, DEST_MQTT);
        g_state_changed = true;
    }
}

static void handle_rgb(char* msg) {
    int r, g, b;
    if (sscanf(msg, "%d,%d,%d", &r, &g, &b) == 3) {
        lamp.setColor(r, g, b, 500, DEST_MQTT);
        g_state_changed = true;
    }
}

static void handle_effect(char* msg) {
    lamp.setEffect(msg);
    g_state_changed = true;
}

static void handle_scene(char* msg) {
    lamp.setScene(msg, DEST_MQTT);
    g_state_changed = true;
}

static void handle_system(char* msg) {
    String cmd = String(msg);
    cmd.toLowerCase();
    
    if (cmd == "restart" || cmd == "reboot") {
        Serial.println("[MQTT] System restart command received.");
        ESP.restart();
    }
    else if (cmd == "info") {
        publish_system_info(true);
    }
    else if (cmd == "discovery") {
        // 强制重发 HA 发现配置
        ha_publish_sensor_discovery(client, g_deviceInfo, g_topics);
        ha_publish_light_discovery(client, g_deviceInfo, g_topics);
        ha_publish_system_discovery(client, g_deviceInfo, g_topics);
    }
}

// =================================================================================
// 状态上报
// =================================================================================

static void publish_system_info(bool retain) {
    if (!client.connected()) return;

    String info = "{";
    info += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    info += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    info += "\"uptime\":" + String(millis() / 1000);
    info += "}";
    client.publish(g_topics.system_info.c_str(), info.c_str(), retain);
}

static void publish_sensors() {
    if (!client.connected()) return;

    if (bh1750_has_reading()) {
        String val = String(bh1750_get_lux(), 1);
        client.publish(g_topics.sensor_lux.c_str(), val.c_str());
    }

    if (sht4x_has_reading()) {
        String t = String(sht4x_get_temperature(), 1);
        String h = String(sht4x_get_humidity(), 1);
        client.publish(g_topics.sensor_temp.c_str(), t.c_str());
        client.publish(g_topics.sensor_humi.c_str(), h.c_str());
    }

    // 发布系统信息 (IP, RSSI, Uptime)
    publish_system_info(true);
}

static void publish_state() {
    if (!client.connected()) return;
    
    char jsonBuf[384]; 
    int displayBri = lamp.getSavedBrightness();
    
    const char* effectStr = "None";
    switch(lamp.getEffect()) {
        case EffectMode::Rainbow:  effectStr = "Rainbow"; break;
        case EffectMode::Breathing:effectStr = "Breathing"; break;
        case EffectMode::Police:     effectStr = "Police"; break;
        case EffectMode::Spin:     effectStr = "Spin"; break;
        case EffectMode::Meteor:   effectStr = "Meteor"; break;
        default: effectStr = "None"; break;
    }
    
    snprintf(jsonBuf, sizeof(jsonBuf), 
        "{\"state\":\"%s\",\"brightness\":%d,\"color_mode\":\"%s\",\"cct\":%d,\"rgb\":{\"r\":%d,\"g\":%d,\"b\":%d},\"effect\":\"%s\",\"scene\":\"%s\"}",
        lamp.isOn() ? "ON" : "OFF",
        displayBri,
        lamp.isCCTMode() ? "color_temp" : "rgb",
        lamp.getCCT(),
        lamp.getRGB().r,
        lamp.getRGB().g,
        lamp.getRGB().b,
        effectStr,
        lamp.getScene().c_str()
    );
    
    client.publish(g_topics.state.c_str(), jsonBuf);
    client.publish(g_topics.switch_state.c_str(), lamp.isOn() ? "ON" : "OFF");
    
    if (g_topics.availability.length() > 0) {
        client.publish(g_topics.availability.c_str(), "online", true);
    }
    
    publish_sensors();
}

// =================================================================================
// 连接与初始化
// =================================================================================

void mqtt_reconnect() {
    while (!client.connected()) {
        if (WiFi.status() != WL_CONNECTED) {
            UIEvent evt = {UI_EVENT_MQTT_STATE, 0};
            send_ui_event(evt);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        Serial.print("[MQTT] Attempting connection...");
        
        String clientId = "ESP32Lamp-" + String(random(0xffff), HEX);
        const char* userPtr = mqtt_user.length() ? mqtt_user.c_str() : nullptr;
        const char* passPtr = mqtt_pass.length() ? mqtt_pass.c_str() : nullptr;
        const char* willTopic = g_topics.availability.length() ? g_topics.availability.c_str() : nullptr;
        
        if (client.connect(clientId.c_str(), userPtr, passPtr, willTopic, 1, true, "offline")) {
            Serial.println("connected");
            
            UIEvent evt = {UI_EVENT_MQTT_STATE, 1};
            send_ui_event(evt);

            if (willTopic) client.publish(willTopic, "online", true);
            
            publish_state();
            
            // 订阅
            client.subscribe(g_topics.switch_set.c_str());
            client.subscribe(g_topics.brightness_set.c_str());
            client.subscribe(g_topics.cct_set.c_str());
            client.subscribe(g_topics.rgb_set.c_str());
            client.subscribe(g_topics.effect_set.c_str());
            client.subscribe(g_topics.scene_set.c_str());
            client.subscribe(g_topics.system_set.c_str());
            
            Serial.println("[MQTT] Subscribed to topics");

            // 发布 HA 发现配置
            ha_publish_sensor_discovery(client, g_deviceInfo, g_topics);
            ha_publish_light_discovery(client, g_deviceInfo, g_topics);
            ha_publish_system_discovery(client, g_deviceInfo, g_topics);
            
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5s");
            
            UIEvent evt = {UI_EVENT_MQTT_STATE, 0};
            send_ui_event(evt);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

static void init_topics() {
    uint64_t chipid = ESP.getEfuseMac();
    String chipIdHex = String((uint32_t)chipid, HEX);
    
    // 初始化设备信息
    g_deviceInfo.chipId = chipIdHex;
    g_deviceInfo.nodeId = "deng_" + chipIdHex;
    g_deviceInfo.name = "Deng_" + chipIdHex;
    g_deviceInfo.model = "ESP32 Smart Lamp";
    g_deviceInfo.manufacturer = "Float";

    // 初始化主题
    g_topics.prefix = "deng/" + chipIdHex;
    g_topics.availability = g_topics.prefix + "/availability";
    g_topics.state = g_topics.prefix + "/state";
    g_topics.switch_set = g_topics.prefix + "/switch/set";
    g_topics.switch_state = g_topics.prefix + "/switch/state";
    g_topics.brightness_set = g_topics.prefix + "/brightness/set";
    g_topics.cct_set = g_topics.prefix + "/cct/set";
    g_topics.rgb_set = g_topics.prefix + "/rgb/set";
    g_topics.effect_set = g_topics.prefix + "/effect/set";
    g_topics.scene_set = g_topics.prefix + "/scene/set";
    
    g_topics.sensor_lux = g_topics.prefix + "/sensor/lux";
    g_topics.sensor_temp = g_topics.prefix + "/sensor/temp";
    g_topics.sensor_humi = g_topics.prefix + "/sensor/humi";
    
    g_topics.system_set = g_topics.prefix + "/system/set";
    g_topics.system_info = g_topics.prefix + "/system/info";
}

static void task_mqtt(void *pvParameters) {
    Serial.println("[MQTT] Task started, waiting for WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    Serial.println("[MQTT] WiFi connected! Initializing MQTT...");

    while (!AppConfig::instance().loadMQTT(mqtt_host, mqtt_port, mqtt_user, mqtt_pass)) {
        Serial.println("[MQTT] No config found. Waiting...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    init_topics();

    client.setServer(mqtt_host.c_str(), mqtt_port);
    client.setCallback(mqtt_callback);
    client.setBufferSize(2048);

    static uint32_t lastPub = 0;
    static uint32_t lastResponsePub = 0;

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!client.connected()) {
                mqtt_reconnect();
            }
            client.loop();

            uint32_t now = millis();

            // 心跳包 (5s)
            if (now - lastPub > 5000) {
                lastPub = now;
                publish_state();
            }
            
            // 状态变更立即上报 (限流 200ms)
            if (g_state_changed && (now - lastResponsePub > 200)) {
                g_state_changed = false;
                lastResponsePub = now;
                publish_state();
            }

            // 处理事件队列
            if (mqttEventQueue) {
                UIEvent evt;
                while (xQueueReceive(mqttEventQueue, &evt, 0) == pdTRUE) {
                    switch (evt.type) {
                        case UI_EVENT_LUX:
                            client.publish(g_topics.sensor_lux.c_str(), String(evt.fvalue, 1).c_str());
                            break;
                        case UI_EVENT_TEMPERATURE:
                            client.publish(g_topics.sensor_temp.c_str(), String(evt.fvalue, 1).c_str());
                            break;
                        case UI_EVENT_HUMIDITY:
                            client.publish(g_topics.sensor_humi.c_str(), String(evt.fvalue, 1).c_str());
                            break;
                        case UI_EVENT_LIGHT:
                        case UI_EVENT_BRIGHTNESS:
                        case UI_EVENT_CCT:
                        case UI_EVENT_RGB:
                            publish_state();
                            break;
                        default: break;
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void setup_mqtt_task() {
    if (!mqttEventQueue) {
        mqttEventQueue = xQueueCreate(8, sizeof(UIEvent));
    }
    xTaskCreate(task_mqtt, "MQTT Task", 4096, NULL, 1, &s_mqttTaskHandle);
}

void stop_mqtt_task() {
    if (s_mqttTaskHandle != nullptr) {
        vTaskDelete(s_mqttTaskHandle);
        s_mqttTaskHandle = nullptr;
        Serial.println("[MQTT] Task stopped.");
    }
}

void mqtt_report_state() {
    g_state_changed = true;
}

void mqtt_get_config(String& host, int& port) {
    host = mqtt_host;
    port = mqtt_port;
}
