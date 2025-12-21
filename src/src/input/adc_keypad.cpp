
/**
 * @file analog_input.cpp
 * @brief 单线键盘模拟输入驱动
 * 
 * 处理 ADC 读取、消抖以及多按键模拟输入系统的事件生成。
 * 支持短按、长按和连发操作。
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../ui/gui_task.hpp"
#include "adc_keypad.hpp"

// ---- 配置常量 ----

// ADC 引脚配置
static constexpr int PIN_ANALOG_INPUT = 0;   // GPIO0

// 时间配置
static constexpr uint32_t TIME_LONG_PRESS_MS   = 500; // 长按阈值
static constexpr uint32_t TIME_REPEAT_START_MS = 500; // 开始连发前的等待时间
static constexpr uint32_t TIME_REPEAT_RATE_MS  = 80;  // 连发事件间隔
static constexpr uint32_t TIME_DEBOUNCE_MS     = 60;  // 状态切换所需的稳定时间

// 采样间隔
static constexpr uint32_t POLL_FAST_MS = 10;   // 活跃/过渡状态下的采样率
static constexpr uint32_t POLL_SLOW_MS = 100;  // 空闲状态下的采样率

// 电压阈值 (mV)
static constexpr int THRESHOLD_NO_PRESS = 100;

// 调试配置
// #define ENABLE_ANALOG_LOGGING
#ifdef ENABLE_ANALOG_LOGGING
    #define LOG_ANALOG(...) Serial.printf(__VA_ARGS__)
#else
    #define LOG_ANALOG(...)
#endif

// ---- 内部状态管理 ----

struct InputState {
    int      currentState;      // 当前稳定的逻辑状态 (-99: 无, -1/0/1: 按键)
    int      lastRawState;      // 上次从 ADC 读取的原始状态 (用于消抖)
    uint32_t stableTimer;       // 信号稳定计时器 (消抖)
    uint32_t holdTimer;         // 按键保持时长计时器
    uint32_t repeatTimer;       // 连发事件生成计时器
    bool     btn1PressActive;   // 标志: 按键 1 按压周期处于活动状态
    bool     btn1LongPressSent; // 标志: 按键 1 长按事件已发送

    InputState() : 
        currentState(-99), 
        lastRawState(-99), 
        stableTimer(0), 
        holdTimer(0), 
        repeatTimer(0), 
        btn1PressActive(false), 
        btn1LongPressSent(false) {}
};

static InputState s_state;

// ---- 辅助函数 ----

/**
 * @brief 读取 ADC 电压（毫伏）。
 */
static inline int read_adc_mv() {
    return analogReadMilliVolts(PIN_ANALOG_INPUT);
}

/**
 * @brief 将电压映射到逻辑按键状态。
 * 
 * @param mv 电压（毫伏）
 * @return int 按键 ID (-1, 0, 1) 或 -99 (无按键按下)
 * 
 * 修改后映射:
 * 原按键 0 (左/下) -> -1
 * 原按键 1 (确认)   -> 0
 * 原按键 2 (右/上) -> 1
 */
static inline int classify_state(int mv) {
    //Serial.println(mv);

    // 按键 -1: 左/下
    if (mv > 800 && mv <= 1100) return -1;
    
    // 按键 0: 确认/菜单
    if (mv > 1200 && mv <= 1500) return 0;
    
    // 按键 1: 右/上
    if (mv > 2100 && mv <= 2400) return 1;
    
    // 无按键
    if (mv <= THRESHOLD_NO_PRESS) return -99;
    
    // 未定义的电压范围
    return -99;
}

/**
 * @brief 根据当前状态确定下一个采样间隔。
 */
static inline uint32_t get_next_poll_interval(int state) {
    // 如果有按键按下，快速采样以捕捉释放/变化
    return (state == -99) ? POLL_SLOW_MS : POLL_FAST_MS;
}

/**
 * @brief 发送导航事件到 UI 队列。
 */
static void send_nav_event(int direction) {
    UIEvent evt{UI_EVENT_NAV, direction};
    send_ui_event(evt, (uint8_t)DEST_MQTT | DEST_BLE);
}

/**
 * @brief 发送状态变更事件到 UI 队列（用于调试/状态显示）。
 */
static void send_state_event(int state) {
    UIEvent evt{UI_EVENT_STATE, state};
    send_ui_event(evt , (uint8_t)DEST_MQTT | DEST_BLE);
}

/**
 * @brief 处理状态保持不变时的逻辑（长按/连发逻辑）。
 */
