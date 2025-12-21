/**
 * @file ui_common.hpp
 * @brief 通用 UI 定义与样式 (Common UI Definitions and Styles)
 * 
 * 包含跨多个屏幕使用的共享常量、样式定义和辅助函数。
 */

#pragma once
#include <lvgl.h>

// 屏幕尺寸
#define SCREEN_W 240
#define SCREEN_H 240

// =================================================================================
// 共享样式 (Shared Styles)
// =================================================================================

/** @brief 焦点文本标签样式 (例如: 蓝色文本) */
extern lv_style_t style_label_focus;

/** @brief 编辑模式样式 (例如: 橙色边框/旋钮) */
extern lv_style_t style_edit;

/** @brief 焦点按钮样式 (例如: 橙色边框) */
extern lv_style_t style_btn_focus;

/**
 * @brief 初始化通用 UI 样式
 * 
 * 必须在创建任何使用这些样式的 UI 对象之前调用。
 */
void ui_init_styles();

/**
 * @brief 创建标准列表容器 (宽度/高度/布局/间距已配置)
 * @param parent 父对象
 * @return 新创建的容器对象
 */
lv_obj_t* ui_create_list_container(lv_obj_t* parent);

/**
 * @brief 创建带可选开关的列表项
 * @param parent 父对象
 * @param text 文本标签
 * @param out_sw 如果不为 nullptr，会创建一个 switch 并将指针写入
 */
lv_obj_t* ui_create_basic_list_item(lv_obj_t* parent, const char* text, lv_obj_t** out_sw = nullptr);

/**
 * @brief 创建信息项（图标+标题 + 可选值标签）
 * @param parent 父对象
 * @param icon 如 LV_SYMBOL_WIFI
 * @param title 标题文本
 * @param out_value_label 如果不为 nullptr，会创建并返回值标签指针
 */
lv_obj_t* ui_create_info_item(lv_obj_t* parent, const char* icon, const char* title, lv_obj_t** out_value_label);

/**
 * @brief 创建带滑块的列表项
 * @param parent 父对象
 * @param title 标题文本
 * @param out_slider 返回创建的滑块对象指针
 * @param out_label 返回创建的右上角标签对象指针
 */
lv_obj_t* ui_create_slider_item(lv_obj_t* parent, const char* title, lv_obj_t** out_slider, lv_obj_t** out_label);

/**
 * @brief 应用统一的样式到列表项
 * @param obj 要设置样式的对象
 * @param focused 是否为焦点态
 * @param editing 编辑态（将使用橙色边框）
 * @param colorText 是否将子标签文字设为黑色（某些 info 项希望保留灰色值标签）
 */
void ui_apply_style(lv_obj_t* obj, bool focused, bool editing = false, bool colorText = true);
