#include "sensor_monitor.h"
#include "board.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <esp_netif.h>

#define TAG "SensorMonitor"

SensorMonitor& SensorMonitor::GetInstance() {
    static SensorMonitor instance;
    return instance;
}

void SensorMonitor::Initialize(Axp2101* pmic, i2c_master_bus_handle_t i2c_bus) {
    pmic_ = pmic;
    i2c_bus_ = i2c_bus;
    initialized_ = true;

    if (i2c_bus_ != nullptr) {
        InitLtr553();
        InitBmi270();
    }
    ESP_LOGI(TAG, "SensorMonitor initialized with PMIC, LTR553 and BMI270");
}

void SensorMonitor::InitLtr553() {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x23, // LTR-553ALS I2C Address
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &ltr553_dev_) == ESP_OK) {
        // Active mode cho ALS & PS
        uint8_t als_ctrl[] = {0x80, 0x01}; // ALS_CONTR: Active mode, Gain 1X
        i2c_master_transmit(ltr553_dev_, als_ctrl, sizeof(als_ctrl), 100);
        uint8_t ps_ctrl[] = {0x81, 0x03};  // PS_CONTR: Active mode
        i2c_master_transmit(ltr553_dev_, ps_ctrl, sizeof(ps_ctrl), 100);
        ESP_LOGI(TAG, "LTR-553ALS initialized successfully (0x23)");
    }
}

void SensorMonitor::InitBmi270() {
    uint8_t possible_addrs[] = {0x69, 0x68};
    for (uint8_t addr : possible_addrs) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &bmi270_dev_) == ESP_OK) {
            // BMI270 Power & Sensor Startup:
            // 1. Disable advanced power save: 0x7C (PWR_CONF) -> 0x00
            uint8_t pwr_conf[] = {0x7C, 0x00};
            i2c_master_transmit(bmi270_dev_, pwr_conf, sizeof(pwr_conf), 100);
            vTaskDelay(pdMS_TO_TICKS(10));

            // 2. Enable Accelerometer, Gyroscope & Temperature in 0x7D (PWR_CTRL) -> 0x0E (bit 1: aux, bit 2: gyr, bit 3: acc, bit 4: temp)
            uint8_t pwr_ctrl[] = {0x7D, 0x0E};
            esp_err_t err = i2c_master_transmit(bmi270_dev_, pwr_ctrl, sizeof(pwr_ctrl), 100);
            vTaskDelay(pdMS_TO_TICKS(10));

            // 3. Configure Accel (0x40): ODR 100Hz (0x08), normal mode
            uint8_t acc_conf[] = {0x40, 0xA8};
            i2c_master_transmit(bmi270_dev_, acc_conf, sizeof(acc_conf), 100);

            // 4. Configure Gyro (0x42): ODR 100Hz (0x08), normal mode
            uint8_t gyr_conf[] = {0x42, 0xA8};
            i2c_master_transmit(bmi270_dev_, gyr_conf, sizeof(gyr_conf), 100);

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "BMI270 IMU initialized successfully at 0x%02x", addr);
                return;
            } else {
                i2c_master_bus_rm_device(bmi270_dev_);
                bmi270_dev_ = nullptr;
            }
        }
    }
    ESP_LOGW(TAG, "BMI270 IMU not responding at 0x69 or 0x68");
}

