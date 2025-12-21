/**
 * @file button_input.cpp
 * @brief 物理按钮输入驱动
 * 
 * 处理按钮中断、去抖动（通过信号量机制隐含）以及灯光开关逻辑。
 */

#include "gpio_button.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../app/lamp.hpp"
#include "../ui/gui_task.hpp"
#include "../network/mqtt_task.hpp"

// ---- 配置 ----
static constexpr int PIN_BUTTON_INTERRUPT = 8;   // 按钮连接的 GPIO 引脚
static constexpr uint32_t DEBOUNCE_DELAY_MS = 50; // 简单的去抖动延迟（如果需要）

// ---- 内部状态 ----
static SemaphoreHandle_t s_buttonSemaphore = nullptr;
static TaskHandle_t s_buttonTaskHandle = nullptr;

/**
 * @brief 按钮中断服务程序 (ISR)
 * 
 * 当按钮按下时触发，释放信号量以唤醒处理任务。
 * 保持极简，不做复杂处理。
 */
static void IRAM_ATTR isr_button_handler() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_buttonSemaphore) {
        xSemaphoreGiveFromISR(s_buttonSemaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

/**
 * @brief 按钮处理任务
 * 
 * 等待信号量，然后切换灯光状态并发送 UI 事件。
 */
static void task_button_process(void *pvParameters) {
    for (;;) {
        // 等待信号量（阻塞直到中断发生）
        if (xSemaphoreTake(s_buttonSemaphore, portMAX_DELAY) == pdTRUE) {
            
            // 简单的软件去抖动：再次检查引脚状态或短暂延时
            // 这里直接执行逻辑，因为信号量机制本身限制了频率，
            // 但为了防止机械抖动导致连续触发，可以加一点延时。
            // vTaskDelay(pdMS_TO_TICKS(50)); 
            // if (digitalRead(PIN_BUTTON_INTERRUPT) == LOW) { ... } // 取决于电路逻辑
            
            // 切换 Lamp 内部逻辑开关并执行渐变
            lamp.togglePower(2000); // 2000ms 渐变
            bool nowOn = lamp.isOn();
            
            Serial.printf("[Button] Light %s (brightness=%u)\n", nowOn ? "ON" : "OFF", lamp.getBrightness());
            
            // 通知 GUI 更新状态
            UIEvent evt{UI_EVENT_LIGHT, nowOn ? 1 : 0};
            send_ui_event(evt);

            // 通知 MQTT 更新状态
            mqtt_report_state();
            
            // 防止连击过快
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void setup_button_task() {
    // 1. 创建二进制信号量
    s_buttonSemaphore = xSemaphoreCreateBinary();
    if (!s_buttonSemaphore) {
        Serial.println("[Button] Failed to create semaphore!");
        return;
    }

    // 2. 配置 GPIO
    pinMode(PIN_BUTTON_INTERRUPT, INPUT_PULLUP);
    
    // 3. 绑定中断
    // 注意：根据电路设计选择 RISING, FALLING 或 CHANGE
    // 假设按下连接到 VCC (RISING) 或 GND (FALLING)
    // 原代码使用 RISING，假设按下为高电平或释放为高电平
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_INTERRUPT), isr_button_handler, RISING);

    // 4. 创建处理任务
    xTaskCreate(
        task_button_process,
        "Button Task",
        2048,           // 栈大小
        NULL,           // 参数
        2,              // 优先级
        &s_buttonTaskHandle
    );
    
    Serial.println("[Button] Task initialized");
}
