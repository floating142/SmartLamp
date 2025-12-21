/**
 * @file screen_settings.hpp
 * @brief 设置屏幕 (Settings Screen)
 * 
 * 允许配置系统参数，如省电模式和雷达使能。
 */

#pragma once
#include <lvgl.h>

/**
 * @brief 创建设置屏幕
 * @param parent 父对象
 */
lv_obj_t* ui_create_settings_screen(lv_obj_t *parent);

/**
 * @brief 设置设置屏幕的可见性
 * @param visible true 显示, false 隐藏
 */
void ui_settings_set_visible(bool visible);

/**
 * @brief 重置视图滚动位置
 */
void ui_settings_reset_view();

// =================================================================================
// 导航与焦点 (Navigation & Focus)
// =================================================================================

/**
 * @brief 应用焦点样式到选中的设置项
 * @param focusIndex 项目索引 (0=省电模式, 1=雷达, -1=无)
 */
void ui_settings_apply_focus(int focusIndex);

/**
 * @brief 切换当前焦点项的状态
 * @param focusIndex 项目索引
 */
void ui_settings_toggle_item(int focusIndex);

// ---- WiFi 子菜单支持 ----
void ui_settings_show_wifi_list(bool show);
void ui_settings_wifi_nav(int dir);
void ui_settings_wifi_select();
