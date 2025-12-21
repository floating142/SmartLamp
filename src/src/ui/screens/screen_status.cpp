/**
 * @file screen_status.cpp
 * @brief 系统状态屏幕实现 (System Status Screen Implementation)
 */

#include "screen_status.hpp"
#include "../ui_common.hpp"
#include "../../network/mqtt_task.hpp" // 获取 MQTT 配置
#include <Arduino.h>
#include <WiFi.h>

static lv_obj_t *win_status = nullptr;
static lv_obj_t *cont_status = nullptr;

static lv_obj_t *label_ssid = nullptr;
static lv_obj_t *label_rssi = nullptr;
static lv_obj_t *label_heap = nullptr;
static lv_obj_t *label_uptime = nullptr;
static lv_obj_t *label_mac = nullptr;
static lv_obj_t *label_ip = nullptr;
static lv_obj_t *label_mqtt = nullptr;

// 使用公共创建信息项函数

lv_obj_t* ui_create_status_screen(lv_obj_t *parent) {
    win_status = lv_obj_create(parent);
    lv_obj_set_size(win_status, SCREEN_W, SCREEN_H);
    lv_obj_align(win_status, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *status_title = lv_label_create(win_status);
    lv_label_set_text(status_title, "System Status");
    lv_obj_align(status_title, LV_ALIGN_TOP_MID, 0, 5);

    cont_status = ui_create_list_container(win_status);

    // 1. WiFi SSID
    ui_create_info_item(cont_status, LV_SYMBOL_WIFI, "WiFi", &label_ssid);

    // 2. IP Address
    ui_create_info_item(cont_status, LV_SYMBOL_HOME, "IP", &label_ip);

    // 3. RSSI
    ui_create_info_item(cont_status, LV_SYMBOL_CHARGE, "Signal", &label_rssi);

    // 4. MQTT
    ui_create_info_item(cont_status, LV_SYMBOL_UPLOAD, "MQTT", &label_mqtt);

    // 5. Heap
    ui_create_info_item(cont_status, LV_SYMBOL_SD_CARD, "Heap", &label_heap);

    // 6. Uptime
    ui_create_info_item(cont_status, LV_SYMBOL_LOOP, "Uptime", &label_uptime);

    // 7. MAC
    ui_create_info_item(cont_status, LV_SYMBOL_SETTINGS, "MAC", &label_mac);

    return win_status;
}

void ui_status_set_visible(bool visible) {
    if (!win_status) return;
    if (visible) {
        lv_obj_clear_flag(win_status, LV_OBJ_FLAG_HIDDEN);
        ui_status_update(); // 显示时立即更新
    } else {
        lv_obj_add_flag(win_status, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_status_update() {
    if (!win_status || lv_obj_has_flag(win_status, LV_OBJ_FLAG_HIDDEN)) return;

    if (label_ssid) {
        String ssid = WiFi.SSID();
        if (ssid.isEmpty()) ssid = "Disconnected";
        lv_label_set_text(label_ssid, ssid.c_str());
    }

    if (label_rssi) {
        int32_t rssi = WiFi.RSSI();
        lv_label_set_text_fmt(label_rssi, "%d dBm", rssi);
    }

    if (label_heap) {
        uint32_t freeHeap = ESP.getFreeHeap() / 1024;
        lv_label_set_text_fmt(label_heap, "%u KB", freeHeap);
    }

    if (label_uptime) {
        unsigned long totalSeconds = millis() / 1000;
        int d = totalSeconds / 86400;
        int h = (totalSeconds % 86400) / 3600;
        int m = (totalSeconds % 3600) / 60;
        lv_label_set_text_fmt(label_uptime, "%dd %02dh %02dm", d, h, m);
    }

    if (label_ip) {
        lv_label_set_text(label_ip, WiFi.localIP().toString().c_str());
    }

    if (label_mac) {
        lv_label_set_text(label_mac, WiFi.macAddress().c_str());
    }
    
    if (label_mqtt) {
        lv_label_set_text(label_mqtt, WiFi.status() == WL_CONNECTED ? "Connected" : "Offline");
    }
}

// 使用公共样式函数 `ui_apply_style`（对 info 项保留值标签颜色）

void ui_status_apply_focus(int index) {
    if (!cont_status) return;
    
    uint32_t child_cnt = lv_obj_get_child_cnt(cont_status);
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(cont_status, i);
        ui_apply_style(child, (int)i == index, false, false);
        if ((int)i == index) {
            lv_obj_scroll_to_view(child, LV_ANIM_ON);
        }
    }
}

void ui_status_reset_view() {
    if (!cont_status) return;
    if (lv_obj_get_child_cnt(cont_status) == 0) return;
    lv_obj_t *first = lv_obj_get_child(cont_status, 0);
    if (first) lv_obj_scroll_to_view(first, LV_ANIM_OFF);
}
