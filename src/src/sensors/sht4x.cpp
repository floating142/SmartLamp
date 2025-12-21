#include "sht4x.hpp"
#include <SensirionI2cSht4x.h>
#include "../system/i2c_manager.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static SensirionI2cSht4x sht;
static float last_temperature = 0.0f;
static float last_humidity = 0.0f;
static int16_t last_error = -1;
static bool have_reading = false;

bool sht4x_init() {
    bool success = false;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            sht.begin(Wire, SHT40_I2C_ADDR_44);
            success = true;
            xSemaphoreGive(i2c_mutex);
        } else {
            Serial.println("[SHT4x] Init failed: I2C timeout");
        }
    } else {
        sht.begin(Wire, SHT40_I2C_ADDR_44);
        success = true;
    }
    if (success) Serial.println("[SHT4x] Initialized");
    return success;
}

void sht4x_read() {
    float t = 0.0f, h = 0.0f;
    int16_t err = -1;
    
    // 保护 I2C 访问
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            err = sht.measureHighPrecision(t, h);
            xSemaphoreGive(i2c_mutex);
        } else {
            // Serial.println("[SHT4x] Read timeout");
            err = -100; 
        }
    } else {
        err = sht.measureHighPrecision(t, h);
    }

    if (err == 0) {
        last_temperature = t;
        last_humidity = h;
        have_reading = true;
        last_error = 0;
    } else {
        last_error = err;
        have_reading = false;
    }
}

bool sht4x_has_reading() {
    return have_reading;
}

float sht4x_get_temperature() {
    return last_temperature;
}

float sht4x_get_humidity() {
    return last_humidity;
}

int16_t sht4x_last_error() {
    return last_error;
}
