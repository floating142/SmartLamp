#include "i2c_manager.hpp"

// Define the mutex
SemaphoreHandle_t i2c_mutex = NULL;

void setup_i2c_manager() {
    if (i2c_mutex == NULL) {
        i2c_mutex = xSemaphoreCreateMutex();
        if (i2c_mutex == NULL) {
            // Failed to create mutex
            Serial.println("[I2C] Failed to create I2C mutex");
        }
    }
}
