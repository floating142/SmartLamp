/**
 * @file ble_task.hpp
 * @brief BLE 配网任务接口声明
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// 声明 BLE 事件队列
extern QueueHandle_t bleEventQueue;

/**
 * @brief 获取当前是否处于 BLE 配网模式
 */
bool is_ble_config_active();

/**
 * @brief 更新雷达能量值到 BLE 特征值
 * @param energy 32个距离门的能量值数组
 */
void ble_update_radar_energy(const uint32_t* energy);

/**
 * @brief 启动 BLE 服务 (常开)
 */
void setup_ble_service();

/**
 * @brief 发送 BLE 通知数据 (用于状态上报)
 * @param msg 要发送的字符串消息
 */
void ble_send_notify(const char* msg);


