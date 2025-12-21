#pragma once

#include <Arduino.h>

/**
 * @brief 初始化 BH1750 传感器
 * @return true 初始化成功
 */
bool bh1750_init();

/**
 * @brief 读取一次光照强度并上报 UI 事件
 */
void bh1750_read();

// ---- 查询接口 ----

/**
 * @brief 检查是否已有有效读数
 */
bool bh1750_has_reading();

/**
 * @brief 获取当前光照强度 (Lux)
 */
float bh1750_get_lux();

/**
 * @brief 获取最后的错误代码
 * @return 0 表示正常, 负值表示错误
 */
int bh1750_last_error();
