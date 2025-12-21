#include "lamp.hpp"

// =================================================================================
// 持久化与存储
// =================================================================================

/**
 * @brief 标记状态已改变
 * 
 * 记录当前时间，用于延迟写入 NVS。
 */
void LampController::markChanged() {
    m_lastChangeMs = millis();
}

/**
 * @brief 检查并写入 NVS
 * 
 * 在 taskLoop 中调用。如果状态改变超过一定时间（COMMIT_DELAY_MS），则写入 NVS。
 */
void LampController::flushIfIdle() {
    if (!(m_dirty_on || m_dirty_br || m_dirty_cct || m_dirty_rgb || m_dirty_mode || m_dirty_auto_br)) return;
    uint32_t now = millis();
    if (m_lastChangeMs == 0) return;
    if (now - m_lastChangeMs >= COMMIT_DELAY_MS) {
        flushNow();
    }
}

/**
 * @brief 立即写入 NVS
 */
void LampController::flushNow() {
    if (m_dirty_on) { 
        saveOnToNVS(); 
        m_dirty_on = false; 
    }
    if (m_dirty_br) { 
        saveSavedBrightnessToNVS(); 
        m_dirty_br = false; 
    }
    if (m_dirty_cct) { 
        saveCCTToNVS(); 
        m_dirty_cct = false; 
    }
    if (m_dirty_rgb) {
        saveRGBToNVS();
        m_dirty_rgb = false;
    }
    if (m_dirty_mode) {
        saveModeToNVS();
        m_dirty_mode = false;
    }
    if (m_dirty_auto_br) {
        saveAutoBrightnessToNVS();
        m_dirty_auto_br = false;
    }
    m_lastChangeMs = 0;
}

/**
 * @brief 从 NVS 加载状态
 * 
 * 在 begin() 中调用。
 */
void LampController::loadStateFromNVS() {
    AppConfig::instance().begin();
    bool on = true;
    uint8_t br = 50;
    uint16_t cct = 4000;
    uint8_t r = 255, g = 255, b = 255;
    bool isCCT = true;
    bool autoBr = false;
    
    (void)AppConfig::instance().loadOn(on);
    (void)AppConfig::instance().loadSavedBrightness(br);
    (void)AppConfig::instance().loadCCT(cct);
    (void)AppConfig::instance().loadRGB(r, g, b);
    (void)AppConfig::instance().loadMode(isCCT);
    (void)AppConfig::instance().loadAutoBrightness(autoBr);

    if (br == 0) br = 50; 
    if (br < 1) br = 1;
    if (cct < LAMP_CCT_MIN) cct = LAMP_CCT_MIN;
    if (cct > LAMP_CCT_MAX) cct = LAMP_CCT_MAX;

    m_on = on;
    m_savedOnBrightness = br;
    m_cct = cct;
    m_rgbColor = CRGB(r, g, b);
    m_useCCT = isCCT;
    m_autoBrightness = autoBr;
}

void LampController::saveOnToNVS() {
    AppConfig::instance().begin();
    AppConfig::instance().saveOn(m_on);
}

void LampController::saveSavedBrightnessToNVS() {
    AppConfig::instance().begin();
    uint8_t br = (m_savedOnBrightness > 0) ? m_savedOnBrightness : 50;
    AppConfig::instance().saveSavedBrightness(br);
}

void LampController::saveCCTToNVS() {
    AppConfig::instance().begin();
    AppConfig::instance().saveCCT(m_cct);
}

void LampController::saveRGBToNVS() {
    AppConfig::instance().begin();
    AppConfig::instance().saveRGB(m_rgbColor.r, m_rgbColor.g, m_rgbColor.b);
}

void LampController::saveModeToNVS() {
    AppConfig::instance().begin();
    AppConfig::instance().saveMode(m_useCCT);
}

void LampController::saveAutoBrightnessToNVS() {
    AppConfig::instance().begin();
    AppConfig::instance().saveAutoBrightness(m_autoBrightness);
}
