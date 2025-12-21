/**
 * @file mqtt_task.hpp
 * @brief MQTT 任务接口声明
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// 声明 MQTT 事件队列
extern QueueHandle_t mqttEventQueue;

/**
 * @brief 初始化并启动 MQTT 任务
 * 
 * 负责连接 MQTT Broker，订阅控制主题，并定期上报设备状态。
 */
void setup_mqtt_task();

/**
 * @brief 停止 MQTT 任务
 */
void stop_mqtt_task();

/**
 * @brief 触发 MQTT 状态上报
 * 
 * 当设备状态在本地发生改变（如物理开关、定时器）时调用此函数，
 * 以便及时通知 MQTT 服务器。
 */
void mqtt_report_state();

/**
 * @brief 获取 MQTT 配置信息
 * @param host 输出主机地址
 * @param port 输出端口号
 */
void mqtt_get_config(String& host, int& port);

