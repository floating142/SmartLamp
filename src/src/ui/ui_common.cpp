/**
 * @file ui_common.cpp
 * @brief 通用 UI 样式实现 (Common UI Styles Implementation)
 */

#include "ui_common.hpp"

lv_style_t style_label_focus;
lv_style_t style_edit;
lv_style_t style_btn_focus;

void ui_init_styles() {
    // 1. 标签焦点样式 (蓝色文本)
    lv_style_init(&style_label_focus);
    lv_style_set_text_color(&style_label_focus, lv_color_hex(0x3399FF)); 

    // 2. 编辑样式 (橙色边框/旋钮)
    lv_style_init(&style_edit);
    lv_style_set_border_color(&style_edit, lv_color_hex(0xFF9900)); 
    lv_style_set_border_width(&style_edit, 3);
    lv_style_set_radius(&style_edit, LV_RADIUS_CIRCLE); // 圆形 (用于旋钮)

    // 3. 按钮焦点样式 (橙色边框)
    lv_style_init(&style_btn_focus);
    lv_style_set_border_color(&style_btn_focus, lv_color_hex(0xFF9900)); 
    lv_style_set_border_width(&style_btn_focus, 3);
}

lv_obj_t* ui_create_list_container(lv_obj_t* parent) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 220, 190);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_style_pad_row(cont, 5, 0);
    return cont;
}

lv_obj_t* ui_create_basic_list_item(lv_obj_t* parent, const char* text, lv_obj_t** out_sw) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, lv_pct(100), 45);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    
    if (out_sw) {
        *out_sw = lv_switch_create(btn);
        lv_obj_align(*out_sw, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_clear_flag(*out_sw, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_t *arrow = lv_label_create(btn);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    return btn;
}

lv_obj_t* ui_create_info_item(lv_obj_t* parent, const char* icon, const char* title, lv_obj_t** out_value_label) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, lv_pct(100), 45);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    
    // Style defaults
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_color(btn, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    
    lv_obj_t *lbl_title = lv_label_create(btn);
    lv_label_set_text_fmt(lbl_title, "%s %s", icon, title);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 0, 0);

    if (out_value_label) {
        *out_value_label = lv_label_create(btn);
        lv_label_set_text(*out_value_label, "--");
        lv_obj_align(*out_value_label, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_text_color(*out_value_label, lv_palette_main(LV_PALETTE_GREY), 0);
    }
    return btn;
}

lv_obj_t* ui_create_slider_item(lv_obj_t* parent, const char* title, lv_obj_t** out_slider, lv_obj_t** out_label) {
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, lv_pct(100), 65); // Taller for slider
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, title);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    
    if (out_label) {
        *out_label = lv_label_create(btn);
        lv_label_set_text(*out_label, "");
        lv_obj_align(*out_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    }

    if (out_slider) {
        *out_slider = lv_slider_create(btn);
        lv_obj_set_width(*out_slider, lv_pct(100));
        lv_obj_align(*out_slider, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    return btn;
}

void ui_apply_style(lv_obj_t* obj, bool focused, bool editing, bool colorText) {
    if (!obj) return;

    // Common base
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);

    if (focused) {
        if (editing) {
            lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_ORANGE), 0);
            lv_obj_set_style_shadow_width(obj, 10, 0);
            lv_obj_set_style_shadow_color(obj, lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_set_style_shadow_width(obj, 10, 0);
            lv_obj_set_style_shadow_color(obj, lv_palette_main(LV_PALETTE_BLUE), 0);
        }
        if (colorText) {
            uint32_t cnt = lv_obj_get_child_cnt(obj);
            for (uint32_t i = 0; i < cnt; i++) {
                lv_obj_t* child = lv_obj_get_child(obj, i);
                if (lv_obj_check_type(child, &lv_label_class)) {
                    lv_obj_set_style_text_color(child, lv_color_black(), 0);
                }
            }
        }
    } else {
        lv_obj_set_style_border_color(obj, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        if (colorText) {
            uint32_t cnt = lv_obj_get_child_cnt(obj);
            for (uint32_t i = 0; i < cnt; i++) {
                lv_obj_t* child = lv_obj_get_child(obj, i);
                if (lv_obj_check_type(child, &lv_label_class)) {
                    lv_obj_set_style_text_color(child, lv_color_black(), 0);
                }
            }
        }
    }
}