void SensorMonitor::ReadLtr553(LightSensorData& data) {
    if (ltr553_dev_ == nullptr) return;

    // LTR-553ALS ALS registers:
    // 0x88: CH1 Low, 0x89: CH1 High (IR spectrum)
    // 0x8A: CH0 Low, 0x8B: CH0 High (Visible + IR spectrum)
    // Must read 0x88 -> 0x8B sequentially to trigger data latch
    uint8_t reg_als = 0x88;
    uint8_t als_buf[4] = {0};
    if (i2c_master_transmit_receive(ltr553_dev_, &reg_als, 1, als_buf, 4, 100) == ESP_OK) {
        data.available = true;
        uint16_t ch1 = als_buf[0] | (als_buf[1] << 8); // IR
        uint16_t ch0 = als_buf[2] | (als_buf[3] << 8); // Visible + IR

        // Standard Lite-On LTR-553ALS Lux Calculation:
        // Gain = 1X (als_gain = 1.0), Int Time = 100ms (als_int = 1.0)
        float als_gain = 1.0f;
        float als_int = 1.0f;
        float lux = 0.0f;

        if (ch0 == 0 && ch1 == 0) {
            lux = 0.0f;
        } else {
            float ratio = static_cast<float>(ch1) / static_cast<float>(ch0 + ch1);
            if (ratio < 0.45f) {
                lux = (1.7743f * ch0 + 1.1059f * ch1) / (als_gain * als_int);
            } else if (ratio < 0.64f) {
                lux = (4.2785f * ch0 - 1.9548f * ch1) / (als_gain * als_int);
            } else if (ratio < 0.85f) {
                lux = (0.5926f * ch0 + 0.1185f * ch1) / (als_gain * als_int);
            } else {
                lux = 0.0f;
            }
        }
        if (lux < 0.0f) lux = 0.0f;
        data.lux = lux;
    }

    // Doc PS Data (0x8D-0x8E)
    uint8_t reg_ps = 0x8D;
    uint8_t ps_buf[2] = {0};
    if (i2c_master_transmit_receive(ltr553_dev_, &reg_ps, 1, ps_buf, 2, 100) == ESP_OK) {
        data.proximity = (ps_buf[0] | ((ps_buf[1] & 0x07) << 8));
    }
}

void SensorMonitor::ReadBmi270(MotionSensorData& data) {
    if (bmi270_dev_ == nullptr) return;

    // Doc ACC_X_LSB (0x0C den 0x17 - 12 bytes cho Accel va Gyro)
    uint8_t reg = 0x0C;
    uint8_t buf[12] = {0};
    if (i2c_master_transmit_receive(bmi270_dev_, &reg, 1, buf, 12, 100) == ESP_OK) {
        data.available = true;
        int16_t raw_ax = static_cast<int16_t>(buf[0] | (buf[1] << 8));
        int16_t raw_ay = static_cast<int16_t>(buf[2] | (buf[3] << 8));
        int16_t raw_az = static_cast<int16_t>(buf[4] | (buf[5] << 8));
        int16_t raw_gx = static_cast<int16_t>(buf[6] | (buf[7] << 8));
        int16_t raw_gy = static_cast<int16_t>(buf[8] | (buf[9] << 8));
        int16_t raw_gz = static_cast<int16_t>(buf[10] | (buf[11] << 8));

        // Range default +-2g (16384 LSB/g)
        data.accel_x = raw_ax / 16384.0f;
        data.accel_y = raw_ay / 16384.0f;
        data.accel_z = raw_az / 16384.0f;

        // Range default +-2000 dps (16.4 LSB/dps)
        data.gyro_x = raw_gx / 16.4f;
        data.gyro_y = raw_gy / 16.4f;
        data.gyro_z = raw_gz / 16.4f;

        if (data.accel_z > 0.7f) {
            data.posture = "Nam phang tren ban";
        } else if (data.accel_y < -0.7f) {
            data.posture = "Dat dung (Upright)";
        } else if (data.accel_z < -0.7f) {
            data.posture = "Up mat xuong";
        } else {
            data.posture = "Dang cam tren tay / Nghieng";
        }
    }
}

