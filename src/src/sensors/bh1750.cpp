#include "bh1750.hpp"
#include "../system/i2c_manager.hpp"
#include <BH1750.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static BH1750 lightSensor(0x23);
static float last_lux = 0.0f;
static int last_error = -1; // 负值表示尚未有有效读数
static bool have_reading = false;

bool bh1750_init() {
    bool success = false;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            if (lightSensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire)) {
                Serial.println("[BH1750] Initialized");
                last_error = 0;
                success = true;
            } else {
                Serial.println("[BH1750] Init failed");
                last_error = -3; 
            }
            xSemaphoreGive(i2c_mutex);
        } else {
            Serial.println("[BH1750] Init failed: I2C timeout");
        }
    }
    return success;
}

void bh1750_read() {
    float lux = -1.0f;
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            lux = lightSensor.readLightLevel();
            xSemaphoreGive(i2c_mutex);
        } else {
            // Serial.println("[BH1750] Read timeout");
            return;
        }
    } else {
        lux = lightSensor.readLightLevel();
    }

    if (lux >= 0.0f) {
        last_lux = lux;
        last_error = 0;
        have_reading = true;
    } else {
        last_error = (int)lux;
        have_reading = false;
    }
}

bool bh1750_has_reading() {
    return have_reading;
}

float bh1750_get_lux() {
    return last_lux;
}

int bh1750_last_error() {
    return last_error;
}
