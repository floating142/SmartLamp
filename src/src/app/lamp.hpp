#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../system/storage.hpp"

// LED 配置
#define LAMP_NUM_LEDS 64
#define LAMP_DATA_PIN 10
#define LAMP_LED_TYPE WS2812
#define LAMP_COLOR_ORDER GRB

// 色温范围（K）
#define LAMP_CCT_MIN 2700
#define LAMP_CCT_MAX 6500


// 统一的物理亮度限制（全局映射用）
static constexpr uint8_t LAMP_PWM_HARD_MIN = 10; // 物理最小占空比
static constexpr uint8_t LAMP_PWM_HARD_MAX = 80; // 物理最大占空比

// 渐变曲线类型
enum class FadeCurve : uint8_t {
	Linear,        // 线性
	EaseIn,        // 缓入（平方）
	EaseOut,       // 缓出（平方）
	EaseInOut,     // 缓入缓出（余弦）
	Smoothstep     // 更平滑的 S 曲线（smootherstep 近似）
};

// 灯光特效模式
enum class EffectMode : uint8_t {
    None = 0,       // 无特效 (静态)
    Rainbow,        // 彩虹流光
    Breathing,      // 呼吸灯
    Police,         // 警灯模式 (红蓝闪烁)
    Night,          // 夜灯模式 (微光)
    Reading,        // 阅读模式 (冷白高亮)
    Spin,           // 旋转彩虹 (针对环形布局)
    Meteor          // 流星拖尾 (针对环形布局)
};

class LampController {
public:
    // 1. 生命周期与任务
	void init();
	void startTask();

    // 2. 核心控制接口
    void setPower(bool on, uint16_t fade_ms, uint8_t excludeMask = 0);   // 逻辑开关（带渐变）
	void togglePower(uint16_t fade_ms);         // 切换开关
	bool isOn() const;                          // 获取开关状态

	void setBrightness(uint8_t percent, uint16_t fade_ms = 0, uint8_t excludeMask = 0);  // 0-100, 支持渐变
	uint8_t getBrightness() const;              // 获取当前亮度

	void setCCT(uint16_t cct, uint16_t fade_ms = 0, uint8_t excludeMask = 0);                  // 设置色温 (切换到 CCT 模式)
	uint16_t getCCT() const;                    // 获取色温

    void setColor(uint8_t r, uint8_t g, uint8_t b, uint16_t fade_ms = 0, uint8_t excludeMask = 0); // 设置 RGB 颜色 (切换到 RGB 模式)
    CRGB getRGB() const;                            // 获取 RGB 颜色
    void setHSV(uint8_t h, uint8_t s, uint8_t v, uint16_t fade_ms = 0, uint8_t excludeMask = 0);   // 设置 HSV 颜色 (切换到 RGB 模式)
    bool isCCTMode() const;                         // 当前是否为色温模式

    // 特效控制
    void setEffect(EffectMode mode);
    void setEffect(const char* effectName); // 根据名称设置特效
    EffectMode getEffect() const;
    void setScene(const char* scene, uint8_t excludeMask = 0); // 设置场景模式
    String getScene() const; // 获取当前场景名称

	void setSavedBrightness(uint8_t percent);   // 设置记忆亮度
	uint8_t getSavedBrightness() const;         // 获取记忆亮度

    // 3. 渐变与动画
	void fadeToBrightness(uint8_t targetPercent, uint16_t duration_ms);
	void cancelFade();
	bool isFading() const;

	void setFadeCurve(FadeCurve curve);
	FadeCurve getFadeCurve() const;

    // 4. 持久化
	void flushNow(); // 立即提交脏数据

    // 5. 自动亮度
    void setAutoBrightness(bool enable);
    bool isAutoBrightness() const;

private:
    // 亮度与物理限制常量（供各实现文件复用）
    static constexpr uint8_t kMaxPwmOutput = LAMP_PWM_HARD_MAX;
    static constexpr uint8_t kMinPwmOutput = LAMP_PWM_HARD_MIN;
    static constexpr uint8_t kMinVisibleBrightness = (uint16_t)kMinPwmOutput * 100 / kMaxPwmOutput;

    // Gamma 查表（声明，定义在 lamp_render.cpp）
    static const uint8_t GAMMA_TABLE[101];

    String m_scene = "None"; // 当前场景名称

    // 5. 内部实现与硬件驱动
	void update();
    void cctToRawRGB(uint16_t cct, uint8_t &r, uint8_t &g, uint8_t &b);
	void cctToRGB(uint16_t cct, uint8_t brightness, uint8_t &r, uint8_t &g, uint8_t &b);
    uint8_t scaleChannel(uint16_t value, uint8_t brightness) const;
	
    static void taskEntry(void *pvParameters);
	void taskLoop();
	
    bool advanceFade(uint32_t &elapsed_ms, uint8_t &start_brightness);
    bool advanceColorFade(uint32_t &elapsed_ms); // 颜色渐变步进

    // 应用曲线 (输入输出均为 Q16 定点数: 0..65535)
    uint32_t applyEasing(uint32_t t_q16) const;

    // 状态变量
	CRGB m_leds[LAMP_NUM_LEDS];
	uint8_t m_brightness = 50;
	uint16_t m_cct = 4000;
    CRGB m_rgbColor = CRGB::White; // RGB 模式下的基色
    bool m_useCCT = true;          // true=CCT模式, false=RGB模式
	
    // 亮度渐变相关
    volatile bool m_fadeActive = false;
	uint8_t m_targetBrightness = 50;
	uint16_t m_fadeDurationMs = 0;
    FadeCurve m_curve = FadeCurve::Linear;

    // 颜色渐变相关
    volatile bool m_colorFadeActive = false;
    uint16_t m_colorFadeDurationMs = 0;
    uint16_t m_startCCT = 0;
    uint16_t m_targetCCT = 0;
    CRGB m_startRGB;
    CRGB m_targetRGB;
    bool m_fadingToCCT = false; // 标记是否正在从 RGB 渐变回 CCT 模式

    // 特效状态
    EffectMode m_effect = EffectMode::None;
    uint32_t m_effectTick = 0;
    void runEffect(); // 执行特效逻辑

    // 逻辑状态与记忆
	bool m_on = false; 
	uint8_t m_savedOnBrightness = 50;
	TaskHandle_t m_taskHandle = nullptr;
	static constexpr uint16_t STEP_MS = 10;

    // 延迟提交相关
    static constexpr uint32_t COMMIT_DELAY_MS = 1000; 
    
    // 线程安全
    SemaphoreHandle_t m_mutex = nullptr;

    // 内部辅助：加载/保存状态
    void loadStateFromNVS();
    void markChanged();
    void flushIfIdle();

	void saveOnToNVS();
	void saveSavedBrightnessToNVS();
	void saveCCTToNVS();
    void saveRGBToNVS();
    void saveModeToNVS();
    
    // 脏标记
    bool m_dirty_on = false;
    bool m_dirty_br = false;
    bool m_dirty_cct = false;
    bool m_dirty_rgb = false;
    bool m_dirty_mode = false;
    bool m_dirty_auto_br = false;
    uint32_t m_lastChangeMs = 0;

    // 自动亮度
    bool m_autoBrightness = false;
    void saveAutoBrightnessToNVS();
};

extern LampController lamp;