CoreS3SensorSnapshot SensorMonitor::GetSnapshot() {
    CoreS3SensorSnapshot snapshot;

    // 1. Doc du lieu tu AXP2101 PMIC
    if (pmic_ != nullptr) {
        snapshot.power.battery_level = pmic_->GetBatteryLevel();
        snapshot.power.is_charging = pmic_->IsCharging();
        snapshot.power.is_discharging = pmic_->IsDischarging();
        snapshot.power.temperature_c = pmic_->GetTemperature();
    }

    // 2. Doc du lieu tu LTR-553ALS va BMI270
    ReadLtr553(snapshot.light);
    ReadBmi270(snapshot.motion);

    // Neu PMIC tra ve <= 0, doc hoac fallback tu BMI270 IMU (0x22, 0x23)
    if (snapshot.power.temperature_c <= 0.0f && bmi270_dev_ != nullptr) {
        uint8_t reg_temp = 0x22;
        uint8_t buf_temp[2] = {0};
        if (i2c_master_transmit_receive(bmi270_dev_, &reg_temp, 1, buf_temp, 2, 100) == ESP_OK) {
            int16_t raw = static_cast<int16_t>(buf_temp[0] | (buf_temp[1] << 8));
            if (raw != static_cast<int16_t>(0x8000)) {
                snapshot.power.temperature_c = (static_cast<float>(raw) / 512.0f) + 23.0f;
            }
        }
    }
    if (snapshot.power.temperature_c <= 0.0f) {
        snapshot.power.temperature_c = 36.5f; // Fallback muc danh dinh cua CoreS3
    }

    // 2. Doc du lieu Mang Wi-Fi
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        snapshot.network.ssid = reinterpret_cast<char*>(ap_info.ssid);
        snapshot.network.rssi_dbm = ap_info.rssi;
        snapshot.network.channel = ap_info.primary;
    } else {
        snapshot.network.ssid = "Disconnected";
        snapshot.network.rssi_dbm = -100;
        snapshot.network.channel = 0;
    }

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[32];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            snapshot.network.ip_address = ip_str;
        }
    }

    // 3. Doc du lieu He thong & Bo nho
    snapshot.system.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.system.min_sram_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.uptime_seconds = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
    snapshot.system.cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    return snapshot;
}

std::string SensorMonitor::GetAllSensorsJson() {
    CoreS3SensorSnapshot s = GetSnapshot();
    cJSON* root = cJSON_CreateObject();

    // Power & Battery
    cJSON* pwr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pwr, "battery_level_percent", s.power.battery_level);
    cJSON_AddBoolToObject(pwr, "is_charging", s.power.is_charging);
    cJSON_AddBoolToObject(pwr, "is_discharging", s.power.is_discharging);
    cJSON_AddNumberToObject(pwr, "board_temperature_celsius", s.power.temperature_c);
    cJSON_AddItemToObject(root, "power_and_battery", pwr);

    // Network
    cJSON* net = cJSON_CreateObject();
    cJSON_AddStringToObject(net, "ssid", s.network.ssid.c_str());
    cJSON_AddNumberToObject(net, "rssi_dbm", s.network.rssi_dbm);
    cJSON_AddNumberToObject(net, "channel", s.network.channel);
    cJSON_AddStringToObject(net, "ip_address", s.network.ip_address.c_str());
    cJSON_AddItemToObject(root, "network", net);

    // Ambient Light & Proximity (LTR-553ALS)
    cJSON* light = cJSON_CreateObject();
    cJSON_AddBoolToObject(light, "available", s.light.available);
    if (s.light.available) {
        cJSON_AddNumberToObject(light, "ambient_lux", s.light.lux);
        cJSON_AddNumberToObject(light, "proximity_raw", s.light.proximity);
    }
    cJSON_AddItemToObject(root, "ambient_light", light);

    // Motion & Orientation (BMI270 IMU)
    cJSON* motion = cJSON_CreateObject();
    cJSON_AddBoolToObject(motion, "available", s.motion.available);
    if (s.motion.available) {
        cJSON* accel = cJSON_CreateObject();
        cJSON_AddNumberToObject(accel, "x_g", s.motion.accel_x);
        cJSON_AddNumberToObject(accel, "y_g", s.motion.accel_y);
        cJSON_AddNumberToObject(accel, "z_g", s.motion.accel_z);
        cJSON_AddItemToObject(motion, "accelerometer", accel);

        cJSON* gyro = cJSON_CreateObject();
        cJSON_AddNumberToObject(gyro, "x_dps", s.motion.gyro_x);
        cJSON_AddNumberToObject(gyro, "y_dps", s.motion.gyro_y);
        cJSON_AddNumberToObject(gyro, "z_dps", s.motion.gyro_z);
        cJSON_AddItemToObject(motion, "gyroscope", gyro);

        cJSON_AddStringToObject(motion, "posture", s.motion.posture.c_str());
    }
    cJSON_AddItemToObject(root, "motion_imu", motion);

    // System Telemetry
    cJSON* sys = cJSON_CreateObject();
    cJSON_AddNumberToObject(sys, "free_sram_kb", s.system.free_sram_bytes / 1024);
    cJSON_AddNumberToObject(sys, "free_psram_mb", s.system.free_psram_bytes / (1024 * 1024));
    cJSON_AddNumberToObject(sys, "uptime_seconds", s.system.uptime_seconds);
    cJSON_AddNumberToObject(sys, "cpu_freq_mhz", s.system.cpu_freq_mhz);
    cJSON_AddItemToObject(root, "system", sys);

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "{}";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
}

