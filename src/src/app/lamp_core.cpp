#include "lamp.hpp"
#include "../ui/gui_task.hpp"
#include <FastLED.h>

// 全局单例实例
LampController lamp;

// =================================================================================
// 1. 生命周期与任务管理
// =================================================================================

void LampController::init() {
    FastLED.addLeds<LAMP_LED_TYPE, LAMP_DATA_PIN, LAMP_COLOR_ORDER>(m_leds, LAMP_NUM_LEDS);
    FastLED.clear(true);

    m_mutex = xSemaphoreCreateMutex();

    loadStateFromNVS();
    m_dirty_on = m_dirty_br = m_dirty_cct = false;
    m_lastChangeMs = 0;

    m_brightness = 0;
    update();

    if (m_on) {
        uint8_t saved = (m_savedOnBrightness > 0 ? m_savedOnBrightness : 50);
        uint8_t target = map(saved, 1, 100, kMinVisibleBrightness, 100);
        fadeToBrightness(target, 1000);
    }
    // Notify UI of current lamp state so UI can initialize correctly
    UIEvent evt_br{UI_EVENT_BRIGHTNESS, (int)getSavedBrightness(), 0.0f};
    send_ui_event(evt_br);
    UIEvent evt_cct{UI_EVENT_CCT, (int)getCCT(), 0.0f};
    send_ui_event(evt_cct);
    UIEvent evt_auto_br{UI_EVENT_AUTO_BR, isAutoBrightness() ? 1 : 0};
    send_ui_event(evt_auto_br);
}

void LampController::startTask() {
    if (m_taskHandle == nullptr) {
        xTaskCreate(
            LampController::taskEntry,
            "LampTask",
            2048,
            this,
            1,
            &m_taskHandle
        );
    }
}

void LampController::taskEntry(void *pvParameters) {
    auto *self = static_cast<LampController *>(pvParameters);
    if (self) self->taskLoop(); else vTaskDelete(nullptr);
}

void LampController::taskLoop() {
    TickType_t last = xTaskGetTickCount();
    uint32_t elapsed = 0;
    uint32_t elapsedColor = 0;
    uint8_t start = m_brightness;

    uint8_t lastTarget = m_targetBrightness;
    uint16_t lastTargetCCT = m_targetCCT;
    CRGB lastTargetRGB = m_targetRGB;
    bool lastUseCCT = m_useCCT;

    for (;;) {
        if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
            // 1) 亮度渐变
            if (m_fadeActive && m_targetBrightness != lastTarget) {
                start = m_brightness;
                elapsed = 0;
                lastTarget = m_targetBrightness;
            }

            if (m_fadeActive) {
                (void)advanceFade(elapsed, start);
            } else {
                start = m_brightness;
                elapsed = 0;
                lastTarget = m_targetBrightness;
            }

            // 2) 颜色渐变
            bool colorChanged = false;
            if (m_useCCT != lastUseCCT) {
                colorChanged = true;
            } else if (m_useCCT) {
                if (m_targetCCT != lastTargetCCT) colorChanged = true;
            } else {
                if (m_targetRGB.r != lastTargetRGB.r ||
                    m_targetRGB.g != lastTargetRGB.g ||
                    m_targetRGB.b != lastTargetRGB.b) {
                    colorChanged = true;
                }
            }

            if (m_colorFadeActive && colorChanged) {
                elapsedColor = 0;
                lastUseCCT = m_useCCT;
                lastTargetCCT = m_targetCCT;
                lastTargetRGB = m_targetRGB;
            }

            if (m_colorFadeActive) {
                (void)advanceColorFade(elapsedColor);
            } else {
                elapsedColor = 0;
                lastUseCCT = m_useCCT;
                lastTargetCCT = m_targetCCT;
                lastTargetRGB = m_targetRGB;
            }

            // 3) 延迟存储
            flushIfIdle();
            
            // 4) 特效
            if (m_effect != EffectMode::None) {
                if (m_on || m_brightness > 0) {
                    runEffect();
                } else {
                    FastLED.clear();
                    FastLED.show();
                }
            }
            
            xSemaphoreGive(m_mutex);
        }

        vTaskDelayUntil(&last, pdMS_TO_TICKS(STEP_MS));
    }
}

// =================================================================================
// 2. 核心控制接口
// =================================================================================

/**
 * @brief 设置逻辑电源状态（带渐变）
 * 
 * @param on true=开灯, false=关灯
 * @param fade_ms 渐变时间(ms)
 */
