#pragma once

#include <Arduino.h>
#include <Wire.h>

// ---- 寄存器定义 ----
#define CW2015_I2C_ADDR       0x62

#define CW2015_REG_VERSION    0x00
#define CW2015_REG_VCELL      0x02
#define CW2015_REG_SOC        0x04
#define CW2015_REG_RRT_ALERT  0x06
#define CW2015_REG_CONFIG     0x08
#define CW2015_REG_MODE       0x0A
#define CW2015_REG_BATINFO    0x10  // 长度 = 64 字节

// 初始化
bool cw2015_init();
// 读取
void cw2015_read();

// 查询接口
/**
 * @brief 是否已有有效读数
 */
bool cw2015_has_reading();

/**
 * @brief 获取打包后的 UI 值 (value, 包含充电标志)
 * @return >=0 有效值, <0 表示尚无读数
 */
int cw2015_get_ui_value();
/**
 * @brief 如果值有变更则返回打包后的 UI 值并清除变更标志，否则返回 -1
 */
int cw2015_take_ui_value_if_changed();

/**
 * @brief CW2015 电池电量计驱动类
 * 
 * 负责与 CW2015 芯片通信，读取电压和 SOC (State of Charge)。
 */
class CW2015 {
public:
    CW2015(TwoWire &wire = Wire);
    
    /**
     * @brief 初始化检查
     * @return true 如果设备存在且通信正常
     */
    bool begin();
    
    /**
     * @brief 读取电池电压
     * @param[out] vcell_mv 电压值 (毫伏)
     * @return true 读取成功
     */
    bool readVCell(uint16_t &vcell_mv);
    
    /**
     * @brief 读取剩余电量百分比 (SOC)
     * @param[out] soc 百分比 (0.0 - 100.0)
     * @return true 读取成功
     */
    bool readSOC(float &soc);
    
    /**
     * @brief 唤醒芯片 (退出睡眠模式)
     */
    bool wakeUp();

    /**
     * @brief 进入睡眠模式 (低功耗)
     */
    bool sleep();

    /**
     * @brief 调试用：打印寄存器内容
     */
    void dumpRegisters();

private:
    TwoWire &_wire;
    
    bool readReg(uint8_t reg, uint8_t *buf, size_t len);
    bool writeReg(uint8_t reg, uint8_t val);
};

/**
 * @brief 初始化 CW2015 并启动监控任务
 * 
 * 该任务将周期性读取电池状态，并在电量变化时发送 UI_EVENT_BATTERY 事件。
 */
void setup_cw2015_task();
