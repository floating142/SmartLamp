#pragma once

/**
 * @brief 初始化按钮硬件并启动处理任务。
 * 
 * 配置 GPIO 中断、创建信号量，并启动 FreeRTOS 任务以处理按钮事件。
 */
void setup_button_task();
