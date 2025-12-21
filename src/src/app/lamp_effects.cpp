#include "lamp.hpp"
#include <FastLED.h>
#include <math.h>

// =================================================================================
// 特效执行
// =================================================================================

/**
 * @brief 运行当前选定的光效
 * 
 * 在主循环中被调用。根据 m_effect 的值更新 LED 状态。
 * 包含 Rainbow, Breathing, Flow, Spin, Meteor 等效果。
 */
void LampController::runEffect() {
    if (m_effect == EffectMode::None) return;
    
    m_effectTick++;
    uint8_t brightness = m_brightness;
    
    switch (m_effect) {
        case EffectMode::Rainbow: {
            uint8_t hue = (m_effectTick * 2) & 0xFF;
            fill_rainbow(m_leds, LAMP_NUM_LEDS, hue, 7);
            for(int i=0; i<LAMP_NUM_LEDS; i++) {
                m_leds[i].nscale8(scaleChannel(255, brightness));
            }
            break;
        }
        case EffectMode::Breathing: {
            // 使用 exp(sin(x)) 产生更自然的呼吸曲线
            float val = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
            
            // 限制范围
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            
            uint8_t breathBri = (uint8_t)val;
            
            // 映射到 [50, 255] 区间，确保最低亮度不为 0，防止看起来像熄灭
            breathBri = map(breathBri, 0, 255, 50, 255);
            
            uint8_t finalBri = scaleChannel(breathBri, brightness);
            
            uint8_t r, g, b;
            if (m_useCCT) {
                cctToRawRGB(m_cct, r, g, b);
            } else {
                r = m_rgbColor.r; g = m_rgbColor.g; b = m_rgbColor.b;
            }
            
            fill_solid(m_leds, LAMP_NUM_LEDS, CRGB(r, g, b));
            for(int i=0; i<LAMP_NUM_LEDS; i++) {
                m_leds[i].nscale8(finalBri);
            }
            break;
        }
        case EffectMode::Police: {
            // 警灯特效: 红蓝旋转 -> 爆闪
            // 假设 runEffect 约 20ms 调用一次
            uint32_t cycle = m_effectTick % 400; // 8秒一个大周期
            
            if (cycle < 300) { // 前6秒旋转 (红蓝各半)
                int offset = ((LAMP_NUM_LEDS / 4) - (cycle / 2) % (LAMP_NUM_LEDS / 4)); // 逆时针旋转
                for(int i=0; i<LAMP_NUM_LEDS; i++) {
                    int panel = i / 16;
                    int local_x = i % 4;
                    int x = panel * 4 + local_x;
                    int pos = (x + offset) % (LAMP_NUM_LEDS / 4);
                    if (pos < LAMP_NUM_LEDS / 8) {
                        m_leds[i] = CRGB::Red;
                    } else {
                        m_leds[i] = CRGB::Blue;
                    }
                }
            } else { // 后2秒爆闪
                // 每 5 ticks (100ms) 切换一次状态
                int flashPhase = (cycle - 300) / 5;
                // 模拟警灯爆闪节奏: 红红 蓝蓝 红红 蓝蓝
                // 0: Red, 1: Off, 2: Red, 3: Off, 4: Blue, 5: Off, 6: Blue, 7: Off ...
                
                if (flashPhase % 4 == 0 || flashPhase % 4 == 2) {
                    fill_solid(m_leds, LAMP_NUM_LEDS, (flashPhase / 8) % 2 == 0 ? CRGB::Red : CRGB::Blue);
                } else {
                    fill_solid(m_leds, LAMP_NUM_LEDS, CRGB::Black);
                }
            }
            
            for(int i=0; i<LAMP_NUM_LEDS; i++) {
                m_leds[i].nscale8(scaleChannel(255, brightness));
            }
            break;
        }
        case EffectMode::Spin: {
            uint8_t baseHue = (m_effectTick * 2) & 0xFF;
            for(int i=0; i<LAMP_NUM_LEDS; i++) {
                int panel = i / 16;
                int local_x = i % 4;
                int x = panel * 4 + local_x;
                uint8_t hue = baseHue + (x * 16);
                m_leds[i] = CHSV(hue, 255, 255);
                m_leds[i].nscale8(scaleChannel(255, brightness));
            }
            break;
        }
        case EffectMode::Meteor: {
            fadeToBlackBy(m_leds, LAMP_NUM_LEDS, 40);
            int headX = (m_effectTick / 3) % 16;
            
            uint8_t r, g, b;
            if (m_useCCT) {
                cctToRawRGB(m_cct, r, g, b);
            } else {
                r = m_rgbColor.r; g = m_rgbColor.g; b = m_rgbColor.b;
            }
            
            for(int i=0; i<LAMP_NUM_LEDS; i++) {
                int panel = i / 16;
                int local_x = i % 4;
                int x = panel * 4 + local_x;
                if (x == headX) {
                    m_leds[i] = CRGB(r, g, b);
                }
                m_leds[i].nscale8(scaleChannel(255, brightness));
            }
            break;
        }
        default:
            break;
    }
    
    FastLED.show();
}
