#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <vector>

/**
 * @brief NVS 存储封装类
 * 
 * 负责将灯光状态（开关、亮度、色温、RGB颜色、模式）持久化到 Flash。
 * 使用 ESP32 Preferences 库。
 */
class AppConfig {
public:
    static AppConfig& instance() {
        static AppConfig instance;
        return instance;
    }

    void begin();

    // ---- 读取接口 ----
    bool loadOn(bool &on);
    bool loadSavedBrightness(uint8_t &br);
    bool loadCCT(uint16_t &cct);
    bool loadRGB(uint8_t &r, uint8_t &g, uint8_t &b);
    bool loadMode(bool &isCCT); // true=CCT, false=RGB
    bool loadMQTT(String &host, int &port, String &user, String &pass);
    bool loadPowerSaveMode(bool &enabled);
    bool loadWeatherConfig(float &lat, float &lon, String &city);
    bool loadAutoBrightness(bool &enabled);
    bool loadDebugMode(bool &enabled);
    bool loadRadarEnable(bool &enabled);

    struct WifiCred {
        String ssid;
        String pass;
    };
    bool loadWifiList(std::vector<WifiCred> &list);

    // ---- 写入接口 ----
    void saveOn(bool on);
    void saveSavedBrightness(uint8_t br);
    void saveCCT(uint16_t cct);
    void saveRGB(uint8_t r, uint8_t g, uint8_t b);
    void saveMode(bool isCCT);
    void addWifi(const String &ssid, const String &password);
    void removeWifi(const String &ssid);
    void clearWifiList();

    void saveMQTT(const String &host, int port, const String &user, const String &pass);
    void savePowerSaveMode(bool enabled);
    void saveWeatherConfig(float lat, float lon, const String &city);
    void saveAutoBrightness(bool enabled);
    void saveDebugMode(bool enabled);
    void saveRadarEnable(bool enabled);

    // Generic helper (if needed publicly, otherwise keep private or specific)
    void putInt(const char* key, int32_t value);
    int32_t getInt(const char* key, int32_t defaultValue = 0);

private:
    Preferences prefs_;
    static constexpr const char *NS = "lamp";
    
    // Keys
    static constexpr const char *K_ON = "on";
    static constexpr const char *K_BR = "br";
    static constexpr const char *K_CCT = "cct";
    static constexpr const char *K_RGB = "rgb";   // 存储为 uint32_t (0x00RRGGBB)
    static constexpr const char *K_MODE = "mode"; // bool: true=CCT, false=RGB
    static constexpr const char *K_WIFI_SSID = "ssid";
    static constexpr const char *K_WIFI_PASS = "pass";
    static constexpr const char *K_MQTT_HOST = "m_host";
    static constexpr const char *K_MQTT_PORT = "m_port";
    static constexpr const char *K_MQTT_USER = "m_user";
    static constexpr const char *K_MQTT_PASS = "m_pass";
    static constexpr const char *K_PSM = "psm";
    static constexpr const char *K_LAT = "lat";
    static constexpr const char *K_LON = "lon";
    static constexpr const char *K_CITY = "city";
    static constexpr const char *K_DEBUG = "debug";
};

