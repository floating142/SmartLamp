#include "cw2015.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../system/i2c_manager.hpp"

#define VCELL_LSB_UV 305u

// 全局实例
static CW2015 battery(Wire);

static float last_sent_soc = -1.0f;
static float last_stable_soc = -1.0f;
static bool is_charging = false;
static int last_packed_val = -1;
static bool last_packed_changed = false;

bool cw2015_init() {
    bool initialized = false;
    int retry_count = 0;

    // 初始化重试逻辑
    while (!initialized && retry_count < 3) {
        if (battery.begin()) {
            Serial.println("[CW2015] Initialized");
            battery.wakeUp();
            initialized = true;
        } else {
            Serial.println("[CW2015] Init failed, retrying...");
            vTaskDelay(pdMS_TO_TICKS(500));
            retry_count++;
        }
    }
    return initialized;
}

void cw2015_read() {
    uint16_t vcell = 0;
    float soc = 0;
    
    // 尝试读取
    if (battery.readVCell(vcell) && battery.readSOC(soc)) {
        
        // ---- 充电状态判定逻辑 ----
        // 1. 初始化 last_stable_soc
        if (last_stable_soc < 0) last_stable_soc = soc;

        // 2. 如果 SOC 增加，判定为充电
        if (soc > last_stable_soc + 0.2f) {
            is_charging = true;
            last_stable_soc = soc;
        }
        // 3. 如果 SOC 减少，判定为放电
        else if (soc < last_stable_soc - 0.2f) {
            is_charging = false;
            last_stable_soc = soc;
        }
        // 4. 特殊情况：满电且电压较高，视为充电(连接电源)中
        else if (soc >= 100.0f && vcell > 4150) {
            is_charging = true;
        }

        // 简单的变化阈值过滤 (1%) 或 状态改变
        // 注意：我们将充电状态编码到 value 中 (value > 100 表示充电)
        int ui_value = (int)soc + (is_charging ? 100 : 0);

        if (ui_value != last_packed_val) {
            last_packed_val = ui_value;
            last_sent_soc = soc;
            last_packed_changed = true;
        }
    } else {
        // 读取失败，可能是 I2C 错误
        // Serial.println("[CW2015] Read failed");
    }
}

CW2015::CW2015(TwoWire &wire) : _wire(wire) {}

bool CW2015::begin() {
    // 检查设备是否存在。使用互斥锁保护 Wire。
    bool present = false;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            _wire.beginTransmission(CW2015_I2C_ADDR);
            present = (_wire.endTransmission() == 0);
            xSemaphoreGive(i2c_mutex);
        } else {
            // 互斥锁超时 — 保守起见报告不存在
            present = false;
        }
    } else {
        _wire.beginTransmission(CW2015_I2C_ADDR);
        present = (_wire.endTransmission() == 0);
    }
    return present;
}

bool CW2015::readReg(uint8_t reg, uint8_t *buf, size_t len) {
    bool ok = false;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            _wire.beginTransmission(CW2015_I2C_ADDR);
            _wire.write(reg);
            if (_wire.endTransmission() == 0) {
                if (_wire.requestFrom(CW2015_I2C_ADDR, len) == (int)len) {
                    for (size_t i = 0; i < len; i++) {
                        buf[i] = _wire.read();
                    }
                    ok = true;
                }
            }
            xSemaphoreGive(i2c_mutex);
        }
    } else {
        _wire.beginTransmission(CW2015_I2C_ADDR);
        _wire.write(reg);
        if (_wire.endTransmission() == 0) {
            if (_wire.requestFrom(CW2015_I2C_ADDR, len) == (int)len) {
                for (size_t i = 0; i < len; i++) {
                    buf[i] = _wire.read();
                }
                ok = true;
            }
        }
    }
    return ok;
}

bool CW2015::writeReg(uint8_t reg, uint8_t val) {
    bool ok = false;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            _wire.beginTransmission(CW2015_I2C_ADDR);
            _wire.write(reg);
            _wire.write(val);
            ok = (_wire.endTransmission() == 0);
            xSemaphoreGive(i2c_mutex);
        }
    } else {
        _wire.beginTransmission(CW2015_I2C_ADDR);
        _wire.write(reg);
        _wire.write(val);
        ok = (_wire.endTransmission() == 0);
    }
    return ok;
}

bool CW2015::readVCell(uint16_t &vcell_mv) {
    uint8_t buf[2];
    if (!readReg(CW2015_REG_VCELL, buf, 2)) return false;

    uint16_t raw16 = ((uint16_t)buf[0] << 8) | buf[1];
    
    // Mask 14-bit (raw & 0x3FFF)
    uint32_t r14 = raw16 & 0x3FFFu;
    
    // Calculate microvolts
    uint32_t uv = r14 * (uint32_t)VCELL_LSB_UV;
    
    // Convert to millivolts (round)
    vcell_mv = (uint16_t)((uv + 500u) / 1000u);
    
    return true;
}

bool CW2015::readSOC(float &soc) {
    uint8_t buf[2];
    if (!readReg(CW2015_REG_SOC, buf, 2)) return false;
    
    // SOC is a 16-bit value where 1/256% is the unit
    // High byte is integer part, Low byte is fractional part
    // Actually datasheet says: SOC (High) . SOC (Low)
    // So value = High + Low/256.0
    
    soc = buf[0] + (float)buf[1] / 256.0f;
    
    // Clamp to 0-100
    if (soc > 100.0f) soc = 100.0f;
    
    return true;
}

bool CW2015::wakeUp() {
    // Write 0x00 to MODE register (0x0A) to wake up
    // Actually MODE register: bit 1-0 is mode. 00=Normal, 11=Sleep
    // We need to read-modify-write or just write 0x00 (default)
    return writeReg(CW2015_REG_MODE, 0x00); // Set to Normal Mode
}

bool CW2015::sleep() {
    // Write 0xC0 to MODE register (0x0A) to sleep
    // Bit 7-6: 11 = Sleep
    return writeReg(CW2015_REG_MODE, 0xC0); 
}

void CW2015::dumpRegisters() {
    uint8_t val;
    if (readReg(CW2015_REG_VERSION, &val, 1)) {
        Serial.printf("[CW2015] VERSION: 0x%02X\n", val);
    }
    
    uint8_t buf[2];
    if (readReg(CW2015_REG_VCELL, buf, 2)) {
        Serial.printf("[CW2015] VCELL RAW: 0x%02X 0x%02X\n", buf[0], buf[1]);
    }
    
    if (readReg(CW2015_REG_SOC, buf, 2)) {
        Serial.printf("[CW2015] SOC RAW: 0x%02X 0x%02X\n", buf[0], buf[1]);
    }
    
    if (readReg(CW2015_REG_MODE, buf, 1)) { // Mode is 1 byte
        Serial.printf("[CW2015] MODE: 0x%02X\n", buf[0]);
    }
    
    if (readReg(CW2015_REG_CONFIG, buf, 1)) { // Config is 1 byte
        Serial.printf("[CW2015] CONFIG: 0x%02X\n", buf[0]);
    }
}

// ---- 查询接口实现 ----
bool cw2015_has_reading() {
    return last_packed_val >= 0;
}

int cw2015_get_ui_value() {
    return last_packed_val;
}

// 如果有变化则返回值并清除变化标志，否则返回 -1
int cw2015_take_ui_value_if_changed() {
    if (!last_packed_changed) return -1;
    last_packed_changed = false;
    return last_packed_val;
}
