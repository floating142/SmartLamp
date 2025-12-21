#include "storage.hpp"

void AppConfig::begin() {
    static bool inited = false;
    if (!inited) {
        prefs_.begin(NS, false);
        inited = true;
    }
}

bool AppConfig::loadOn(bool &on) {
    begin();
    // 默认 true (开)
    on = prefs_.getUChar(K_ON, 1) != 0;
    return true;
}

bool AppConfig::loadSavedBrightness(uint8_t &br) {
    begin();
    // 默认 50%
    br = prefs_.getUChar(K_BR, 50);
    return true;
}

bool AppConfig::loadCCT(uint16_t &cct) {
    begin();
    // 默认 4000K
    cct = prefs_.getUShort(K_CCT, 4000);
    return true;
}

bool AppConfig::loadRGB(uint8_t &r, uint8_t &g, uint8_t &b) {
    begin();
    // 默认白色 (0xFFFFFF)
    uint32_t val = prefs_.getUInt(K_RGB, 0xFFFFFF);
    r = (val >> 16) & 0xFF;
    g = (val >> 8) & 0xFF;
    b = val & 0xFF;
    return true;
}

bool AppConfig::loadMode(bool &isCCT) {
    begin();
    // 默认 CCT 模式 (true)
    isCCT = prefs_.getBool(K_MODE, true);
    return true;
}

void AppConfig::saveOn(bool on) {
    begin();
    prefs_.putUChar(K_ON, on ? 1 : 0);
}

void AppConfig::saveSavedBrightness(uint8_t br) {
    begin();
    prefs_.putUChar(K_BR, br);
}

void AppConfig::saveCCT(uint16_t cct) {
    begin();
    prefs_.putUShort(K_CCT, cct);
}

void AppConfig::saveRGB(uint8_t r, uint8_t g, uint8_t b) {
    begin();
    uint32_t val = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    prefs_.putUInt(K_RGB, val);
}

void AppConfig::saveMode(bool isCCT) {
    begin();
    prefs_.putBool(K_MODE, isCCT);
}

bool AppConfig::loadWifiList(std::vector<WifiCred> &list) {
    begin();
    list.clear();
    
    // 1. 尝试加载旧版单点配置 (迁移逻辑)
    if (prefs_.isKey(K_WIFI_SSID)) {
        String ssid = prefs_.getString(K_WIFI_SSID);
        String pass = prefs_.getString(K_WIFI_PASS);
        if (ssid.length() > 0) {
            list.push_back({ssid, pass});
            // 迁移后可以删除旧key，或者保留作为备份。这里保留。
        }
    }

    // 2. 加载多点配置
    int count = prefs_.getInt("wifi_count", 0);
    for (int i = 0; i < count; i++) {
        String k_ssid = "ssid_" + String(i);
        String k_pass = "pass_" + String(i);
        String ssid = prefs_.getString(k_ssid.c_str());
        String pass = prefs_.getString(k_pass.c_str());
        if (ssid.length() > 0) {
            // 避免重复 (如果旧配置和新列表里有一样的)
            bool exists = false;
            for (const auto& cred : list) {
                if (cred.ssid == ssid) { exists = true; break; }
            }
            if (!exists) list.push_back({ssid, pass});
        }
    }
    return list.size() > 0;
}

void AppConfig::addWifi(const String &ssid, const String &password) {
    begin();
    std::vector<WifiCred> list;
    loadWifiList(list);

    // 检查是否存在，存在则更新密码
    for (auto &cred : list) {
        if (cred.ssid == ssid) {
            cred.pass = password;
            // 保存整个列表
            goto save_all;
        }
    }
    
    // 不存在则添加
    if (list.size() >= 5) {
        // 如果满了，移除最早的一个 (index 0) - 或者是旧版配置
        list.erase(list.begin());
    }
    list.push_back({ssid, password});

save_all:
    // 保存列表
    prefs_.putInt("wifi_count", list.size());
    for (size_t i = 0; i < list.size(); i++) {
        String k_ssid = "ssid_" + String(i);
        String k_pass = "pass_" + String(i);
        prefs_.putString(k_ssid.c_str(), list[i].ssid);
        prefs_.putString(k_pass.c_str(), list[i].pass);
    }
    
    // 同时更新旧版 key 以保持兼容性 (总是指向最新的)
    prefs_.putString(K_WIFI_SSID, ssid);
    prefs_.putString(K_WIFI_PASS, password);
}

