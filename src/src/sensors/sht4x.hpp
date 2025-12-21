#pragma once

#include <Arduino.h>

/**
 * @brief 初始化 SHT4x 传感器
 */
bool sht4x_init();

/**
 * @brief 读取一次数据并上报
 */
void sht4x_read();

// ---- 查询接口 (返回最后缓存的值) ----

/**
 * @brief 检查是否已有有效读数
 */
bool sht4x_has_reading();

/**
 * @brief 获取当前温度 (摄氏度)
 */
float sht4x_get_temperature();

/**
 * @brief 获取当前相对湿度 (%)
 */
float sht4x_get_humidity();

/**
 * @brief 获取最后的错误代码
 * @return 0 表示正常, 负值表示错误
 */
int16_t sht4x_last_error();
