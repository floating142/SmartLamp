/**
 * @file screen_lamp.hpp
 * @brief 灯光控制屏幕 (Lamp Control Screen)
 * 
 * 提供亮度与色温的调节界面。
 */

#pragma once
#include <lvgl.h>

/**
 * @brief 创建灯光控制屏幕
 * @param parent 父对象
 */
lv_obj_t* ui_create_lamp_screen(lv_obj_t *parent);

/**
 * @brief 设置灯光屏幕的可见性
 * @param visible true 显示, false 隐藏
 */
void ui_lamp_set_visible(bool visible);

/**
 * @brief 重置视图滚动位置
 */
void ui_lamp_reset_view();

// =================================================================================
// 更新函数 (Update Functions)
// =================================================================================

/**
 * @brief 更新亮度显示
 * @param val 亮度值 (0-100)
 */
void ui_lamp_update_brightness(uint8_t val);

/**
 * @brief 更新色温显示
 * @param val 色温值 (Kelvin)
 */
void ui_lamp_update_cct(uint16_t val);

/**
 * @brief 更新自动亮度开关状态
 * @param enabled 是否开启
 */
void ui_lamp_update_auto_brightness(bool enabled);

// =================================================================================
// 导航与焦点 (Navigation & Focus)
// =================================================================================

/**
 * @brief 应用焦点样式
 * @param editMode 是否处于编辑模式 (调整数值)
 * @param focusIndex 0=亮度, 1=色温, 2=自动亮度
 */
void ui_lamp_apply_focus(bool editMode, int focusIndex);

/**
 * @brief 清除所有焦点样式
 */
void ui_lamp_clear_focus();

/**
 * @brief 处理数值调整导航
 * @param dir 方向 (+1/-1)
 * @param editMode 是否处于编辑模式
 * @param focusIndex 当前焦点项
 */
void ui_lamp_handle_nav(int dir, bool editMode, int focusIndex);
