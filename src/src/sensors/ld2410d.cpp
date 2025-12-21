#include "ld2410d.hpp"

namespace Sensor {

LD2410D::LD2410D(Stream& stream) : _stream(stream) {
    // 初始化数据
    _data.state = RadarState::NO_TARGET;
    _data.distance_cm = 0;
    memset(_data.gate_energy, 0, sizeof(_data.gate_energy));
}

void LD2410D::begin() {
    // 可以在这里做一些初始化工作，比如清空缓冲区
    while (_stream.available()) {
        _stream.read();
    }
}

void LD2410D::setDebugStream(Stream* debugStream) {
    _debugStream = debugStream;
}

void LD2410D::update() {
    while (_stream.available()) {
        uint8_t byte = _stream.read();
        if (_debugStream) {
            _debugStream->printf("RX: %02X\n", byte);
        }
        processByte(byte);
    }
}

void LD2410D::processByte(uint8_t byte) {
    // 环形缓冲区写入
    _buffer[_bufIndex] = byte;
    _bufIndex++;
    if (_bufIndex >= BUFFER_SIZE) {
        _bufIndex = 0;
        _bufferFilled = true;
    }

    // 获取当前有效数据长度
    size_t validLen = _bufferFilled ? BUFFER_SIZE : _bufIndex;
    
    // 至少需要 11 字节 (Head 4 + Len 2 + Data 1 + Tail 4)
    if (validLen < 11) return;

    // 辅助 lambda: 获取相对于当前写入位置偏移 offset 的字节
    // offset = 0 表示刚刚写入的那个字节 (byte)
    auto getByte = [&](size_t offset) -> uint8_t {
        // 计算索引: (_bufIndex - 1 - offset)
        // 注意处理负数回绕
        // 因为 _bufIndex 已经自增了，所以当前最新字节在 _bufIndex - 1
        size_t idx = (_bufIndex - 1 - offset + BUFFER_SIZE) % BUFFER_SIZE;
        return _buffer[idx];
    };

    // 1. 检查帧尾 (F8 F7 F6 F5) -> 0xF5F6F7F8 (Little Endian)
    // 对应 getByte(0)..getByte(3)
    if (getByte(0) == 0xF5 && getByte(1) == 0xF6 && 
        getByte(2) == 0xF7 && getByte(3) == 0xF8) {
        
        // 2. 向前寻找帧头 (F4 F3 F2 F1)
        // 帧头至少在 10 字节之前 (Tail 4 + Len 2 + Head 4) -> offset 10
        // 最大搜索范围: validLen
        
        for (size_t i = 10; i < validLen; i++) {
            // 检查帧头: F4 F3 F2 F1
            // getByte(i) 是 F1, getByte(i+1) 是 F2...
            if (getByte(i) == 0xF1 && getByte(i+1) == 0xF2 && 
                getByte(i+2) == 0xF3 && getByte(i+3) == 0xF4) {
                
                // 找到帧头，位置在 offset i+3
                // 包总长度 = i + 4 (从 offset 0 到 offset i+3)
                size_t packetLen = i + 4;
                
                // 3. 验证长度字段
                // Len 字段在 Header 之后，即 offset i-1 (LSB) 和 i-2 (MSB)
                uint16_t dataLen = getByte(i-1) | (getByte(i-2) << 8);
                
                // 预期包总长度 = 4(Head) + 2(Len) + dataLen + 4(Tail)
                if (packetLen == (4 + 2 + dataLen + 4)) {
                    // 4. 提取完整包到临时缓冲区进行解析
                    // 避免在 parsePacket 里处理环形逻辑
                    if (packetLen <= BUFFER_SIZE) {
                        uint8_t tempBuf[256]; // 栈上分配，最大 256
                        if (packetLen > 256) return; // 异常保护

                        for (size_t k = 0; k < packetLen; k++) {
                            // 复制顺序: 从 Header 开始
                            // Header 在 offset i+3
                            // 第 0 个字节 -> getByte(i + 3 - 0)
                            // 第 k 个字节 -> getByte(i + 3 - k)
                            tempBuf[k] = getByte(i + 3 - k);
                        }
                        
                        parsePacket(tempBuf, packetLen);
                        return; // 处理完毕
                    }
                }
            }
        }
    }
}

void LD2410D::parsePacket(const uint8_t* data, size_t len) {
    // data 指向帧头 F4...
    // 格式: Head(4) + Len(2) + State(1) + Dist(2) + Energy(128) + Tail(4)
    // 偏移:
    // State: 6
    // Dist: 7
    // Energy: 9
    
    if (len < 10) return; // 保护

    uint8_t stateVal = data[6];
    if (stateVal == 0x00) _data.state = RadarState::NO_TARGET;
    else if (stateVal == 0x01) _data.state = RadarState::MOVING;
    else if (stateVal == 0x02) _data.state = RadarState::STATIONARY;
    else _data.state = RadarState::NO_TARGET; // 未知

    _data.distance_cm = data[7] | (data[8] << 8);

    // 解析能量值
    // 能量值起始偏移: 9
    // 长度: 128字节 (32 * 4)
    // 检查剩余长度是否足够
    // Head(4)+Len(2)+State(1)+Dist(2) = 9 bytes header
    // Tail(4)
    // Total overhead = 13 bytes.
    // Payload len should be len - 10.
    // dataLen field at data[4] should be checked.
    
    uint16_t payloadLen = data[4] | (data[5] << 8);
    // Payload contains: State(1) + Dist(2) + Energy(N)
    if (payloadLen >= 3 + 128) {
        const uint8_t* energyPtr = &data[9];
        for (int i = 0; i < 32; i++) {
            // 4字节整数，小端
            _data.gate_energy[i] = energyPtr[0] | (energyPtr[1] << 8) | (energyPtr[2] << 16) | (energyPtr[3] << 24);
            energyPtr += 4;
        }
    }
}

bool LD2410D::enableConfiguration() {
    // 命令字: 0x00FF, 值: 0x0001
    // FD FC FB FA 04 00 FF 00 01 00 04 03 02 01
    uint16_t val = 0x0001;
    sendCommand(0x00FF, 0, (uint8_t*)&val, 2);
    return waitForAck(0x00FF);
}

bool LD2410D::endConfiguration() {
    // 命令字: 0x00FE
    sendCommand(0x00FE);
    return waitForAck(0x00FE);
}

String LD2410D::readFirmwareVersion() {
    // 命令字: 0x0000
    sendCommand(0x0000);
    
    uint8_t payload[32];
    size_t len = sizeof(payload);
    if (waitForAck(0x0000, payload, &len)) {
        // Payload: Len(2) + VersionString(N)
        if (len > 2) {
            uint16_t verLen = payload[0] | (payload[1] << 8);
            if (verLen > 0 && verLen < len) {
                char verStr[32];
                memcpy(verStr, &payload[2], verLen);
                verStr[verLen] = 0; // Null terminate
                // 这里的版本号可能是 hex 或者是 string，文档示例是 hex 转换 string
                // 示例: 76 34 2E 33 2E 30 -> "v4.3.0" (ASCII)
                // 直接返回字符串即可
                return String(verStr);
            }
        }
    }
    return String("");
}

bool LD2410D::setEngineeringMode(bool enable) {
    // 命令字: 0x0012
    // 命令值: 0x0000
    // 参数值: 0x00000004 (工程模式) / 0x00000064 (正常模式)
    // 注意：文档中参数值是4字节
    
    uint8_t params[6]; // 2字节命令值 + 4字节参数值
    params[0] = 0x00; params[1] = 0x00; // 命令值 0x0000
    
    uint32_t modeVal = enable ? 0x00000004 : 0x00000064;
    memcpy(&params[2], &modeVal, 4);
    
    sendCommand(0x0012, 0, params, 6);
    return waitForAck(0x0012);
}

String LD2410D::readSerialNumber() {
    // 命令字: 0x0011 (字符形式)
    sendCommand(0x0011);
    
    uint8_t payload[32];
    size_t len = sizeof(payload);
    if (waitForAck(0x0011, payload, &len)) {
        // Payload: SN Len(2) + SN(N)
        if (len > 2) {
            uint16_t snLen = payload[0] | (payload[1] << 8);
            if (snLen > 0 && snLen < len) {
                char snStr[32];
                memcpy(snStr, &payload[2], snLen);
                snStr[snLen] = 0;
                return String(snStr);
            }
        }
    }
    return String("");
}

bool LD2410D::readBasicParameters(uint8_t& maxDistVal, uint16_t& duration) {
    // 命令字: 0x0008
    // 命令值: (2字节参数ID) * N
    // 读取最大距离(0x0001) 和 延迟(0x0004)
    uint8_t req[] = {0x01, 0x00, 0x04, 0x00}; // ID 1, ID 4
    sendCommand(0x0008, 0, req, 4);
    
    uint8_t payload[32];
    size_t len = sizeof(payload);
    if (waitForAck(0x0008, payload, &len)) {
        // 返回值: (4字节参数值) * N
        // 顺序对应请求的 ID
        if (len >= 8) {
            // Param 1 (Max Dist)
            uint32_t val1 = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
            maxDistVal = (uint8_t)val1;
            
            // Param 2 (Duration)
            uint32_t val2 = payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24);
            duration = (uint16_t)val2;
            return true;
        }
    }
    return false;
}