void AppConfig::removeWifi(const String &ssid) {
    begin();
    std::vector<WifiCred> list;
    loadWifiList(list);
    
    for (auto it = list.begin(); it != list.end(); ) {
        if (it->ssid == ssid) {
            it = list.erase(it);
        } else {
            ++it;
        }
    }
    
    // 保存列表
    prefs_.putInt("wifi_count", list.size());
    for (size_t i = 0; i < list.size(); i++) {
        String k_ssid = "ssid_" + String(i);
        String k_pass = "pass_" + String(i);
        prefs_.putString(k_ssid.c_str(), list[i].ssid);
        prefs_.putString(k_pass.c_str(), list[i].pass);
    }
}

void AppConfig::clearWifiList() {
    begin();
    prefs_.putInt("wifi_count", 0);
    prefs_.remove(K_WIFI_SSID);
    prefs_.remove(K_WIFI_PASS);
}

bool AppConfig::loadMQTT(String &host, int &port, String &user, String &pass) {
    begin();
    host = prefs_.getString(K_MQTT_HOST, "");
    port = prefs_.getInt(K_MQTT_PORT, 1883);
    user = prefs_.getString(K_MQTT_USER, "");
    pass = prefs_.getString(K_MQTT_PASS, "");
    return (host.length() > 0);
}

bool AppConfig::loadPowerSaveMode(bool &enabled) {
    begin();
    // 默认开启省电模式
    enabled = prefs_.getBool(K_PSM, false); // 默认为关
    return true;
}

void AppConfig::saveMQTT(const String &host, int port, const String &user, const String &pass) {
    begin();
    prefs_.putString(K_MQTT_HOST, host);
    prefs_.putInt(K_MQTT_PORT, port);
    prefs_.putString(K_MQTT_USER, user);
    prefs_.putString(K_MQTT_PASS, pass);
}

void AppConfig::savePowerSaveMode(bool enabled) {
    begin();
    prefs_.putBool(K_PSM, enabled);
}

bool AppConfig::loadWeatherConfig(float &lat, float &lon, String &city) {
    begin();
    if (!prefs_.isKey(K_LAT)) return false;
    lat = prefs_.getFloat(K_LAT, 39.9042);
    lon = prefs_.getFloat(K_LON, 116.4074);
    city = prefs_.getString(K_CITY, "Beijing");
    return true;
}

void AppConfig::saveWeatherConfig(float lat, float lon, const String &city) {
    begin();
    prefs_.putFloat(K_LAT, lat);
    prefs_.putFloat(K_LON, lon);
    prefs_.putString(K_CITY, city);
}

bool AppConfig::loadAutoBrightness(bool &enabled) {
    begin();
    enabled = prefs_.getInt("auto_br", 0) != 0;
    return true;
}

void AppConfig::saveAutoBrightness(bool enabled) {
    begin();
    prefs_.putInt("auto_br", enabled ? 1 : 0);
}

bool AppConfig::loadDebugMode(bool &enabled) {
    begin();
    enabled = prefs_.getBool(K_DEBUG, false); // 默认关闭
    return true;
}

bool AppConfig::loadRadarEnable(bool &enabled) {
    begin();
    enabled = prefs_.getBool("radar_en", true); // 默认开启
    return true;
}

void AppConfig::saveDebugMode(bool enabled) {
    begin();
    prefs_.putBool(K_DEBUG, enabled);
}

void AppConfig::saveRadarEnable(bool enabled) {
    begin();
    prefs_.putBool("radar_en", enabled);
}

void AppConfig::putInt(const char* key, int32_t value) {
    begin();
    prefs_.putInt(key, value);
}

int32_t AppConfig::getInt(const char* key, int32_t defaultValue) {
    begin();
    return prefs_.getInt(key, defaultValue);
}


