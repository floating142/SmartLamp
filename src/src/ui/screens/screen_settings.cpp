/**
 * @file screen_settings.cpp
 * @brief 设置屏幕实现 (Settings Screen Implementation)
 */

#include "screen_settings.hpp"
#include "../ui_common.hpp"
#include "../gui_task.hpp"
#include "../../sensors/sensor_manager.hpp"
#include "../../system/storage.hpp"
#include <vector>
#include <string>

static lv_obj_t *win_settings = nullptr;
static lv_obj_t *cont_main = nullptr;
static lv_obj_t *cont_wifi = nullptr;

// Main items
static lv_obj_t *item_power_save = nullptr;
static lv_obj_t *sw_power_save = nullptr;
static lv_obj_t *item_radar = nullptr;
static lv_obj_t *sw_radar = nullptr;
static lv_obj_t *item_debug = nullptr;
static lv_obj_t *sw_debug = nullptr;
static lv_obj_t *item_wifi = nullptr;
static lv_obj_t *item_restart = nullptr;

// WiFi list state
static int s_wifiFocusIndex = 0;
static std::vector<lv_obj_t*> s_wifiItems;

// 使用公共样式函数 `ui_apply_style`

// 使用公共创建函数

lv_obj_t* ui_create_settings_screen(lv_obj_t *parent) {
    win_settings = lv_obj_create(parent);
    lv_obj_set_size(win_settings, SCREEN_W, SCREEN_H);
    lv_obj_align(win_settings, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title = lv_label_create(win_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

    // Main Container
    cont_main = ui_create_list_container(win_settings);

    // 1. Power Save
    item_power_save = ui_create_basic_list_item(cont_main, "Power Save", &sw_power_save);
    if (gui_is_power_save_mode()) lv_obj_add_state(sw_power_save, LV_STATE_CHECKED);
    ui_apply_style(item_power_save, false);
    
    // 2. Radar
    item_radar = ui_create_basic_list_item(cont_main, "Radar", &sw_radar);
    bool radarEnabled = true;
    AppConfig::instance().loadRadarEnable(radarEnabled);
    if (radarEnabled) lv_obj_add_state(sw_radar, LV_STATE_CHECKED);
    ui_apply_style(item_radar, false);

    // 3. Debug
    item_debug = ui_create_basic_list_item(cont_main, "Debug Mode", &sw_debug);
    bool debugEnabled = false;
    AppConfig::instance().loadDebugMode(debugEnabled);
    if (debugEnabled) lv_obj_add_state(sw_debug, LV_STATE_CHECKED);
    ui_apply_style(item_debug, false);

    // 4. WiFi List
    item_wifi = ui_create_basic_list_item(cont_main, "Saved Networks", nullptr);
    ui_apply_style(item_wifi, false);

    // 5. Restart
    item_restart = ui_create_basic_list_item(cont_main, "Restart System", nullptr);
    ui_apply_style(item_restart, false);

    return win_settings;
}

void ui_settings_set_visible(bool visible) {
    if (!win_settings) return;
    if (visible) lv_obj_clear_flag(win_settings, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(win_settings, LV_OBJ_FLAG_HIDDEN);
}

void ui_settings_reset_view() {
    if (item_power_save) {
        lv_obj_scroll_to_view(item_power_save, LV_ANIM_OFF);
    }
}

void ui_settings_apply_focus(int focusIndex) {
    ui_apply_style(item_power_save, focusIndex == 0);
    ui_apply_style(item_radar, focusIndex == 1);
    ui_apply_style(item_debug, focusIndex == 2);
    ui_apply_style(item_wifi, focusIndex == 3);
    ui_apply_style(item_restart, focusIndex == 4);
    
    // Auto scroll
    if (focusIndex >= 0) {
        lv_obj_t* target = nullptr;
        if (focusIndex == 0) target = item_power_save;
        else if (focusIndex == 1) target = item_radar;
        else if (focusIndex == 2) target = item_debug;
        else if (focusIndex == 3) target = item_wifi;
        else if (focusIndex == 4) target = item_restart;
        
        if (target) lv_obj_scroll_to_view(target, LV_ANIM_ON);
    }
}

void ui_settings_toggle_item(int focusIndex) {
    if (focusIndex == 0 && sw_power_save) {
        bool state = !lv_obj_has_state(sw_power_save, LV_STATE_CHECKED);
        if (state) lv_obj_add_state(sw_power_save, LV_STATE_CHECKED);
        else lv_obj_clear_state(sw_power_save, LV_STATE_CHECKED);
        gui_set_power_save_mode(state);
    } else if (focusIndex == 1 && sw_radar) {
        bool state = !lv_obj_has_state(sw_radar, LV_STATE_CHECKED);
        if (state) lv_obj_add_state(sw_radar, LV_STATE_CHECKED);
        else lv_obj_clear_state(sw_radar, LV_STATE_CHECKED);
        sensor_set_radar_enable(state);
    } else if (focusIndex == 2 && sw_debug) {
        bool state = !lv_obj_has_state(sw_debug, LV_STATE_CHECKED);
        if (state) lv_obj_add_state(sw_debug, LV_STATE_CHECKED);
        else lv_obj_clear_state(sw_debug, LV_STATE_CHECKED);
        AppConfig::instance().saveDebugMode(state);
    } else if (focusIndex == 4) {
        // Restart
        ESP.restart();
    }
}

// ---- WiFi Sub-menu ----

void ui_settings_show_wifi_list(bool show) {
    if (show) {
        lv_obj_add_flag(cont_main, LV_OBJ_FLAG_HIDDEN);
        
        if (cont_wifi) lv_obj_del(cont_wifi);
        cont_wifi = lv_obj_create(win_settings);
        lv_obj_set_size(cont_wifi, 220, 190);
        lv_obj_align(cont_wifi, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_obj_set_flex_flow(cont_wifi, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(cont_wifi, 5, 0);
        lv_obj_set_style_pad_row(cont_wifi, 5, 0);
        
        // Title for sub-menu
        lv_obj_t *lbl = lv_label_create(cont_wifi);
        lv_label_set_text(lbl, "Saved Networks:");
        
        s_wifiItems.clear();
        std::vector<AppConfig::WifiCred> list;
        AppConfig::instance().loadWifiList(list);
        
        if (list.empty()) {
            lv_obj_t *empty = lv_label_create(cont_wifi);
            lv_label_set_text(empty, "No networks saved");
        } else {
            for (const auto& cred : list) {
                lv_obj_t *btn = lv_obj_create(cont_wifi);
                lv_obj_set_size(btn, lv_pct(100), 40);
                lv_obj_t *txt = lv_label_create(btn);
                lv_label_set_text(txt, cred.ssid.c_str());
                lv_obj_align(txt, LV_ALIGN_LEFT_MID, 0, 0);
                s_wifiItems.push_back(btn);
            }
        }
        
        s_wifiFocusIndex = 0;
        ui_settings_wifi_nav(0); // Apply initial focus
        
    } else {
        if (cont_wifi) {
            lv_obj_del(cont_wifi);
            cont_wifi = nullptr;
        }
        lv_obj_clear_flag(cont_main, LV_OBJ_FLAG_HIDDEN);
        s_wifiItems.clear();
    }
}

void ui_settings_wifi_nav(int dir) {
    if (s_wifiItems.empty()) return;
    
    // Clear old focus
        if (s_wifiFocusIndex >= 0 && s_wifiFocusIndex < s_wifiItems.size()) {
        ui_apply_style(s_wifiItems[s_wifiFocusIndex], false);
    }
    
    s_wifiFocusIndex += dir;
    if (s_wifiFocusIndex < 0) s_wifiFocusIndex = 0;
    if (s_wifiFocusIndex >= s_wifiItems.size()) s_wifiFocusIndex = s_wifiItems.size() - 1;
    
    // Apply new focus
        if (s_wifiFocusIndex >= 0 && s_wifiFocusIndex < s_wifiItems.size()) {
        ui_apply_style(s_wifiItems[s_wifiFocusIndex], true);
        lv_obj_scroll_to_view(s_wifiItems[s_wifiFocusIndex], LV_ANIM_ON);
    }
}

void ui_settings_wifi_select() {
    // Placeholder: Could show details or delete confirmation
    // For now, just blink or do nothing
}