bool LD2410D::setBasicParameters(uint8_t maxDistVal, uint16_t duration) {
    // 命令字: 0x0007
    // 命令值: (2字节参数ID + 4字节参数值) * N
    
    uint8_t data[12];
    // Param 1: Max Dist (ID 0x0001)
    data[0] = 0x01; data[1] = 0x00;
    uint32_t v1 = maxDistVal;
    memcpy(&data[2], &v1, 4);
    
    // Param 2: Duration (ID 0x0004)
    data[6] = 0x04; data[7] = 0x00;
    uint32_t v2 = duration;
    memcpy(&data[8], &v2, 4);
    
    sendCommand(0x0007, 0, data, 12);
    return waitForAck(0x0007);
}

bool LD2410D::setGateSensitivity(uint8_t gate, uint8_t motionThreshold, uint8_t staticThreshold) {
    if (gate > 15) return false; // 仅支持 0-15 距离门配置

    // 命令字: 0x0007
    // 运动门限 ID: 0x0010 + gate
    // 微动门限 ID: 0x0030 + gate
    
    uint8_t data[12];
    
    // 1. Motion Threshold
    uint16_t motionId = 0x0010 + gate;
    data[0] = motionId & 0xFF; 
    data[1] = (motionId >> 8) & 0xFF;
    uint32_t v1 = motionThreshold;
    memcpy(&data[2], &v1, 4); // 4字节值

    // 2. Static Threshold
    uint16_t staticId = 0x0030 + gate;
    data[6] = staticId & 0xFF;
    data[7] = (staticId >> 8) & 0xFF;
    uint32_t v2 = staticThreshold;
    memcpy(&data[8], &v2, 4); // 4字节值

    sendCommand(0x0007, 0, data, 12);
    return waitForAck(0x0007);
}