static void handle_stable_state(uint32_t elapsed_ms) {
    if (s_state.currentState == -99) return;
    
    s_state.holdTimer += elapsed_ms;

    // 按键 0: 长按 -> 退出菜单
    if (s_state.currentState == 0) {
        if (s_state.holdTimer >= TIME_LONG_PRESS_MS && !s_state.btn1LongPressSent) {
            UIEvent evt{UI_EVENT_EXIT_MENU, 0};
            send_ui_event(evt, (uint8_t)DEST_MQTT | DEST_BLE);
            s_state.btn1LongPressSent = true;
            LOG_ANALOG("[Analog] Button 0 Long Press: Exit Menu\n");
        }
    }

    // 按键 -1 & 1: 长按 -> 连发导航
    if (s_state.currentState == -1 || s_state.currentState == 1) {
        if (s_state.holdTimer >= TIME_REPEAT_START_MS) {
            s_state.repeatTimer += elapsed_ms;
            if (s_state.repeatTimer >= TIME_REPEAT_RATE_MS) {
                s_state.repeatTimer = 0;
                int dir = s_state.currentState; // 直接使用 -1 或 1
                send_nav_event(dir);
                // LOG_ANALOG("[Analog] Button %d Repeat\n", s_state.currentState);
            }
        }
    }
}

/**
 * @brief 在消抖后提交状态转换。
 */
static void commit_state_transition(int newState, int prevState, int voltageMv) {
    uint32_t previousHoldTime = s_state.holdTimer;

    // 更新状态
    s_state.currentState = newState;
    s_state.holdTimer = 0;
    s_state.repeatTimer = 0;

    // 如果是有效按键按下（非释放），则唤醒屏幕
    if (s_state.currentState != -99) {
        gui_report_activity();
    }

    // --- 按键 0 逻辑 (确认/退出) ---
    if (s_state.currentState == 0) {
        // 按键 0 按下
        s_state.btn1PressActive = true;
        s_state.btn1LongPressSent = false;
    } else if (prevState == 0) {
        // 按键 0 释放
        // 如果是短按且未发送长按退出命令
        if (s_state.btn1PressActive && !s_state.btn1LongPressSent) {
            if (previousHoldTime < TIME_LONG_PRESS_MS) {
                UIEvent evt{UI_EVENT_ENTER_MENU, 0};
                send_ui_event(evt, (uint8_t)DEST_MQTT | DEST_BLE);
                LOG_ANALOG("[Analog] Button 0 Short Press: Enter Menu\n");
            }
        }
        s_state.btn1PressActive = false;
        s_state.btn1LongPressSent = false;
    }

    // --- 按键 -1 & 1 逻辑 (导航) ---
    // 按下瞬间触发立即步进
    if (s_state.currentState == -1 || s_state.currentState == 1) {
        int dir = s_state.currentState; // 直接使用 -1 或 1
        send_nav_event(dir);
        LOG_ANALOG("[Analog] Button %d Press: Nav %d\n", s_state.currentState, dir);
    }

    // 通知系统状态变更
    send_state_event(s_state.currentState);

    if (s_state.currentState != -99) {
        LOG_ANALOG("[Analog] State -> %d (%dmV)\n", s_state.currentState, voltageMv);
    } else {
        LOG_ANALOG("[Analog] Released (%dmV)\n", voltageMv);
    }
}

/**
 * @brief 在提交更改之前检查状态稳定性（消抖）。
 */
static void process_debounce(int rawState, int voltageMv, uint32_t elapsed_ms) {
    if (rawState == s_state.lastRawState) {
        s_state.stableTimer += elapsed_ms;
    } else {
        s_state.lastRawState = rawState;
        s_state.stableTimer = 0;
    }

    if (s_state.stableTimer >= TIME_DEBOUNCE_MS) {
        // 状态稳定
        if (s_state.currentState != rawState) {
            int prevState = s_state.currentState;
            s_state.stableTimer = 0; 
            commit_state_transition(rawState, prevState, voltageMv);
        }
    }
}

// ---- 内部逻辑 ----

static uint32_t get_initial_poll_ms() {
    return POLL_SLOW_MS;
}

static uint32_t process_step(uint32_t elapsed_ms) {
    int mv = read_adc_mv();
    int rawState = classify_state(mv);

    // 自适应采样：如果处于活跃或过渡状态则快速采样，空闲则慢速采样
    uint32_t nextPoll = get_next_poll_interval(rawState);

    if (rawState == s_state.currentState) {
        // 状态稳定（物理上保持按下或释放）
        handle_stable_state(elapsed_ms);
        
        // 保持消抖状态同步，以避免信号短暂抖动导致故障
        s_state.lastRawState = rawState; 
        s_state.stableTimer = 0; 
    } else {
        // 状态正在变化或不稳定
        process_debounce(rawState, mv, elapsed_ms);
    }

    return nextPoll;
}

// ---- 任务实现 ----

static TaskHandle_t s_analogTaskHandle = nullptr;

/**
 * @brief 模拟量读取任务循环
 */
static void task_analog_read(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    
    // 获取初始采样间隔
    uint32_t poll_ms = get_initial_poll_ms();
    
    for (;;) {
        // 执行一步模拟量处理，并获取下一次建议的等待时间
        poll_ms = process_step(poll_ms);
        
        // 精确延时
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(poll_ms));
    }
}

// ---- 公共 API ----

void setup_analog_input_task() {
    xTaskCreate(
        task_analog_read,
        "Analog Task",
        2048,
        NULL,
        1, // 优先级较低
        &s_analogTaskHandle
    );
    Serial.println("[Analog] Task started");
}
