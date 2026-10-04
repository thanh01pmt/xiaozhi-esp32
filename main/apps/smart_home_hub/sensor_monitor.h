#ifndef SENSOR_MONITOR_H
#define SENSOR_MONITOR_H

#include <string>
#include <cJSON.h>
#include "axp2101.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct PowerSensorData {
    int battery_level = 0;       // %
    uint16_t vbat_mv = 0;        // mV
    uint16_t vbus_mv = 0;        // mV
    uint16_t vsys_mv = 0;        // mV
    bool vbus_present = false;
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
    float roll_deg = 0.0f;      // Goc Roll (-180..+180)
    float pitch_deg = 0.0f;     // Goc Pitch (-90..+90)
    float heading_deg = 0.0f;   // Huong la ban (0..360)
    float tilt_degrees = 0.0f;  // Goc nghieng so voi mat phang (0-90 do)
    std::string posture = "Chua co du lieu";
};

struct PortPeripheralData {
    bool pmic_ok = false;     // 0x34 AXP2101
    bool imu_ok = false;      // 0x69 BMI270
    bool light_ok = false;    // 0x23 LTR-553ALS
    bool touch_ok = false;    // 0x38 FT6336
    bool amp_ok = false;      // 0x36 AW88298
    bool mic_adc_ok = false;  // 0x40 ES7210
    bool io_exp_ok = false;   // 0x58 AW9523
    bool rtc_ok = false;      // 0x51 BM8563
    int total_online = 0;
};

struct CoreS3SensorSnapshot {
    PowerSensorData power;
    NetworkSensorData network;
    SystemSensorData system;
    LightSensorData light;
    MotionSensorData motion;
    PortPeripheralData peripherals;
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
    i2c_master_dev_handle_t axp2101_dev_ = nullptr;
    i2c_master_dev_handle_t ltr553_dev_ = nullptr;
    i2c_master_dev_handle_t bmi270_dev_ = nullptr;
    bool ltr553_available_ = false;
    bool bmi270_available_ = false;
    bool initialized_ = false;
    PortPeripheralData peripherals_ = {};
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