bool LD2410D::saveConfiguration() {
    // 命令字: 0x00FD
    sendCommand(0x00FD);
    return waitForAck(0x00FD);
}

bool LD2410D::startGainCalibration() {
    // 命令字: 0x00EE
    sendCommand(0x00EE);
    // 注意：这个命令可能需要较长时间，或者有后续的 0xF000 回复
    // 这里只等待 ACK (EE 01)
    return waitForAck(0x00EE);
}

bool LD2410D::restart() {
    // 文档 5.2.3 结束配置命令 (恢复工作模式)
    // 文档 5.4 上电自动增益调节 (0x00EE) ?
    // 通常 LD2410 有重启指令 0x00A3 ? 文档没写。
    // 这里暂时只实现 "结束配置" 作为恢复手段，或者使用 0x00A3 尝试一下 (通用 LD2410 协议)
    // 根据提供的文档，没有明确的软重启指令。
    // 但 "结束配置" 会让雷达恢复工作。
    return endConfiguration();
}

void LD2410D::sendCommand(uint16_t cmd, uint16_t value, const uint8_t* extraData, size_t extraLen) {
    // 构造帧
    // Head(4) + Len(2) + Cmd(2) + [Value(N) or ExtraData] + Tail(4)
    // 注意：sendCommand 参数里的 value 只是为了兼容某些简单命令，
    // 对于复杂命令，数据都在 extraData 里。
    // 根据文档：
    // 帧内数据 = 命令字(2) + 命令值(N)
    // 帧内数据长度 = 2 + N
    
    uint16_t dataLen = 2 + extraLen;
    
    uint8_t frame[64];
    int idx = 0;
    
    // Head: FD FC FB FA
    frame[idx++] = 0xFD; frame[idx++] = 0xFC; frame[idx++] = 0xFB; frame[idx++] = 0xFA;
    
    // Len
    frame[idx++] = dataLen & 0xFF;
    frame[idx++] = (dataLen >> 8) & 0xFF;
    
    // Cmd
    frame[idx++] = cmd & 0xFF;
    frame[idx++] = (cmd >> 8) & 0xFF;
    
    // Value / Extra Data
    if (extraData && extraLen > 0) {
        memcpy(&frame[idx], extraData, extraLen);
        idx += extraLen;
    }
    
    // Tail: 04 03 02 01
    frame[idx++] = 0x04; frame[idx++] = 0x03; frame[idx++] = 0x02; frame[idx++] = 0x01;
    
    if (_debugStream) {
        _debugStream->print("TX: ");
        for(int i=0; i<idx; i++) _debugStream->printf("%02X ", frame[i]);
        _debugStream->println();
    }

    _stream.write(frame, idx);
}

