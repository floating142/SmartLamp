/**
 * @file screen_lamp.cpp
 * @brief 灯光控制屏幕实现 (Lamp Control Screen Implementation)
 */

#include "screen_lamp.hpp"
#include "../ui_common.hpp"
#include "../../app/lamp.hpp"
#include "../ui_manager.hpp"

static lv_obj_t *win_lamp = nullptr;
static lv_obj_t *cont_lamp = nullptr;

// List items
static lv_obj_t *item_brightness = nullptr;
static lv_obj_t *slider_brightness = nullptr;
static lv_obj_t *label_brightness = nullptr;

static lv_obj_t *item_cct = nullptr;
static lv_obj_t *slider_cct = nullptr;
static lv_obj_t *label_cct = nullptr;

static lv_obj_t *item_auto_br = nullptr;
static lv_obj_t *sw_auto_br = nullptr;

// 使用公共样式函数 `ui_apply_style`

// 使用公共创建函数（滑块/开关项已迁移到 ui_common）

lv_obj_t* ui_create_lamp_screen(lv_obj_t *parent) {
    win_lamp = lv_obj_create(parent);
    lv_obj_set_size(win_lamp, SCREEN_W, SCREEN_H);
    lv_obj_align(win_lamp, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title = lv_label_create(win_lamp);
    lv_label_set_text(title, "Lamp Control");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    // Main Container (List Style)
    cont_lamp = ui_create_list_container(win_lamp);

    // 1. Brightness
    item_brightness = ui_create_slider_item(cont_lamp, "Brightness", &slider_brightness, &label_brightness);
    lv_slider_set_range(slider_brightness, 0, 100);
    uint8_t uiBr = lamp.getSavedBrightness();
    lv_slider_set_value(slider_brightness, uiBr, LV_ANIM_OFF);
    lv_label_set_text_fmt(label_brightness, "%d%%", uiBr);
    ui_apply_style(item_brightness, false, false); // Apply default style

    // 2. CCT
    item_cct = ui_create_slider_item(cont_lamp, "Color Temp", &slider_cct, &label_cct);
    lv_slider_set_range(slider_cct, LAMP_CCT_MIN, LAMP_CCT_MAX);
    lv_slider_set_value(slider_cct, lamp.getCCT(), LV_ANIM_OFF);
    lv_label_set_text_fmt(label_cct, "%dK", lamp.getCCT());
    ui_apply_style(item_cct, false, false); // Apply default style

    // 3. Auto Brightness
    item_auto_br = ui_create_basic_list_item(cont_lamp, "Auto Brightness", &sw_auto_br);
    if (lamp.isAutoBrightness()) lv_obj_add_state(sw_auto_br, LV_STATE_CHECKED);
    ui_apply_style(item_auto_br, false, false); // Apply default style

    // Events
    lv_obj_add_event_cb(slider_brightness, [](lv_event_t *e){
        lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
        int v = lv_slider_get_value(obj);
        lamp.cancelFade();
        lamp.setBrightness((uint8_t)v);
        if (v > 0) lamp.setSavedBrightness((uint8_t)v);
        if (label_brightness) lv_label_set_text_fmt(label_brightness, "%d%%", v);
        ui_set_light(lamp.isOn());
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_add_event_cb(slider_cct, [](lv_event_t *e){
        lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
        int v = lv_slider_get_value(obj);
        lamp.setCCT((uint16_t)v);
        if (label_cct) lv_label_set_text_fmt(label_cct, "%dK", v);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_add_event_cb(sw_auto_br, [](lv_event_t *e){
        lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
        bool checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
        lamp.setAutoBrightness(checked);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    return win_lamp;
}

void ui_lamp_set_visible(bool visible) {
    if (!win_lamp) return;
    if (visible) lv_obj_clear_flag(win_lamp, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(win_lamp, LV_OBJ_FLAG_HIDDEN);
}

void ui_lamp_reset_view() {
    if (item_brightness) {
        lv_obj_scroll_to_view(item_brightness, LV_ANIM_OFF);
    }
}

// ===== Navigation / Focus handlers (previously local functions) =====
void ui_lamp_apply_focus(bool editMode, int focusIndex) {
    ui_apply_style(item_brightness, focusIndex == 0, editMode && focusIndex == 0);
    ui_apply_style(item_cct, focusIndex == 1, editMode && focusIndex == 1);
    ui_apply_style(item_auto_br, focusIndex == 2, editMode && focusIndex == 2);

    // Auto-scroll to focused item
    if (focusIndex >= 0) {
        lv_obj_t* target = nullptr;
        if (focusIndex == 0) target = item_brightness;
        else if (focusIndex == 1) target = item_cct;
        else if (focusIndex == 2) target = item_auto_br;

        if (target) lv_obj_scroll_to_view(target, LV_ANIM_ON);
    }
}

void ui_lamp_clear_focus() {
    ui_apply_style(item_brightness, false);
    ui_apply_style(item_cct, false);
    ui_apply_style(item_auto_br, false);
}

void ui_lamp_handle_nav(int dir, bool editMode, int focusIndex) {
    if (focusIndex == 0 && slider_brightness) {
        int step = editMode ? 5 : 1;
        int v = lv_slider_get_value(slider_brightness);
        v += dir * step;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        lamp.cancelFade();
        lamp.setBrightness((uint8_t)v);
        if (v > 0) lamp.setSavedBrightness((uint8_t)v);
        lv_slider_set_value(slider_brightness, v, LV_ANIM_OFF);
        if (label_brightness) lv_label_set_text_fmt(label_brightness, "%d%%", v);
        ui_set_light(lamp.isOn());
    } else if (focusIndex == 1 && slider_cct) {
        int step = editMode ? 100 : 10;
        int v = lv_slider_get_value(slider_cct);
        v += dir * step;
        if (v < LAMP_CCT_MIN) v = LAMP_CCT_MIN;
        if (v > LAMP_CCT_MAX) v = LAMP_CCT_MAX;
        lamp.setCCT((uint16_t)v);
        lv_slider_set_value(slider_cct, v, LV_ANIM_OFF);
        if (label_cct) lv_label_set_text_fmt(label_cct, "%dK", v);
    } else if (focusIndex == 2 && sw_auto_br) {
        // Toggle auto brightness on any nav adjustment while editing
        bool cur = lv_obj_has_state(sw_auto_br, LV_STATE_CHECKED);
        bool next = !cur;
        if (next) lv_obj_add_state(sw_auto_br, LV_STATE_CHECKED);
        else lv_obj_clear_state(sw_auto_br, LV_STATE_CHECKED);
        lamp.setAutoBrightness(next);
    }
}


void ui_lamp_update_auto_brightness(bool enabled) {
    if (!sw_auto_br) return;
    if (enabled) lv_obj_add_state(sw_auto_br, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw_auto_br, LV_STATE_CHECKED);
}

void ui_lamp_update_brightness(uint8_t br) {
    if (slider_brightness) {
        lv_slider_set_value(slider_brightness, br, LV_ANIM_ON);
        if (label_brightness) lv_label_set_text_fmt(label_brightness, "%d%%", br);
    }
}

void ui_lamp_update_cct(uint16_t cct) {
    if (slider_cct) {
        lv_slider_set_value(slider_cct, cct, LV_ANIM_ON);
        if (label_cct) lv_label_set_text_fmt(label_cct, "%dK", cct);
    }
}