void LampController::setPower(bool on, uint16_t fade_ms, uint8_t excludeMask) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
        cancelFade();
        
        if (on) {
            m_on = true;
            uint8_t saved = getSavedBrightness();
            uint8_t target = map(saved, 1, 100, kMinVisibleBrightness, 100);
            fadeToBrightness(target, fade_ms);
            UIEvent evt{UI_EVENT_LIGHT, 1};
            send_ui_event(evt, excludeMask);
        } else {
            m_on = false;
            
            // 特殊处理：如果正在运行特效，用户希望“直接关闭”而不是等待渐变或特效周期
            if (m_effect != EffectMode::None) {
                fadeToBrightness(0, 0); // 立即关闭
            } else {
                fadeToBrightness(0, fade_ms);
            }
            
            UIEvent evt{UI_EVENT_LIGHT, 0};
            send_ui_event(evt, excludeMask);
        }
        
        m_dirty_on = true; 
        markChanged();
        xSemaphoreGive(m_mutex);
    }
}

/**
 * @brief 切换电源状态
 */
void LampController::togglePower(uint16_t fade_ms) {
    setPower(!m_on, fade_ms);
}

/**
 * @brief 获取当前逻辑电源状态
 */
bool LampController::isOn() const { 
    return m_on; 
}

/**
 * @brief 设置亮度
 * 
 * @param percent 用户输入的亮度 0-100
 * @param fade_ms 渐变时间 (0 表示立即设置)
 */
void LampController::setBrightness(uint8_t percent, uint16_t fade_ms, uint8_t excludeMask) {
    if (percent > 100) percent = 100;
    
    // 修正：用户设置亮度最小为 1%，0% 仅由关灯触发
    if (percent == 0) percent = 1;
    
    uint8_t internal_val = 0;
    if (percent > 0) {
        internal_val = map(percent, 1, 100, kMinVisibleBrightness, 100);
    }

    if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
        if (m_on) {
            if (fade_ms > 0) {
                fadeToBrightness(internal_val, fade_ms);
            } else {
                cancelFade();
                m_brightness = internal_val;
                update();
            }
            
            if (percent > 0) {
                m_savedOnBrightness = percent;
                m_dirty_br = true;
            }

            UIEvent evt{UI_EVENT_BRIGHTNESS, percent};
            send_ui_event(evt, excludeMask);
        }
        xSemaphoreGive(m_mutex);
    }
}

/**
 * @brief 获取当前实际输出亮度
 */
uint8_t LampController::getBrightness() const { 
    return m_brightness; 
}

/**
 * @brief 设置色温 (切换到 CCT 模式)
 */
void LampController::setCCT(uint16_t cct, uint16_t fade_ms, uint8_t excludeMask) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
        if (cct < LAMP_CCT_MIN) cct = LAMP_CCT_MIN;
        if (cct > LAMP_CCT_MAX) cct = LAMP_CCT_MAX;

        if (m_useCCT && fade_ms > 0) {
            m_startCCT = m_cct;
            m_targetCCT = cct;
            m_colorFadeDurationMs = fade_ms;
            m_colorFadeActive = true;
            m_fadingToCCT = false;
        } else if (!m_useCCT && fade_ms > 0) {
            m_startRGB = m_rgbColor;
            uint8_t r, g, b;
            cctToRawRGB(cct, r, g, b);
            m_targetRGB = CRGB(r, g, b);
            m_targetCCT = cct;
            m_useCCT = false;
            m_colorFadeDurationMs = fade_ms;
            m_colorFadeActive = true;
            m_fadingToCCT = true;
        } else {
            m_cct = cct;
            m_useCCT = true;
            m_colorFadeActive = false;
            m_fadingToCCT = false;
            update();
        }
        
        UIEvent evt{UI_EVENT_CCT, cct};
        send_ui_event(evt, excludeMask);

        m_dirty_cct = true;
        m_dirty_mode = true;
        markChanged();
        xSemaphoreGive(m_mutex);
    }
}

/**
 * @brief 获取当前色温
 */
uint16_t LampController::getCCT() const { 
    if (m_colorFadeActive && m_useCCT) return m_targetCCT;
    return m_cct; 
}

/**
 * @brief 设置 RGB 颜色 (切换到 RGB 模式)
 */
void LampController::setColor(uint8_t r, uint8_t g, uint8_t b, uint16_t fade_ms, uint8_t excludeMask) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
        CRGB target(r, g, b);
        
        if (!m_useCCT && fade_ms > 0) {
            m_startRGB = m_rgbColor;
            m_targetRGB = target;
            m_colorFadeDurationMs = fade_ms;
            m_colorFadeActive = true;
            m_fadingToCCT = false;
        } else if (m_useCCT && fade_ms > 0) {
            uint8_t r0, g0, b0;
            cctToRawRGB(m_cct, r0, g0, b0);
            m_startRGB = CRGB(r0, g0, b0);
            m_targetRGB = target;
            m_useCCT = false;
            m_colorFadeDurationMs = fade_ms;
            m_colorFadeActive = true;
            m_fadingToCCT = false;
        } else {
            m_rgbColor = target;
            m_useCCT = false;
            m_colorFadeActive = false;
            m_fadingToCCT = false;
            update();
        }

        int rgbValue = (r << 16) | (g << 8) | b;
        UIEvent evt{UI_EVENT_RGB, rgbValue};
        send_ui_event(evt, excludeMask);

        m_dirty_rgb = true;
        m_dirty_mode = true;
        markChanged();
        xSemaphoreGive(m_mutex);
    }
}

