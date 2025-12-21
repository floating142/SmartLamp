/**
 * @file wifi_task.hpp
 * @brief WiFi 任务接口声明
 */

#pragma once

/**
 * @brief 初始化并启动 WiFi 任务
 * 
 * 负责连接 WiFi、维护连接状态以及同步 NTP 时间。
 */
void setup_wifi_task();

/**
 * @brief 停止 WiFi 任务
 * 
 * 停止任务并关闭 WiFi 硬件，通常用于释放资源给 BLE 配网。
 */
void stop_wifi_task();

/**
 * @brief 重新加载 WiFi 配置
 * 
 * 从 NVS 读取新配置并重新连接，无需重启设备。
 */
void wifi_reload_config();