bool LD2410D::waitForAck(uint16_t cmd, uint8_t* outPayload, size_t* outLen, uint32_t timeoutMs) {
    uint32_t start = millis();
    uint8_t buf[128];
    int bufIdx = 0;
    
    while (millis() - start < timeoutMs) {
        if (_stream.available()) {
            uint8_t c = _stream.read();
            if (_debugStream) _debugStream->printf("RX_ACK: %02X\n", c);

            buf[bufIdx++] = c;
            if (bufIdx >= 128) bufIdx = 0; // Overflow protection
            
            // Check for Tail: 04 03 02 01
            if (bufIdx >= 4 && 
                buf[bufIdx-1] == 0x01 && buf[bufIdx-2] == 0x02 && 
                buf[bufIdx-3] == 0x03 && buf[bufIdx-4] == 0x04) {
                
                // Found tail, look for head FD FC FB FA
                for (int i = 0; i < bufIdx - 8; i++) {
                    if (buf[i] == 0xFD && buf[i+1] == 0xFC && 
                        buf[i+2] == 0xFB && buf[i+3] == 0xFA) {
                        
                        // Found head
                        // Parse Len
                        uint16_t len = buf[i+4] | (buf[i+5] << 8);
                        // Check Cmd
                        uint16_t ackCmd = buf[i+6] | (buf[i+7] << 8);
                        
                        // 协议特性：ACK命令字通常是 发送命令字 | 0x0100
                        // 例如发送 0x00FF, 回复 0x01FF
                        if (ackCmd == (cmd | 0x0100)) {
                            // Check Status (next 2 bytes)
                            // ACK Frame Data: Cmd(2) + Status(2) + ReturnValue(N)
                            uint16_t status = buf[i+8] | (buf[i+9] << 8);
                            
                            if (status == 0x0000) { // Success
                                if (outPayload && outLen) {
                                    // Copy return value
                                    // Return value len = len - 2(Cmd) - 2(Status)
                                    size_t retLen = len - 4;
                                    if (retLen > *outLen) retLen = *outLen;
                                    memcpy(outPayload, &buf[i+10], retLen);
                                    *outLen = retLen;
                                }
                                return true;
                            } else {
                                if (_debugStream) _debugStream->printf("ACK Failed Status: %04X\n", status);
                                return false; // Command failed
                            }
                        } else {
                             if (_debugStream) _debugStream->printf("ACK Cmd Mismatch: Expected %04X, Got %04X\n", (cmd | 0x0100), ackCmd);
                        }
                    }
                }
                // Reset buffer if packet processed or invalid
                bufIdx = 0;
            }
        }
    }
    return false;
}

const RadarData& LD2410D::getData() const {
    return _data;
}

bool LD2410D::hasTarget() const {
    return _data.state != RadarState::NO_TARGET;
}

void LD2410D::printDebugInfo(Stream& debugSerial) {
    debugSerial.printf("State: %d, Dist: %d cm\n", (int)_data.state, _data.distance_cm);
    debugSerial.print("Energy: [");
    for(int i=0; i<5; i++) { // Print first 5 gates for brevity
        debugSerial.printf("%u, ", _data.gate_energy[i]);
    }
    debugSerial.println("...]");
}

}
