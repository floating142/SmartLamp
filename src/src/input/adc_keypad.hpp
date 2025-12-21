#pragma once

#include <stdint.h>

/**
 * @brief 初始化并启动模拟输入采样任务。
 * 
 * 创建一个 FreeRTOS 任务，周期性地读取 ADC 并处理按键逻辑。
 */
void setup_analog_input_task();

