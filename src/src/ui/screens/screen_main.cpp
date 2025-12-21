/**
 * @file screen_main.cpp
 * @brief 主仪表盘屏幕实现 (Main Dashboard Screen Implementation)
 */

#include "screen_main.hpp"
#include "../ui_common.hpp"
#include <stdio.h> // 用于 snprintf/dtostrf
#include <Arduino.h>
#include <cstring>

static lv_obj_t *win_main = nullptr;
static lv_obj_t *label_state = nullptr;
static lv_obj_t *label_wifi = nullptr;
static lv_obj_t *label_ble = nullptr;
static lv_obj_t *label_time = nullptr;
static lv_obj_t *label_battery = nullptr;
static lv_obj_t *label_temp = nullptr;
static lv_obj_t *label_humi = nullptr;
static lv_obj_t *label_lux = nullptr;
static lv_obj_t *label_radar = nullptr;
static lv_obj_t *label_weather_city = nullptr;
static lv_obj_t *label_weather_info = nullptr;

lv_obj_t* ui_create_main_screen(lv_obj_t *parent) {
    win_main = lv_obj_create(parent);
    lv_obj_set_size(win_main, SCREEN_W, SCREEN_H);
    lv_obj_align(win_main, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_add_flag(win_main, LV_OBJ_FLAG_HIDDEN); // Removed for animation support

    // 添加简单的背景渐变 (可选)
    // 如果要使用图片背景，请取消注释以下代码并包含图片资源
    LV_IMG_DECLARE(bg_image); 
    lv_obj_t * img = lv_image_create(win_main);
    lv_image_set_src(img, &bg_image);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_to_index(img, 0); // 确保背景在最底层

    lv_obj_set_style_bg_color(win_main, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_grad_color(win_main, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_grad_dir(win_main, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(win_main, LV_OPA_COVER, 0);

    // 1. 状态标签 (左上角 - WiFi & BLE)
    label_wifi = lv_label_create(win_main);
    lv_label_set_text(label_wifi, LV_SYMBOL_WIFI); 
    lv_obj_align(label_wifi, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x808080), 0); // 默认灰色

    label_ble = lv_label_create(win_main);
    lv_label_set_text(label_ble, LV_SYMBOL_BLUETOOTH);
    lv_obj_align_to(label_ble, label_wifi, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_text_color(label_ble, lv_color_hex(0x808080), 0); // 默认灰色

    // 2. 电池状态 (右上角)
    label_battery = lv_label_create(win_main);
    lv_label_set_text(label_battery, LV_SYMBOL_BATTERY_EMPTY);
    lv_obj_align(label_battery, LV_ALIGN_TOP_RIGHT, -5, 5);

    // 3. 时间标签 (顶部居中，大字体)
    label_time = lv_label_create(win_main);
    // 尝试使用较大的字体，如果 lv_font_montserrat_28 不可用，请确保在 lv_conf.h 中启用
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_28, 0); 
    lv_label_set_text(label_time, "00:00");
    lv_obj_align(label_time, LV_ALIGN_TOP_MID, 0, 10); // 移动到顶部居中


    // 4. 传感器数据 (右下角堆叠)
    // 为了对齐整齐，可以从下往上布局
    
    // 光照 (最下方)
    label_lux = lv_label_create(win_main);
    lv_label_set_text(label_lux, "Lux: --");
    lv_obj_set_style_text_font(label_lux, &lv_font_montserrat_14, 0);
    lv_obj_align(label_lux, LV_ALIGN_BOTTOM_RIGHT, -15, -6);

    // 湿度 (光照上方)
    label_humi = lv_label_create(win_main);
    lv_label_set_text(label_humi, "H: --.- %");
    lv_obj_set_style_text_font(label_humi, &lv_font_montserrat_14, 0);
    lv_obj_align_to(label_humi, label_lux, LV_ALIGN_OUT_TOP_RIGHT, 0, -2);

    // 温度 (湿度上方)
    label_temp = lv_label_create(win_main);
    lv_label_set_text(label_temp, "T: --.- °C"); // 添加单位符号
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_14, 0);
    lv_obj_align_to(label_temp, label_humi, LV_ALIGN_OUT_TOP_RIGHT, 0, -2);

    // 5. 天气信息 (左下角 -> 优化布局)
    // 使用一个容器来包裹天气信息，使其看起来像个 Widget
    lv_obj_t *weather_cont = lv_obj_create(win_main);
    lv_obj_set_size(weather_cont, 100, 60);
    lv_obj_align(weather_cont, LV_ALIGN_BOTTOM_LEFT, 5, -5);
    lv_obj_set_style_bg_color(weather_cont, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(weather_cont, LV_OPA_60, 0); // 半透明
    lv_obj_set_style_radius(weather_cont, 10, 0);
    lv_obj_set_style_border_width(weather_cont, 0, 0);
    lv_obj_clear_flag(weather_cont, LV_OBJ_FLAG_SCROLLABLE);

    // 城市名称
    label_weather_city = lv_label_create(weather_cont);
    lv_label_set_text(label_weather_city, "--");
    lv_obj_set_style_text_font(label_weather_city, &lv_font_montserrat_14, 0);
    lv_obj_align(label_weather_city, LV_ALIGN_TOP_LEFT, 0, -10);
    lv_obj_set_width(label_weather_city, 90);
    lv_label_set_long_mode(label_weather_city, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // 天气信息 (图标 + 温度)
    label_weather_info = lv_label_create(weather_cont);
    lv_label_set_text(label_weather_info, "--\n--°C"); 
    lv_obj_set_style_text_font(label_weather_info, &lv_font_montserrat_14, 0);
    lv_obj_align(label_weather_info, LV_ALIGN_BOTTOM_LEFT, 0, 10);

    // 调试状态 (隐藏)
    label_state = lv_label_create(win_main);
    lv_obj_add_flag(label_state, LV_OBJ_FLAG_HIDDEN);

    return win_main;
}

void ui_main_set_visible(bool visible) {
    if (!win_main) return;
    if (visible) lv_obj_clear_flag(win_main, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(win_main, LV_OBJ_FLAG_HIDDEN);
}

void ui_main_update_time(int hour, int minute, int second) {
    if (!label_time) return;
    // 仅显示时分，秒数可选
    lv_label_set_text_fmt(label_time, "%02d:%02d", hour, minute);
}

void ui_main_update_state(int state) {
    if (!label_state) return;
    if (state < 0) lv_label_set_text(label_state, "");
    else lv_label_set_text_fmt(label_state, "S:%d", state);
}

void ui_main_update_wifi_state(bool connected, int rssi) {
    if (!label_wifi) return;
    if (connected) {
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x00FF00), 0); // 绿色
    } else {
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x808080), 0); // 灰色
    }
}

void ui_main_update_ble_state(bool connected) {
    if (!label_ble) return;
    if (connected) {
        lv_obj_set_style_text_color(label_ble, lv_color_hex(0x0000FF), 0); // 蓝色
    } else {
        lv_obj_set_style_text_color(label_ble, lv_color_hex(0x808080), 0); // 灰色
    }
}

void ui_main_update_battery(int value) {
    if (!label_battery) return;
    bool charging = (value > 100);
    int soc = charging ? (value - 100) : value;
    
    const char* symbol;
    if (charging) {
        symbol = LV_SYMBOL_CHARGE;
    } else if (soc >= 90) {
        symbol = LV_SYMBOL_BATTERY_FULL;
    } else if (soc >= 70) {
        symbol = LV_SYMBOL_BATTERY_3;
    } else if (soc >= 50) {
        symbol = LV_SYMBOL_BATTERY_2;
    } else if (soc >= 30) {
        symbol = LV_SYMBOL_BATTERY_1;
    } else {
        symbol = LV_SYMBOL_BATTERY_EMPTY;
    }

    // 显示图标和百分比
    lv_label_set_text_fmt(label_battery, "%s %d%%", symbol, soc);

    if (charging) {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0x00FF00), 0); // 绿色
    } else if (soc < 20) {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0xFF0000), 0); // 红色
    } else {
        lv_obj_set_style_text_color(label_battery, lv_color_hex(0x000000), 0); // 黑色
    }
}

