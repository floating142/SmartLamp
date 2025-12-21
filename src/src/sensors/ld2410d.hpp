#pragma once

#include <Arduino.h>
#include <vector>

namespace Sensor {

// 目标状态定义
enum class RadarState : uint8_t {
    NO_TARGET = 0x00,
    MOVING = 0x01,
    STATIONARY = 0x02
};

// 包含雷达实时数据的结构体
struct RadarData {
    RadarState state;       // 目标状态
    uint16_t distance_cm;   // 目标距离 (cm)
    uint32_t gate_energy[32]; // 32个距离门的能量值
};

class LD2410D {
public:
    /**
     * @brief 构造函数
     * @param stream 串口流对象 (如 Serial1, Serial2)
     */
    LD2410D(Stream& stream);

    /**
     * @brief 初始化
     */
    void begin();

    /**
     * @brief 在主循环中调用，用于处理串口数据
     */
    void update();

    // --- 配置命令 ---

    /**
     * @brief 进入配置模式
     * @return true 成功, false 失败
     */
    bool enableConfiguration();

    /**
     * @brief 退出配置模式
     * @return true 成功, false 失败
     */
    bool endConfiguration();

    /**
     * @brief 读取固件版本
     * @return 版本号字符串，失败返回空字符串
     */
    String readFirmwareVersion();

    /**
     * @brief 开启或关闭工程模式 (输出详细能量值)
     * @param enable true 开启, false 关闭
     * @return true 成功, false 失败
     */
    bool setEngineeringMode(bool enable);

    /**
     * @brief 读取序列号 (MAC地址)
     * @return 序列号字符串
     */
    String readSerialNumber();

    /**
     * @brief 读取基本参数 (最大距离和无人延迟)
     * @param maxDistVal 输出: 最大距离值 (单位: 0.1m, 例如 80 代表 8m)
     * @param duration 输出: 无人延迟时间 (单位: 秒)
     * @return true 成功, false 失败
     */
    bool readBasicParameters(uint8_t& maxDistVal, uint16_t& duration);

    /**
     * @brief 设置基本参数
     * @param maxDistVal 最大距离值 (范围 7~100, 单位 0.1m)
     * @param duration 无人延迟时间 (0~65535 秒)
     * @return true 成功, false 失败
     */
    bool setBasicParameters(uint8_t maxDistVal, uint16_t duration);

    /**
     * @brief 设置指定距离门的灵敏度门限
     * @param gate 距离门索引 (0-15)
     * @param motionThreshold 运动门限 (0-100, 值越小越灵敏)
     * @param staticThreshold 微动/静止门限 (0-100, 值越小越灵敏)
     * @return true 成功
     */
    bool setGateSensitivity(uint8_t gate, uint8_t motionThreshold, uint8_t staticThreshold);

    /**
     * @brief 保存配置到 Flash (掉电不丢失)
     * @return true 成功
     */
    bool saveConfiguration();

    /**
     * @brief 触发自动增益调节 (解决外壳遮挡导致的饱和问题)
     * @return true 成功
     */
    bool startGainCalibration();

    /**
     * @brief 重启模块 (软复位)
     * @return true 成功
     */
    bool restart();

    // --- 数据获取 ---

    /**
     * @brief 获取当前雷达数据
     * @return RadarData 结构体引用
     */
    const RadarData& getData() const;

    /**
     * @brief 是否检测到目标
     */
    bool hasTarget() const;

    /**
     * @brief 打印调试信息到指定串口
     */
    void printDebugInfo(Stream& debugSerial);

    /**
     * @brief 设置底层通信调试输出流
     * @param debugStream 调试串口指针，传入 nullptr 关闭
     */
    void setDebugStream(Stream* debugStream);

private:
    Stream& _stream;
    Stream* _debugStream = nullptr;
    RadarData _data;
    
    // 接收缓冲区
    static const size_t BUFFER_SIZE = 256;
    uint8_t _buffer[BUFFER_SIZE];
    size_t _bufIndex = 0;
    bool _bufferFilled = false;

    // 内部辅助函数
    void processByte(uint8_t byte);
    void parsePacket(const uint8_t* data, size_t len);
    void sendCommand(uint16_t cmd, uint16_t value = 0, const uint8_t* extraData = nullptr, size_t extraLen = 0);
    bool waitForAck(uint16_t cmd, uint8_t* outPayload = nullptr, size_t* outLen = nullptr, uint32_t timeoutMs = 1000);
    
    // 协议常量
    static const uint32_t CMD_HEADER = 0xFAFBFCFD; // 小端: FD FC FB FA -> 0xFAFBFCFD
    static const uint32_t CMD_TAIL   = 0x01020304; // 小端: 04 03 02 01 -> 0x01020304
    static const uint32_t DATA_HEADER = 0xF1F2F3F4; // 小端: F4 F3 F2 F1 -> 0xF1F2F3F4
    static const uint32_t DATA_TAIL   = 0xF5F6F7F8; // 小端: F8 F7 F6 F5 -> 0xF5F6F7F8
};

}
