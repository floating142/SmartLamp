#include "lamp.hpp"
#include <FastLED.h>

// Gamma 2.2 校正表 (0-100) -> (0-255)
// 重新生成以提供更好的低亮度分辨率
const uint8_t LampController::GAMMA_TABLE[101] = {
    0, 0, 1, 1, 2, 3, 4, 5, 6, 8, 9, 11, 13, 15, 17, 19, 21, 24, 26, 29,
    32, 35, 38, 41, 44, 48, 51, 55, 59, 63, 67, 71, 75, 80, 84, 89, 94, 99, 104, 109,
    114, 119, 125, 130, 136, 142, 148, 154, 160, 166, 172, 179, 185, 192, 199, 206, 213, 220, 227, 234,
    242, 249, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};
// 注意：上面的表只填充到了 62 (255)。因为 0-100 映射到 0-255，实际上 100 对应 255。
// 让我们用更精确的计算替换查表，或者使用正确的表。
// 既然用户抱怨低亮度问题，我们直接在 scaleChannel 中使用浮点计算或者高精度整数计算，
// 废弃 GAMMA_TABLE 的查表方式，或者仅将其作为参考。
// 为了保持兼容性，我们更新 scaleChannel 函数，不再依赖 GAMMA_TABLE 的 0-100 映射，
// 而是直接计算。

/**
 * @brief 缩放通道值
 * 
 * 结合 Gamma 校正和全局亮度，计算最终的 PWM 输出值。
 * 
 * @param value 原始通道值 (0-255)
 * @param brightness 全局亮度 (0-100)
 * @return uint8_t 缩放后的值 (0-255)
 */
uint8_t LampController::scaleChannel(uint16_t value, uint8_t brightness) const {
    if (brightness == 0) return 0;
    if (value == 0) return 0;

    // 1. 计算目标物理亮度 (0-255 范围)
    // 将 0-100 的逻辑亮度映射到 kMinPwmOutput - kMaxPwmOutput 的物理 PWM 范围
    // 使用 Gamma 2.0 曲线使亮度变化更自然
    
    uint32_t target_pwm;
    
    if (brightness <= 10) {
        // 低亮度区间 (1-10%)：线性映射到 kMinPwmOutput 的低端部分
        // 假设 kMinPwmOutput 是物理上能点亮的最小值
        // 我们让 1% 对应 kMinPwmOutput，10% 对应一个稍亮的值
        // 为了平滑，我们定义 low_range_max 为 kMinPwmOutput + (kMax - kMin) * 0.05
        
        uint32_t range = kMaxPwmOutput - kMinPwmOutput;
        uint32_t low_end = kMinPwmOutput;
        uint32_t high_end = kMinPwmOutput + range / 10; // 10% 的物理范围
        
        target_pwm = map(brightness, 1, 10, low_end, high_end);
    } else {
        // 高亮度区间 (>10%)：Gamma 2.0 映射
        // 输入 11-100 -> 输出 high_end -> kMaxPwmOutput
        
        uint32_t range = kMaxPwmOutput - kMinPwmOutput;
        uint32_t low_end = kMinPwmOutput + range / 10;
        
        // 归一化输入 0-89 (对应 11-100)
        uint32_t b_norm = brightness - 10;
        uint32_t b_sq = b_norm * b_norm; // Max 89*89 = 7921
        
        // 映射 b_sq (0-7921) 到 (low_end -> kMaxPwmOutput)
        uint32_t pwm_range = kMaxPwmOutput - low_end;
        target_pwm = low_end + (b_sq * pwm_range) / 7921;
    }

    // 2. 应用通道颜色值 (value: 0-255)
    // 最终输出 = value * target_pwm / 255
    uint32_t numerator = (uint32_t)value * target_pwm;
    return (uint8_t)(numerator / 255);
}

/**
 * @brief 更新 LED 显示
 * 
 * 当没有特效运行时，根据当前颜色和亮度更新 LED。
 */
void LampController::update() {
    if (m_effect != EffectMode::None) return;

    uint8_t r = 0, g = 0, b = 0;
    if (m_useCCT) {
        cctToRGB(m_cct, m_brightness, r, g, b);
    } else {
        r = scaleChannel(m_rgbColor.r, m_brightness);
        g = scaleChannel(m_rgbColor.g, m_brightness);
        b = scaleChannel(m_rgbColor.b, m_brightness);
    }
    fill_solid(m_leds, LAMP_NUM_LEDS, CRGB(r, g, b));
    FastLED.show();
}

/**
 * @brief CCT 转原始 RGB
 * 
 * 将色温值转换为 RGB 值，不考虑亮度。
 * 
 * @param cct 色温 (2700-6500)
 * @param r 输出 R
 * @param g 输出 G
 * @param b 输出 B
 */
void LampController::cctToRawRGB(uint16_t cct, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (cct < LAMP_CCT_MIN) cct = LAMP_CCT_MIN;
    if (cct > LAMP_CCT_MAX) cct = LAMP_CCT_MAX;

    const uint8_t warmR = 255, warmG = 147, warmB = 41;
    const uint8_t coolR = 255, coolG = 255, coolB = 255;

    uint32_t span = LAMP_CCT_MAX - LAMP_CCT_MIN;
    uint32_t pos = cct - LAMP_CCT_MIN;
    uint32_t alphaQ10 = (span == 0) ? 0 : (pos * 1024u) / span;

    auto lerpQ10 = [&](uint8_t a, uint8_t b) -> uint16_t {
        return (uint16_t)((a * (1024u - alphaQ10) + b * alphaQ10) / 1024u);
    };

    r = (uint8_t)lerpQ10(warmR, coolR);
    g = (uint8_t)lerpQ10(warmG, coolG);
    b = (uint8_t)lerpQ10(warmB, coolB);
}

/**
 * @brief CCT 转最终 RGB
 * 
 * 将色温值转换为 RGB 值，并应用亮度缩放。
 * 
 * @param cct 色温
 * @param brightness 亮度
 * @param r 输出 R
 * @param g 输出 G
 * @param b 输出 B
 */
void LampController::cctToRGB(uint16_t cct, uint8_t brightness, uint8_t &r, uint8_t &g, uint8_t &b) {
    uint8_t rawR, rawG, rawB;
    cctToRawRGB(cct, rawR, rawG, rawB);
    r = scaleChannel(rawR, brightness);
    g = scaleChannel(rawG, brightness);
    b = scaleChannel(rawB, brightness);
}