/**
 * @brief 设置 HSV 颜色 (切换到 RGB 模式)
 */
void LampController::setHSV(uint8_t h, uint8_t s, uint8_t v, uint16_t fade_ms, uint8_t excludeMask) {
    CRGB rgb;
    rgb.setHSV(h, s, v);
    setColor(rgb.r, rgb.g, rgb.b, fade_ms, excludeMask);
}

/**
 * @brief 获取当前 RGB 颜色
 */
CRGB LampController::getRGB() const {
    if (m_colorFadeActive && !m_useCCT) return m_targetRGB;
    return m_rgbColor;
}

/**
 * @brief 当前是否为 CCT 模式
 */
bool LampController::isCCTMode() const {
    return m_useCCT;
}

/**
 * @brief 设置开灯记忆亮度
 */
void LampController::setSavedBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (percent > 0) {
        if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
            m_savedOnBrightness = percent;
            m_dirty_br = true; 
            markChanged();
            xSemaphoreGive(m_mutex);
        }
    }
}

/**
 * @brief 获取开灯记忆亮度
 */
uint8_t LampController::getSavedBrightness() const { 
    return m_savedOnBrightness > 0 ? m_savedOnBrightness : 50; 
}

// =================================================================================
// 3. 特效控制（接口）
// =================================================================================

/**
 * @brief 设置特效模式
 */
void LampController::setEffect(EffectMode mode) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY)) {
        m_effect = mode;
        m_effectTick = 0;
        if (mode != EffectMode::None) {
            m_scene = "None"; // 启用特效时清除场景
        }
        if (mode == EffectMode::None) {
            update();
        }
        xSemaphoreGive(m_mutex);
    }
}

/**
 * @brief 获取当前特效模式
 */
EffectMode LampController::getEffect() const {
    return m_effect;
}

/**
 * @brief 设置特效模式 (字符串)
 */
void LampController::setEffect(const char* effectName) {
    String s = String(effectName);
    s.toLowerCase();
    
    EffectMode mode = EffectMode::None;
    if (s == "rainbow") mode = EffectMode::Rainbow;
    else if (s == "breathing") mode = EffectMode::Breathing;
    else if (s == "police") mode = EffectMode::Police;
    else if (s == "spin") mode = EffectMode::Spin;
    else if (s == "meteor") mode = EffectMode::Meteor;
    else if (s == "none") mode = EffectMode::None;
    
    setEffect(mode);
}

/**
 * @brief 设置场景模式
 * 
 * 封装了常用的场景预设，如阅读、夜灯等。
 */
void LampController::setScene(const char* scene, uint8_t excludeMask) {
    String s = String(scene);
    s.toLowerCase();
    
    // 场景模式 (宏)
    if (s == "reading") {
        setCCT(4500, 500, excludeMask);
        setBrightness(80, 500, excludeMask);
        m_scene = "Reading";
    }
    else if (s == "night") {
        setCCT(2700, 500, excludeMask);
        setBrightness(5, 500, excludeMask);
        m_scene = "Night";
    }
    else if (s == "cozy") {
        setCCT(3000, 500, excludeMask);
        setBrightness(50, 500, excludeMask);
        m_scene = "Cozy";
    }
    else if (s == "bright") {
        setCCT(6000, 500, excludeMask);
        setBrightness(100, 500, excludeMask);
        m_scene = "Bright";
    }
    else if (s == "none") {
        m_scene = "None";
    }
    
    // 场景模式通常意味着退出特效
    if (s != "none") {
        setEffect(EffectMode::None);
    }
    
    // 发送 UI 事件通知状态变更 (这里不再发送 EFFECT 事件，因为 setEffect 会发送)
    // 如果需要通知 Scene 变更，可能需要新的事件类型，或者复用 EFFECT 事件但带特殊值？
    // 目前 UI 似乎没有显示当前 Scene 的功能，只显示 Effect。
}

String LampController::getScene() const {
    return m_scene;
}

void LampController::setAutoBrightness(bool enable) {
    if (m_autoBrightness != enable) {
        m_autoBrightness = enable;
        m_dirty_auto_br = true;
        markChanged();
        
        UIEvent evt = {UI_EVENT_AUTO_BR, enable ? 1 : 0};
        send_ui_event(evt);
    }
}

bool LampController::isAutoBrightness() const {
    return m_autoBrightness;
}