void ui_main_update_temp(float t) {
    if (!label_temp) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", t);
    lv_label_set_text_fmt(label_temp, "T: %s °C", buf);
}

void ui_main_update_humi(float h) {
    if (!label_humi) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", h);
    lv_label_set_text_fmt(label_humi, "H: %s %%", buf);
}

void ui_main_update_lux(float lux) {
    if (!label_lux) return;
    if (lux >= 0.0f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", lux);
        lv_label_set_text_fmt(label_lux, "Lux: %s", buf);
    } else {
        lv_label_set_text(label_lux, "Lux: --");
    }
}

void ui_main_update_radar_dist(int dist) {
    // Deprecated
}

void ui_main_update_radar_state(int state) {
    // Deprecated
}

void ui_main_update_ip(const char* ip) {
    // 目前主屏幕不显示 IP
}

void ui_main_update_weather(const char* city, const char* weather, const char* temp) {
    if (label_weather_city) {
        lv_label_set_text(label_weather_city, city);
    }
    if (label_weather_info) {
        // Simple icon mapping based on text
        const char* icon = "";
        String w = String(weather);
        w.toLowerCase();
        if (w.indexOf("sun") >= 0 || w.indexOf("clear") >= 0) icon = "Sun";
        else if (w.indexOf("cloud") >= 0) icon = "Cloud";
        else if (w.indexOf("rain") >= 0) icon = "Rain";
        else if (w.indexOf("snow") >= 0) icon = "Snow";
        
        if (strlen(icon) > 0) {
             lv_label_set_text_fmt(label_weather_info, "%s\n%s °C", icon, temp);
        } else {
             lv_label_set_text_fmt(label_weather_info, "%s °C", temp);
        }
    }
}