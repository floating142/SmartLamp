#include "rtc_task.hpp"
#include <Wire.h>
#include <DS3231.h>
#include "i2c_manager.hpp"

// We'll hold pointer to instance created after Wire.begin()
static DS3231* prtc = nullptr;

/**
 * @brief 从 RTC 读取时间并设置到系统时间 (用于启动时)
 */
static void sync_system_from_rtc() {
    if (!prtc) return;
    
    time_t rtc_sec = 0;
    bool rtc_ok = false;

    // 读取 RTC
    if (i2c_mutex) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
            DateTime rtc_now = RTClib::now(Wire);
            rtc_sec = rtc_now.unixtime();
            rtc_ok = true;
            xSemaphoreGive(i2c_mutex);
        } else {
            Serial.println("[RTC] I2C mutex timeout (boot sync)");
        }
    } else {
        DateTime rtc_now = RTClib::now(Wire);
        rtc_sec = rtc_now.unixtime();
        rtc_ok = true;
    }

    if (rtc_ok) {
        // 简单校验：如果 RTC 时间大于 2020 年，认为有效
        // 2020-01-01 00:00:00 UTC = 1577836800
        if (rtc_sec > 1577836800) {
            struct timeval tv;
            tv.tv_sec = rtc_sec;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            
            struct tm* tm_info = localtime(&rtc_sec);
            Serial.printf("[RTC] System time synced from RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                          tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                          tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            Serial.println("[RTC] RTC time invalid (pre-2020), skipping sync.");
        }
    }
}

static void rtc_task(void* pvParameters) {
    (void)pvParameters;
    
    // Sync interval: 10 minutes
    const TickType_t syncInterval = pdMS_TO_TICKS(10 * 60 * 1000);
    TickType_t lastSyncTime = 0;

    for (;;) {
        // 1. Get System Time
        time_t now_sec;
        time(&now_sec);
        struct tm timeinfo;
        localtime_r(&now_sec, &timeinfo);

        // Debug output (optional)
        // Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
        //               timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        //               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        // 2. Sync System Time -> RTC (if System Time is valid/NTP synced)
        // We assume if year > 2020, it's valid.
        if (timeinfo.tm_year + 1900 > 2020) {
            TickType_t currentTick = xTaskGetTickCount();
            // Sync if interval elapsed OR this is the first valid time seen (lastSyncTime == 0)
            if (currentTick - lastSyncTime > syncInterval || lastSyncTime == 0) {
                if (prtc) {
                    // Check difference before writing to avoid resetting DS3231 internal divider unnecessarily
                    // DS3231 time registers are SRAM (unlimited writes), but writing 'Seconds' resets the 1Hz chain.
                    time_t rtc_sec = 0;
                    bool rtc_ok = false;
                    if (i2c_mutex) {
                        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
                            DateTime rtc_now = RTClib::now(Wire);
                            rtc_sec = rtc_now.unixtime();
                            rtc_ok = true;
                            xSemaphoreGive(i2c_mutex);
                        } else {
                            Serial.println("[RTC] I2C mutex timeout while reading RTC");
                        }
                    } else {
                        DateTime rtc_now = RTClib::now(Wire);
                        rtc_sec = rtc_now.unixtime();
                        rtc_ok = true;
                    }

                    if (rtc_ok) {
                        long diff = (long)now_sec - (long)rtc_sec;
                        if (abs(diff) > 1) {
                            // setEpoch expects UTC timestamp
                            if (i2c_mutex) {
                                if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
                                    prtc->setEpoch(now_sec);
                                    xSemaphoreGive(i2c_mutex);
                                } else {
                                    Serial.println("[RTC] I2C mutex timeout while writing RTC");
                                }
                            } else {
                                prtc->setEpoch(now_sec);
                            }
                            Serial.printf("[RTC] Correcting RTC drift. Diff: %lds. Synced from NTP.\n", diff);
                        } else {
                            Serial.println("[RTC] RTC is accurate (diff <= 1s). Skipping write.");
                        }
                    }
                }
                lastSyncTime = currentTick;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_rtc_task() {
    // Create DS3231 instance bound to Wire
    static DS3231 rtcInstance(Wire);
    prtc = &rtcInstance;

    // Optional: check oscillator stop flag (protect with I2C mutex)
    if (prtc) {
        bool osc_ok = true;
        if (i2c_mutex) {
            if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
                osc_ok = prtc->oscillatorCheck();
                xSemaphoreGive(i2c_mutex);
            } else {
                Serial.println("[RTC] I2C mutex timeout while checking oscillator");
            }
        } else {
            osc_ok = prtc->oscillatorCheck();
        }
        if (!osc_ok) {
            Serial.println("Warning: RTC oscillator stop flag is set (clock may be invalid)");
        }
    }

    // --- Sync RTC -> System Time on Boot ---
    if (prtc) {
        DateTime now;
        bool got = false;
        if (i2c_mutex) {
            if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500))) {
                now = RTClib::now(Wire);
                xSemaphoreGive(i2c_mutex);
                got = true;
            } else {
                Serial.println("[RTC] I2C mutex timeout while reading RTC for boot sync");
            }
        } else {
            now = RTClib::now(Wire);
            got = true;
        }

        // Basic check: if RTC year is reasonable (>2020), trust it
        if (got && now.year() > 2020) {
            struct timeval tv;
            tv.tv_sec = now.unixtime();
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            
            // Set timezone to CST-8 (GMT+8) so localtime() works correctly immediately
            // Format: STD offset (CST-8 means UTC+8)
            setenv("TZ", "CST-8", 1);
            tzset();
            
            Serial.printf("[RTC] System time initialized from RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                          now.year(), now.month(), now.day(),
                          now.hour(), now.minute(), now.second());
        } else {
            Serial.println("[RTC] RTC time invalid (year <= 2020), skipping system time set.");
        }
    }

    // Start FreeRTOS task to periodically read and print time
    xTaskCreate(
        rtc_task,
        "RTC Task",
        2048, // 优化内存：4096 -> 2048
        NULL,
        tskIDLE_PRIORITY + 1, // 低优先级
        NULL
    );
}
