/**
 * @file ble_cmd.hpp
 * @brief BLE 指令处理模块
 * 
 * 负责解析和执行通过 BLE 接收到的控制指令和配置指令。
 */

#pragma once

#include <Arduino.h>
#include <string>

/**
 * @brief 处理 BLE 指令
 * 
 * @param cmd 接收到的完整指令字符串 (不含结束符)
 */
void ble_handle_command(const std::string& cmd);
