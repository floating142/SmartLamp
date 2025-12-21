/**
 * @file ui_manager.hpp
 * @brief UI 管理器 (UI Manager)
 * 
 * 负责屏幕初始化、导航路由和事件分发。
 * 充当后端 (gui_task) 和各个独立屏幕之间的中介。
 */

#pragma once
#include <lvgl.h>
#include <stdint.h>

/**
 * @brief 初始化 UI 系统
 * 
 * 创建所有屏幕并设置初始可见屏幕。
 * 应在 LVGL 初始化后调用。
 */
void ui_init();

/**
 * @brief 进入深度睡眠准备 (不显示 Screensaver)
 * 
 * 仅重置状态并切换回主屏幕，不进入 Screensaver 模式。
 * 配合背光关闭使用。
 */
void ui_enter_sleep();

/**
 * @brief 进入息屏显示模式 (Screensaver)
 */
void ui_enter_screensaver();

/**
 * @brief 退出息屏显示模式
 * @param animate 是否使用过渡动画 (默认 true)
 */
void ui_exit_screensaver(bool animate = true);

/**
 * @brief 检查是否处于息屏显示模式
 */
bool ui_is_screensaver();

// =================================================================================
// 导航与输入 (Navigation & Input)
// =================================================================================

/**
 * @brief 处理导航输入 (编码器/按钮)
 * 
 * @param dir 方向: -1 (左/下/减), +1 (右/上/加)
 */
void ui_nav(int dir);

/**
 * @brief 进入当前菜单或激活选中项
 * 
 * 用于 "确定" 或 "点击" 事件。
 */
void ui_enter_menu();

/**
 * @brief 退出当前菜单或返回上一级
 * 
 * 用于 "返回" 或 "取消" 事件。
 */
void ui_exit_menu();

// =================================================================================
// 数据更新 (由 gui_task 调用)
// =================================================================================

/**
 * @brief 更新系统时间显示
 * @param hour 小时 (0-23)
 * @param minute 分钟 (0-59)
 * @param second 秒 (0-59)
 */
void ui_update_time(int hour, int minute, int second);

/**
 * @brief 更新日期显示 (用于息屏界面)
 * @param dateStr 日期字符串
 */
void ui_update_date(const char* dateStr);

/**
 * @brief 更新系统状态码显示
 * @param state 状态码
 */
void ui_update_state(int state);

/**
 * @brief 一次性更新所有传感器数据
 * @param temp 温度 (摄氏度)
 * @param humi 湿度 (%)
 * @param lux 光照强度 (Lux)
 * @param radar_dist 雷达距离 (cm)
 */
void ui_update_sensor_data(float temp, float humi, float lux, int radar_dist);

/** @brief 更新温度显示 */
void ui_update_temperature(float temp);

/** @brief 更新湿度显示 */
void ui_update_humidity(float humi);

/** @brief 更新光照显示 */
void ui_update_lux(float lux);

/** @brief 更新雷达距离显示 */
void ui_update_radar_dist(int dist);

/** @brief 更新雷达状态 (运动/静止) */
void ui_update_radar_state(int state);

/** @brief 刷新状态页面 (RSSI, Heap 等) */
void ui_update_status_page();

/** @brief 更新 IP 地址显示 */
void ui_update_ip(const char* ip);

/** @brief 更新 WiFi 连接状态 */
void ui_update_wifi_state(bool connected, int rssi);

/** @brief 更新 BLE 连接状态 */
void ui_update_ble_state(bool connected);

/** @brief 更新 MQTT 连接状态 */
void ui_update_mqtt_status(bool connected);

/** @brief 更新电池电量 */
void ui_update_battery(int level);

/** @brief 更新灯光开关状态 */
void ui_update_light_state(bool on);

/** @brief 更新亮度滑块/数值 */
void ui_update_brightness(uint8_t val);

/** @brief 更新色温滑块/数值 */
void ui_update_cct(uint16_t val);

// =================================================================================
// 辅助函数 (Helpers)
// =================================================================================

/**
 * @brief 设置灯光状态的辅助函数 (ui_update_light_state 的别名)
 * @param on true=开, false=关
 */
void ui_set_light(bool on);
