/**
 * @file ui_manager.cpp
 * @brief UI 管理器实现 (UI Manager Implementation)
 */

#include "ui_manager.hpp"
#include "ui_common.hpp"
#include "screens/screen_main.hpp"
#include "screens/screen_lamp.hpp"
#include "screens/screen_settings.hpp"
#include "screens/screen_status.hpp"
#include <Arduino.h>
#include <freertos/task.h>

// ---- 状态变量 (State Variables) ----
static int s_currentWindow = 0; // 0=主页, 1=灯光, 2=设置, 3=状态
static bool s_inMenu = false;   // true 表示进入了子菜单 (焦点模式)

// 屏幕对象指针
static lv_obj_t *scr_boot = nullptr;
static lv_obj_t *scr_screensaver = nullptr;
static lv_obj_t *scr_main = nullptr;
static lv_obj_t *scr_lamp = nullptr;
static lv_obj_t *scr_settings = nullptr;
static lv_obj_t *scr_status = nullptr;

// 息屏显示组件
static lv_obj_t *label_saver_time = nullptr;
static lv_obj_t *label_saver_date = nullptr;
static bool s_inScreensaver = false;

// 灯光屏幕状态
static bool s_lampEditMode = false;       // true 表示正在编辑滑块数值
static int s_lampFocusIndex = 0;          // 0=亮度, 1=色温, 2=自动亮度

// 设置屏幕状态
static int s_settingsFocusIndex = 0;      // 0=省电模式, 1=雷达开关, 2=Debug模式, 3=WiFi列表
static int s_settingsSubMenu = 0;         // 0=None, 1=WiFi List

// 状态屏幕状态
static int s_statusFocusIndex = 0;

// ---- 内部函数声明 ----
static lv_obj_t* ui_create_boot_screen(lv_obj_t *parent);
static lv_obj_t* ui_create_screensaver_screen(lv_obj_t *parent);
static void boot_anim_cb(lv_timer_t * timer);

