#ifndef SENSOR_MONITOR_H
#define SENSOR_MONITOR_H

#include <string>
#include <cJSON.h>
#include "axp2101.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct PowerSensorData {
    int battery_level = 0;       // %
    bool is_charging = false;
    bool is_discharging = false;
    float temperature_c = 0.0f;  // Nhiet do PMIC / Bo mach (°C)
};

struct NetworkSensorData {
    std::string ssid;
    int rssi_dbm = 0;
    int channel = 0;
    std::string ip_address;
};

struct SystemSensorData {
    uint32_t free_sram_bytes = 0;
    uint32_t free_psram_bytes = 0;
    uint32_t min_sram_bytes = 0;
    uint32_t uptime_seconds = 0;
    uint32_t cpu_freq_mhz = 240;
};

struct LightSensorData {
    bool available = false;
    float lux = 0.0f;           // Cuong do anh sang (Lux)
    uint16_t proximity = 0;     // Cam bien tiem can (11-bit raw)
};

struct MotionSensorData {
    bool available = false;
    float accel_x = 0.0f;       // Gia toc X (g)
    float accel_y = 0.0f;       // Gia toc Y (g)
    float accel_z = 0.0f;       // Gia toc Z (g)
    float gyro_x = 0.0f;        // Con quay hoi chuyen X (dps)
    float gyro_y = 0.0f;        // Con quay hoi chuyen Y (dps)
    float gyro_z = 0.0f;        // Con quay hoi chuyen Z (dps)
    float tilt_degrees = 0.0f;  // Goc nghieng so voi mat phang (0-90 do)
    std::string posture = "Chua co du lieu";
};

struct CoreS3SensorSnapshot {
    PowerSensorData power;
    NetworkSensorData network;
    SystemSensorData system;
    LightSensorData light;
    MotionSensorData motion;
};

class SensorMonitor {
public:
    static SensorMonitor& GetInstance();

    void Initialize(Axp2101* pmic, i2c_master_bus_handle_t i2c_bus = nullptr);
    CoreS3SensorSnapshot GetSnapshot();

    // Export JSON de phuc vu MCP Tools Grounding cho AI
    std::string GetAllSensorsJson();
    std::string GetSensorDataJson(const std::string& sensor_type);

private:
    SensorMonitor() = default;
    Axp2101* pmic_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t ltr553_dev_ = nullptr;
    i2c_master_dev_handle_t bmi270_dev_ = nullptr;
    bool ltr553_available_ = false;
    bool bmi270_available_ = false;
    bool initialized_ = false;
    // GetSnapshot() is called from the MCP thread, the app loop and the LVGL
    // timers; the shared I2C bus must only be driven by one of them at a time.
    SemaphoreHandle_t i2c_mutex_ = nullptr;

    bool InitLtr553();
    bool InitBmi270();
    bool ReadLtr553(LightSensorData& data);
    bool ReadBmi270(MotionSensorData& data);

    bool WriteRegs(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t* data, size_t len, int timeout_ms);
    bool ReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len, int timeout_ms);
};

#endif // SENSOR_MONITOR_H
