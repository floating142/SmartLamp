#include "lamp.hpp"

// =================================================================================
// 渐变与动画
// =================================================================================

/**
 * @brief 启动亮度渐变
 * 
 * @param targetPercent 目标亮度 (0-100)
 * @param duration_ms 渐变持续时间 (ms)
 */
void LampController::fadeToBrightness(uint8_t targetPercent, uint16_t duration_ms) {
    if (targetPercent > 100) targetPercent = 100;

    uint8_t diff = (targetPercent > m_brightness) ? (targetPercent - m_brightness) : (m_brightness - targetPercent);
    uint32_t actual_duration = ((uint32_t)duration_ms * diff) / 100;
    
    // 避免时间过短
    if (actual_duration < 50 && diff > 0 && duration_ms > 0) actual_duration = 50;
    if (duration_ms == 0) actual_duration = 0;

    // 修复：如果目标亮度很低（例如关灯），确保渐变能执行到底
    // 原逻辑可能因为 diff 小导致 actual_duration 很短，或者步进计算问题
    
    m_targetBrightness = targetPercent;
    m_fadeDurationMs = (uint16_t)actual_duration;
    m_fadeActive = true;
}

void LampController::cancelFade() { 
    m_fadeActive = false; 
}

bool LampController::isFading() const { 
    return m_fadeActive; 
}

void LampController::setFadeCurve(FadeCurve curve) { 
    m_curve = curve; 
}

FadeCurve LampController::getFadeCurve() const { 
    return m_curve; 
}

bool LampController::advanceFade(uint32_t &elapsed, uint8_t &start) {
    if (m_brightness == m_targetBrightness || m_fadeDurationMs == 0) {
        m_brightness = m_targetBrightness;
        update();
        m_fadeActive = false;
        start = m_brightness;
        elapsed = 0;
        return false;
    }

    elapsed += STEP_MS;
    if (elapsed >= m_fadeDurationMs) elapsed = m_fadeDurationMs;

    uint32_t t_q16 = (uint32_t)(((uint64_t)elapsed << 16) / m_fadeDurationMs);
    t_q16 = applyEasing(t_q16);

    int16_t delta = (int16_t)m_targetBrightness - (int16_t)start;
    
    // 使用更精确的计算，避免低亮度时的截断误差
    // 将计算域扩大，最后再缩小
    int32_t val = (int32_t)start * 65536 + (int32_t)delta * (int32_t)t_q16;
    uint8_t cur = (uint8_t)(val >> 16);
    
    if (cur != m_brightness) {
        m_brightness = cur;
        update();
    }

    if (elapsed >= m_fadeDurationMs) {
        if (m_brightness != m_targetBrightness) {
            m_brightness = m_targetBrightness;
            update();
        }
        
        m_fadeActive = false;
        start = m_brightness;
        elapsed = 0;
        return false;
    }
    return true;
}

bool LampController::advanceColorFade(uint32_t &elapsed) {
    elapsed += STEP_MS;
    if (elapsed >= m_colorFadeDurationMs) elapsed = m_colorFadeDurationMs;

    uint32_t t_q16 = (uint32_t)(((uint64_t)elapsed << 16) / m_colorFadeDurationMs);
    t_q16 = applyEasing(t_q16);

    if (m_useCCT) {
        int32_t delta = (int32_t)m_targetCCT - (int32_t)m_startCCT;
        m_cct = (uint16_t)(m_startCCT + ((delta * (int32_t)t_q16) >> 16));
    } else {
        m_rgbColor.r = m_startRGB.r + ((((int16_t)m_targetRGB.r - m_startRGB.r) * (int32_t)t_q16) >> 16);
        m_rgbColor.g = m_startRGB.g + ((((int16_t)m_targetRGB.g - m_startRGB.g) * (int32_t)t_q16) >> 16);
        m_rgbColor.b = m_startRGB.b + ((((int16_t)m_targetRGB.b - m_startRGB.b) * (int32_t)t_q16) >> 16);
    }

    update();

    if (elapsed >= m_colorFadeDurationMs) {
        if (m_fadingToCCT) {
            m_useCCT = true;
            m_cct = m_targetCCT;
            m_fadingToCCT = false;
        } else if (m_useCCT) {
            m_cct = m_targetCCT;
        } else {
            m_rgbColor = m_targetRGB;
        }
        update();

        m_colorFadeActive = false;
        elapsed = 0;
        return false;
    }
    return true;
}

uint32_t LampController::applyEasing(uint32_t t) const {
    if (t == 0) return 0;
    if (t >= 65535) return 65535;
    
    switch (m_curve) {
        case FadeCurve::Linear:
            return t;
        case FadeCurve::EaseIn:
            return (uint32_t)(((uint64_t)t * t) >> 16);
        case FadeCurve::EaseOut: {
            uint32_t u = 65535u - t;
            return 65535u - (uint32_t)(((uint64_t)u * u) >> 16);
        }
        case FadeCurve::EaseInOut:
        case FadeCurve::Smoothstep: {
            uint64_t t_64 = t;
            uint64_t t2 = (t_64 * t_64) >> 16;
            uint64_t t3 = (t2 * t_64) >> 16;
            return (uint32_t)((3 * t2) - (2 * t3));
        }
        default:
            return t;
    }
}
