/**
 * @file screen_status.hpp
 * @brief 系统状态屏幕 (System Status Screen)
 * 
 * 显示详细的系统信息：RSSI, 堆内存, 运行时间, MAC, IP。
 */

#pragma once
#include <lvgl.h>

/**
 * @brief 创建状态屏幕
 * @param parent 父对象
 */
lv_obj_t* ui_create_status_screen(lv_obj_t *parent);

/**
 * @brief 设置状态屏幕的可见性
 * @param visible true 显示, false 隐藏
 */
void ui_status_set_visible(bool visible);

/**
 * @brief 刷新状态数据 (RSSI, Heap, Uptime 等)
 * 
 * 应在屏幕可见时定期调用。
 */
void ui_status_update();

/**
 * @brief 应用焦点样式
 * @param index 焦点索引, -1 表示清除焦点
 */
void ui_status_apply_focus(int index);

/**
 * @brief 退出焦点后重置视图（滚动到顶部）
 */
void ui_status_reset_view();