void ui_init() {
    ui_init_styles();
    
    // 创建独立屏幕 (Parent = NULL)
    scr_boot = ui_create_boot_screen(NULL);
    scr_screensaver = ui_create_screensaver_screen(NULL);
    scr_main = ui_create_main_screen(NULL);
    scr_lamp = ui_create_lamp_screen(NULL);
    scr_settings = ui_create_settings_screen(NULL);
    scr_status = ui_create_status_screen(NULL);

    // 初始加载启动屏幕
    lv_scr_load(scr_boot);
    
    // 创建一个一次性定时器，作为保底，5秒后强制切换到主屏幕 (防止 main 初始化卡死)
    lv_timer_t * t = lv_timer_create(boot_anim_cb, 5000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

void ui_boot_complete() {
    // 如果当前还在启动屏幕，则切换到主屏幕
    if (lv_scr_act() == scr_boot) {
        boot_anim_cb(NULL);
    }
}

static void boot_anim_cb(lv_timer_t * timer) {
    // 切换到主屏幕
    if (lv_scr_act() != scr_main) {
        lv_scr_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, true); 
        ui_main_set_visible(true);
    }
}

// =================================================================================
// 息屏显示 (Screensaver)
// =================================================================================

void ui_enter_sleep() {
    // 强制退出菜单/焦点模式
    if (s_inMenu) {
        s_inMenu = false;
        s_lampEditMode = false;
        s_lampFocusIndex = 0;
        s_settingsFocusIndex = 0;
        s_settingsSubMenu = 0;

        if (s_currentWindow == 1) {
            ui_lamp_clear_focus();
            ui_lamp_reset_view();
        } else if (s_currentWindow == 2) {
            ui_settings_show_wifi_list(false);
            ui_settings_apply_focus(-1);
            ui_settings_reset_view();
        } else if (s_currentWindow == 3) {
            ui_status_apply_focus(-1);
            ui_status_reset_view();
        }
    }
    
    // 切换回主屏幕，准备唤醒
    s_currentWindow = 0;
    lv_scr_load(scr_main);
}

void ui_enter_screensaver() {
    if (s_inScreensaver) return;
    
    // 强制退出菜单/焦点模式，确保唤醒时处于顶层导航
    if (s_inMenu) {
        s_inMenu = false;
        
        // Reset all navigation states
        s_lampEditMode = false;
        s_lampFocusIndex = 0;
        s_settingsFocusIndex = 0;
        s_settingsSubMenu = 0;

        // Clear visual focus
        if (s_currentWindow == 1) {
            ui_lamp_clear_focus();
            ui_lamp_reset_view();
        } else if (s_currentWindow == 2) {
            ui_settings_show_wifi_list(false);
            ui_settings_apply_focus(-1);
            ui_settings_reset_view();
        } else if (s_currentWindow == 3) {
            ui_status_apply_focus(-1);
            ui_status_reset_view();
        }
    }
    
    s_inScreensaver = true;
    
    // 确保 screensaver 屏幕已创建
    if (!scr_screensaver) scr_screensaver = ui_create_screensaver_screen(NULL);
    
    // 切换到息屏显示
    lv_scr_load_anim(scr_screensaver, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void ui_exit_screensaver(bool animate) {
    if (!s_inScreensaver) return;
    s_inScreensaver = false;
    
    // 唤醒时强制回到主屏幕 (UX 优化)
    s_currentWindow = 0;
    
    if (animate) {
        lv_scr_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);
    } else {
        lv_scr_load(scr_main);
    }
    
    // 触发一次更新以确保数据最新
    ui_main_update_state(-1);
}

bool ui_is_screensaver() {
    return s_inScreensaver;
}

// =================================================================================
// 屏幕创建函数
// =================================================================================

static lv_obj_t* ui_create_boot_screen(lv_obj_t *parent) {
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Smart Lamp");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t *spinner = lv_spinner_create(scr);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    
    return scr;
}

static lv_obj_t* ui_create_screensaver_screen(lv_obj_t *parent) {
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    
    label_saver_time = lv_label_create(scr);
    lv_label_set_text(label_saver_time, "00:00");
    lv_obj_set_style_text_font(label_saver_time, &lv_font_montserrat_48, 0); // 大字体
    lv_obj_set_style_text_color(label_saver_time, lv_color_white(), 0);
    lv_obj_align(label_saver_time, LV_ALIGN_CENTER, 0, 0);
    
    label_saver_date = lv_label_create(scr);
    lv_label_set_text(label_saver_date, "Loading...");
    lv_obj_set_style_text_font(label_saver_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_saver_date, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align_to(label_saver_date, label_saver_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    return scr;
}

void ui_nav(int dir) {
    if (s_inScreensaver) return; // 息屏时不处理导航，或者导航触发唤醒（由 gui_task 处理）
    
    if (!s_inMenu) {
        // ---- 屏幕切换模式 (Screen Switching Mode) ----
        int prevWindow = s_currentWindow;
        s_currentWindow += dir;
        
        // 循环切换 (0-3)
        if (s_currentWindow > 3) s_currentWindow = 0;
        if (s_currentWindow < 0) s_currentWindow = 3;

        if (prevWindow != s_currentWindow) {
            lv_obj_t *next_scr = nullptr;
            lv_scr_load_anim_t anim_type = (dir > 0) ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT;

            switch(s_currentWindow) {
                case 0: next_scr = scr_main; break;
                case 1: next_scr = scr_lamp; break;
                case 2: next_scr = scr_settings; break;
                case 3: next_scr = scr_status; break;
            }

            if (next_scr) {
                // 确保目标屏幕不隐藏，否则动画可能看不到内容
                lv_obj_clear_flag(next_scr, LV_OBJ_FLAG_HIDDEN);
                
                // 使用动画加载新屏幕
                lv_scr_load_anim(next_scr, anim_type, 300, 0, false);
                
                // 我们不再手动调用 ui_*_set_visible(false) 来隐藏旧屏幕，
                // 因为 lv_scr_load_anim 会处理屏幕切换。
                // 但是，我们需要确保新屏幕的 update 函数能正常工作。
                
                // 触发一次立即更新，确保新屏幕数据显示最新
                switch(s_currentWindow) {
                    case 0: ui_main_update_state(-1); break; // 示例：触发刷新
                    case 3: ui_status_update(); break;
                }
            }
        }
    } else {
        // ---- 菜单内导航模式 (In-Menu Navigation Mode) ----
        if (s_currentWindow == 1) { // 灯光屏幕
             if (!s_lampEditMode) {
                 // 焦点导航: 在亮度、色温、自动亮度之间切换
                 s_lampFocusIndex += dir;
                 if (s_lampFocusIndex < 0) s_lampFocusIndex = 0;
                 if (s_lampFocusIndex > 2) s_lampFocusIndex = 2;
                 ui_lamp_apply_focus(s_lampEditMode, s_lampFocusIndex);
             } else {
                 // 数值调整: 改变滑块值
                 ui_lamp_handle_nav(dir, s_lampEditMode, s_lampFocusIndex);
             }
        } else if (s_currentWindow == 2) { // 设置屏幕
            if (s_settingsSubMenu == 1) {
                ui_settings_wifi_nav(dir);
            } else {
                // 焦点导航: 在设置项之间切换
                s_settingsFocusIndex += dir;
                if (s_settingsFocusIndex < 0) s_settingsFocusIndex = 0;
                if (s_settingsFocusIndex > 4) s_settingsFocusIndex = 4; // max index = 4 (Restart is last)
                ui_settings_apply_focus(s_settingsFocusIndex);
            }
        } else if (s_currentWindow == 3) { // 状态屏幕
            s_statusFocusIndex += dir;
            if (s_statusFocusIndex < 0) s_statusFocusIndex = 0;
            if (s_statusFocusIndex > 6) s_statusFocusIndex = 6;
            ui_status_apply_focus(s_statusFocusIndex);
        }
    }
}

void ui_enter_menu() {
    if (s_currentWindow == 1) { // 灯光屏幕
        if (!s_inMenu) {
            // 进入菜单: 激活焦点
            s_inMenu = true;
            s_lampFocusIndex = 0; // 默认选中亮度
            s_lampEditMode = false;
            ui_lamp_apply_focus(s_lampEditMode, s_lampFocusIndex);
        } else {
            // 切换编辑模式 (选择 -> 编辑 -> 选择)
            s_lampEditMode = !s_lampEditMode;
            ui_lamp_apply_focus(s_lampEditMode, s_lampFocusIndex);
        }
    } else if (s_currentWindow == 2) { // 设置屏幕
        if (!s_inMenu) {
            // 进入菜单: 激活焦点
            s_inMenu = true;
            s_settingsFocusIndex = 0; // 默认选中第一项
            ui_settings_apply_focus(s_settingsFocusIndex);
        } else {
            if (s_settingsSubMenu == 1) {
                ui_settings_wifi_select();
            } else {
                if (s_settingsFocusIndex == 3) {
                    // Enter WiFi Sub-menu
                    s_settingsSubMenu = 1;
                    ui_settings_show_wifi_list(true);
                } else {
                    // 切换开关状态
                    ui_settings_toggle_item(s_settingsFocusIndex);
                }
            }
        }
    } else if (s_currentWindow == 3) { // 状态屏幕
        if (!s_inMenu) {
            s_inMenu = true;
            s_statusFocusIndex = 0;
            ui_status_apply_focus(s_statusFocusIndex);
        }
    }
}

void ui_exit_menu() {
    if (s_currentWindow == 1) {
        if (s_lampEditMode) {
            // 如果在编辑模式，仅退出编辑模式，保留在菜单中
            s_lampEditMode = false;
            ui_lamp_apply_focus(s_lampEditMode, s_lampFocusIndex);
            return;
        }
        // 否则退出菜单
        s_inMenu = false;
        ui_lamp_clear_focus();
        s_lampFocusIndex = 0; // Reset to top
        ui_lamp_reset_view(); // Scroll to top
    } else if (s_currentWindow == 2) {
        if (s_settingsSubMenu == 1) {
            // Exit sub-menu
            s_settingsSubMenu = 0;
            ui_settings_show_wifi_list(false);
        } else {
            s_inMenu = false;
            ui_settings_apply_focus(-1); // 清除焦点
            s_settingsFocusIndex = 0; // Reset to top
            ui_settings_reset_view(); // Scroll to top
        }
    } else if (s_currentWindow == 3) {
        s_inMenu = false;
        ui_status_apply_focus(-1);
        s_statusFocusIndex = 0;
        ui_status_reset_view();
    } else {
        s_inMenu = false;
    }
}

// =================================================================================
// 更新转发器 (Update Forwarders)
// =================================================================================

void ui_update_time(int hour, int minute, int second) {
    ui_main_update_time(hour, minute, second);
    
    if (s_inScreensaver && label_saver_time) {
        lv_label_set_text_fmt(label_saver_time, "%02d:%02d", hour, minute);
        
        // 仅在分钟变化或初始化时对齐，避免每秒重绘导致闪烁
        if (second == 0) {
            lv_obj_align(label_saver_time, LV_ALIGN_CENTER, 0, 0);
            if (label_saver_date) {
                lv_obj_align_to(label_saver_date, label_saver_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
            }
        }
    }
}

void ui_update_date(const char* dateStr) {
    if (s_inScreensaver && label_saver_date) {
        lv_label_set_text(label_saver_date, dateStr);
        // 重新对齐
        if (label_saver_time) {
            lv_obj_align_to(label_saver_date, label_saver_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
        }
    }
}

void ui_update_state(int state) {
    ui_main_update_state(state);
}

void ui_update_sensor_data(float temp, float humi, float lux, int radar_dist) {
    ui_main_update_temp(temp);
    ui_main_update_humi(humi);
    ui_main_update_lux(lux);
    ui_main_update_radar_dist(radar_dist);
}

void ui_update_temperature(float temp) {
    ui_main_update_temp(temp);
}

void ui_update_humidity(float humi) {
    ui_main_update_humi(humi);
}

void ui_update_lux(float lux) {
    ui_main_update_lux(lux);
}

void ui_update_radar_dist(int dist) {
    ui_main_update_radar_dist(dist);
}

void ui_update_radar_state(int state) {
    ui_main_update_radar_state(state);
}

void ui_update_status_page() {
    ui_status_update();
}

void ui_update_ip(const char* ip) {
    ui_main_update_ip(ip);
}

void ui_update_wifi_state(bool connected, int rssi) {
    ui_main_update_wifi_state(connected, rssi);
}

void ui_update_ble_state(bool connected) {
    ui_main_update_ble_state(connected);
}

void ui_update_mqtt_status(bool connected) {
    // ui_main_update_mqtt(connected); // Removed from main screen
    if (s_currentWindow == 3) {
        ui_status_update();
    }
}

void ui_update_battery(int level) {
    ui_main_update_battery(level);
}

void ui_update_light_state(bool on) {
    // ui_main_update_light(on); // Removed from main screen
}

void ui_update_brightness(uint8_t val) {
    ui_lamp_update_brightness(val);
}

void ui_update_cct(uint16_t val) {
    ui_lamp_update_cct(val);
}

void ui_set_light(bool on) {
    ui_update_light_state(on);
}