std::string SensorMonitor::GetSensorDataJson(const std::string& sensor_type) {
    CoreS3SensorSnapshot s = GetSnapshot();
    cJSON* root = cJSON_CreateObject();

    if (sensor_type == "battery" || sensor_type == "power") {
        cJSON_AddNumberToObject(root, "battery_level_percent", s.power.battery_level);
        cJSON_AddBoolToObject(root, "is_charging", s.power.is_charging);
        cJSON_AddBoolToObject(root, "is_discharging", s.power.is_discharging);
        cJSON_AddStringToObject(root, "status", s.power.is_charging ? "Charging" : (s.power.battery_level > 20 ? "Normal" : "Low Battery"));
    } else if (sensor_type == "temperature") {
        cJSON_AddNumberToObject(root, "board_temperature_celsius", s.power.temperature_c);
        cJSON_AddStringToObject(root, "thermal_status", s.power.temperature_c < 45.0f ? "Cool/Normal" : "Warm");
    } else if (sensor_type == "light" || sensor_type == "lux" || sensor_type == "als") {
        cJSON_AddBoolToObject(root, "available", s.light.available);
        if (s.light.available) {
            cJSON_AddNumberToObject(root, "ambient_lux", s.light.lux);
            cJSON_AddNumberToObject(root, "proximity_raw", s.light.proximity);
            cJSON_AddStringToObject(root, "lighting_condition", s.light.lux < 50.0f ? "Dark/Dim" : (s.light.lux < 500.0f ? "Normal Indoor" : "Bright Indoor/Outdoor"));
        } else {
            cJSON_AddStringToObject(root, "status", "Sensor not detected or initializing");
        }
    } else if (sensor_type == "motion" || sensor_type == "imu" || sensor_type == "accel" || sensor_type == "gyro") {
        cJSON_AddBoolToObject(root, "available", s.motion.available);
        if (s.motion.available) {
            cJSON_AddNumberToObject(root, "accel_x", s.motion.accel_x);
            cJSON_AddNumberToObject(root, "accel_y", s.motion.accel_y);
            cJSON_AddNumberToObject(root, "accel_z", s.motion.accel_z);
            cJSON_AddNumberToObject(root, "gyro_x", s.motion.gyro_x);
            cJSON_AddNumberToObject(root, "gyro_y", s.motion.gyro_y);
            cJSON_AddNumberToObject(root, "gyro_z", s.motion.gyro_z);
            cJSON_AddStringToObject(root, "posture", s.motion.posture.c_str());
        } else {
            cJSON_AddStringToObject(root, "status", "Sensor not detected or initializing");
        }
    } else if (sensor_type == "network" || sensor_type == "wifi") {
        cJSON_AddStringToObject(root, "ssid", s.network.ssid.c_str());
        cJSON_AddNumberToObject(root, "rssi_dbm", s.network.rssi_dbm);
        cJSON_AddNumberToObject(root, "channel", s.network.channel);
        cJSON_AddStringToObject(root, "ip_address", s.network.ip_address.c_str());
        cJSON_AddStringToObject(root, "signal_quality", s.network.rssi_dbm > -60 ? "Excellent" : (s.network.rssi_dbm > -75 ? "Good" : "Fair"));
    } else if (sensor_type == "system" || sensor_type == "memory") {
        cJSON_AddNumberToObject(root, "free_sram_kb", s.system.free_sram_bytes / 1024);
        cJSON_AddNumberToObject(root, "free_psram_mb", s.system.free_psram_bytes / (1024 * 1024));
        cJSON_AddNumberToObject(root, "uptime_seconds", s.system.uptime_seconds);
        cJSON_AddNumberToObject(root, "cpu_freq_mhz", s.system.cpu_freq_mhz);
    } else {
        cJSON_AddStringToObject(root, "error", "Unknown sensor type. Available: battery, temperature, light, motion, network, system");
    }

    char* str = cJSON_PrintUnformatted(root);
    std::string res = str ? str : "{}";
    cJSON_free(str);
    cJSON_Delete(root);
    return res;
}
