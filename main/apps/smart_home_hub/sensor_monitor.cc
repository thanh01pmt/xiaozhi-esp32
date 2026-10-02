#include "sensor_monitor.h"
#include "board.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_clk.h>
// bmi270_api.h exposes the Bosch 8 KB configuration blob (bmi270_config_file)
// that every BMI270 needs after power-up, plus the register map.
#include <bmi270_api.h>
#include <cmath>
#include <cstring>

#define TAG "SensorMonitor"

// ---------------------------------------------------------------------------
// M5Stack CoreS3 on-board sensors (I2C port 1, SDA=GPIO12, SCL=GPIO11)
//   LTR-553ALS-WA  ambient light + proximity, addr 0x23
//   BMI270         6-axis IMU, addr 0x69 (SDO tied high on CoreS3)
// ---------------------------------------------------------------------------

namespace {

// --- LTR-553ALS-WA (Lite-On) -------------------------------------------------
constexpr uint8_t kLtr553Addr = 0x23;
constexpr uint8_t kLtrRegAlsContr = 0x80;  // bit0 active, bit1 sw_reset, bits[4:2] gain
constexpr uint8_t kLtrRegPsContr = 0x81;   // bit0 mode_b, bit1 active
constexpr uint8_t kLtrRegMeasRate = 0x85;  // bits[5:3] repeat rate, bits[2:0] integration time
constexpr uint8_t kLtrRegStatus = 0x8C;    // bit2 als_new_data, bit7 invalid, bits[6:4] gain
constexpr uint8_t kLtrRegCh1L = 0x88;      // CH1 = infrared only
constexpr uint8_t kLtrRegPsL = 0x8D;       // proximity data (11 bit)
constexpr float kLtrGainCount[8] = {1.0f, 2.0f, 4.0f, 8.0f, 0.0f, 0.0f, 48.0f, 96.0f};
constexpr uint8_t kLtrIntTimeMs[8] = {100, 50, 200, 400, 150, 250, 300, 350};

// --- BMI270 (Bosch) ----------------------------------------------------------
constexpr uint8_t kBmi270Addr = 0x69;
constexpr uint8_t kBmi270ChipId = 0x24;
constexpr uint8_t kBmiRegAccConf = BMI2_ACC_CONF_ADDR;   // 0x40
constexpr uint8_t kBmiRegGyrConf = BMI2_GYR_CONF_ADDR;   // 0x42
constexpr uint8_t kBmiRegDataStart = 0x0C;               // ACC_X_LSB .. GYR_Z_MSB (12 bytes)
constexpr size_t kBmiConfigFileSize = 8192;
constexpr size_t kBmiChunkSize = 128;
// ACC_CONF / GYR_CONF = range(0b00) << 6 | bwp(0b10) << 4 | odr(0x08)
//   accel  -> +/-2 g     -> 16384 LSB/g
//   gyroscope -> +/-2000 dps -> 16.4 LSB/dps
constexpr uint8_t kBmiAccGyrConf = 0x28;
constexpr float kBmiAccLsbPerG = 16384.0f;
constexpr float kBmiGyrLsbPerDps = 16.4f;

}  // namespace

SensorMonitor& SensorMonitor::GetInstance() {
    static SensorMonitor instance;
    return instance;
}

bool SensorMonitor::WriteRegs(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t* data,
                              size_t len, int timeout_ms) {
    // Single transaction: send the register address followed by the payload.
    uint8_t buf[160];
    if (len + 1 > sizeof(buf)) return false;
    buf[0] = reg;
    memcpy(buf + 1, data, len);
    return i2c_master_transmit(dev, buf, len + 1, timeout_ms) == ESP_OK;
}

bool SensorMonitor::ReadRegs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len,
                             int timeout_ms) {
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, timeout_ms) == ESP_OK;
}

void SensorMonitor::Initialize(Axp2101* pmic, i2c_master_bus_handle_t i2c_bus) {
    pmic_ = pmic;
    i2c_bus_ = i2c_bus;
    initialized_ = true;
    i2c_mutex_ = xSemaphoreCreateMutex();

    if (i2c_bus_ != nullptr) {
        ltr553_available_ = InitLtr553();
        bmi270_available_ = InitBmi270();
    }
    ESP_LOGI(TAG, "SensorMonitor ready: PMIC=%s LTR-553ALS=%s BMI270=%s",
             pmic_ != nullptr ? "yes" : "no", ltr553_available_ ? "yes" : "no",
             bmi270_available_ ? "yes" : "no");
}

bool SensorMonitor::InitLtr553() {
    // i2c_master_bus_add_device() succeeds even for an address that never ACKs,
    // so probe first - otherwise a missing sensor looks initialised.
    if (i2c_master_probe(i2c_bus_, kLtr553Addr, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "LTR-553ALS not found at 0x%02X", kLtr553Addr);
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kLtr553Addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &ltr553_dev_) != ESP_OK) {
        ltr553_dev_ = nullptr;
        return false;
    }

    // Datasheet: the part needs ~100 ms after power-on before it answers I2C.
    vTaskDelay(pdMS_TO_TICKS(150));

    uint8_t contr = 0x02;  // software reset
    WriteRegs(ltr553_dev_, kLtrRegAlsContr, &contr, 1, 100);
    vTaskDelay(pdMS_TO_TICKS(20));

    contr = 0x01;  // active mode, gain 1x
    WriteRegs(ltr553_dev_, kLtrRegAlsContr, &contr, 1, 100);

    // repeat rate 200 ms, integration time 100 ms: fast enough that the 1 Hz
    // dashboard always reads a freshly measured sample.
    uint8_t meas_rate = (2 << 3) | 0;
    WriteRegs(ltr553_dev_, kLtrRegMeasRate, &meas_rate, 1, 100);

    uint8_t ps_contr = 0x03;  // proximity active
    WriteRegs(ltr553_dev_, kLtrRegPsContr, &ps_contr, 1, 100);

    ESP_LOGI(TAG, "LTR-553ALS ready at 0x%02X", kLtr553Addr);
    return true;
}

bool SensorMonitor::ReadLtr553(LightSensorData& data) {
    if (ltr553_dev_ == nullptr) return false;
    data.available = true;

    uint8_t status = 0;
    if (!ReadRegs(ltr553_dev_, kLtrRegStatus, &status, 1, 100)) {
        return false;
    }
    // bit2: new sample available, bit7: sample invalid (saturated / blacked out)
    if ((status & 0x04) == 0 || (status & 0x80) != 0) {
        return false;
    }

    uint8_t buf[4] = {0};
    if (!ReadRegs(ltr553_dev_, kLtrRegCh1L, buf, sizeof(buf), 100)) {
        return false;
    }
    uint16_t ch1 = buf[0] | (buf[1] << 8);  // infrared only
    uint16_t ch0 = buf[2] | (buf[3] << 8);  // visible + infrared

    if (ch0 == 0xFFFF || ch1 == 0xFFFF || (ch0 == 0 && ch1 == 0)) {
        data.lux = 0.0f;
        return false;
    }

    const float gain = kLtrGainCount[(status >> 4) & 0x07];
    const float int_time = kLtrIntTimeMs[0] / 100.0f;  // integration time programmed above
    const float ratio = static_cast<float>(ch1) / static_cast<float>(ch0 + ch1);
    float lux = 0.0f;
    if (ratio < 0.45f) {
        lux = 1.7743f * ch0 + 1.1059f * ch1;
    } else if (ratio < 0.64f) {
        lux = 4.2785f * ch0 - 1.9548f * ch1;
    } else if (ratio < 0.85f) {
        lux = 0.5926f * ch0 + 0.1185f * ch1;
    }
    lux /= (gain * int_time);
    data.lux = lux > 0.0f ? lux : 0.0f;

    uint8_t ps[2] = {0};
    if (ReadRegs(ltr553_dev_, kLtrRegPsL, ps, sizeof(ps), 100)) {
        data.proximity = ps[0] | ((ps[1] & 0x07) << 8);
    }
    return true;
}

bool SensorMonitor::InitBmi270() {
    if (i2c_master_probe(i2c_bus_, kBmi270Addr, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGW(TAG, "BMI270 not found at 0x%02X", kBmi270Addr);
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kBmi270Addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &bmi270_dev_) != ESP_OK) {
        bmi270_dev_ = nullptr;
        return false;
    }

    auto reg_write = [this](uint8_t reg, uint8_t value) {
        return WriteRegs(bmi270_dev_, reg, &value, 1, 100);
    };
    auto reg_read = [this](uint8_t reg, uint8_t* value) {
        return ReadRegs(bmi270_dev_, reg, value, 1, 100);
    };

    uint8_t chip_id = 0;
    if (!reg_read(BMI2_CHIP_ID_ADDR, &chip_id) || chip_id != kBmi270ChipId) {
        ESP_LOGW(TAG, "BMI270 chip id mismatch at 0x%02X (got 0x%02X, want 0x%02X)",
                 kBmi270Addr, chip_id, kBmi270ChipId);
        i2c_master_bus_rm_device(bmi270_dev_);
        bmi270_dev_ = nullptr;
        return false;
    }

    // Bosch reference sequence: soft reset, upload the 8 KB configuration blob,
    // wait for the load to complete, then configure the sensors.
    reg_write(BMI2_CMD_REG_ADDR, BMI2_SOFT_RESET_CMD);
    vTaskDelay(pdMS_TO_TICKS(5));
    reg_read(BMI2_CHIP_ID_ADDR, &chip_id);
    vTaskDelay(pdMS_TO_TICKS(2));

    reg_write(BMI2_PWR_CONF_ADDR, BMI2_DISABLE);       // leave advanced power save
    uint8_t init_ctrl = 0x00;
    reg_write(BMI2_INIT_CTRL_ADDR, init_ctrl);         // config load off while uploading

    for (size_t index = 0; index < kBmiConfigFileSize; index += kBmiChunkSize) {
        const uint8_t addr[2] = {
            static_cast<uint8_t>((index / 2) & 0x0F),
            static_cast<uint8_t>((index / 2) >> 4),
        };
        if (!WriteRegs(bmi270_dev_, BMI2_INIT_ADDR_0, addr, sizeof(addr), 100) ||
            !WriteRegs(bmi270_dev_, BMI2_INIT_DATA_ADDR, &bmi270_config_file[index],
                       kBmiChunkSize, 200)) {
            ESP_LOGE(TAG, "BMI270 config upload failed at offset %u", (unsigned)index);
            i2c_master_bus_rm_device(bmi270_dev_);
            bmi270_dev_ = nullptr;
            return false;
        }
    }

    init_ctrl = 0x01;
    reg_write(BMI2_INIT_CTRL_ADDR, init_ctrl);  // apply the uploaded config
    reg_write(BMI2_PWR_CONF_ADDR, BMI2_ENABLE);

    vTaskDelay(pdMS_TO_TICKS(25));
    uint8_t load_status = 0;
    reg_read(BMI2_INTERNAL_STATUS_ADDR, &load_status);
    if ((load_status & BMI2_CONFIG_LOAD_STATUS_MASK) != BMI2_CONFIG_LOAD_SUCCESS) {
        ESP_LOGE(TAG, "BMI270 config load failed (INTERNAL_STATUS=0x%02X)", load_status);
        i2c_master_bus_rm_device(bmi270_dev_);
        bmi270_dev_ = nullptr;
        return false;
    }

    reg_write(kBmiRegAccConf, kBmiAccGyrConf);
    reg_write(kBmiRegGyrConf, kBmiAccGyrConf);
    reg_write(BMI2_PWR_CTRL_ADDR, BMI2_ACC_EN_MASK | BMI2_GYR_EN_MASK | BMI2_TEMP_EN_MASK);

    ESP_LOGI(TAG, "BMI270 ready at 0x%02X (accel +/-2g, gyro +/-2000dps, 100 Hz)", kBmi270Addr);
    return true;
}

bool SensorMonitor::ReadBmi270(MotionSensorData& data) {
    if (bmi270_dev_ == nullptr) return false;
    data.available = true;

    uint8_t buf[12] = {0};
    if (!ReadRegs(bmi270_dev_, kBmiRegDataStart, buf, sizeof(buf), 100)) {
        return false;
    }

    const int16_t raw_ax = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    const int16_t raw_ay = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    const int16_t raw_az = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    const int16_t raw_gx = static_cast<int16_t>(buf[6] | (buf[7] << 8));
    const int16_t raw_gy = static_cast<int16_t>(buf[8] | (buf[9] << 8));
    const int16_t raw_gz = static_cast<int16_t>(buf[10] | (buf[11] << 8));

    data.accel_x = raw_ax / kBmiAccLsbPerG;
    data.accel_y = raw_ay / kBmiAccLsbPerG;
    data.accel_z = raw_az / kBmiAccLsbPerG;
    data.gyro_x = raw_gx / kBmiGyrLsbPerDps;
    data.gyro_y = raw_gy / kBmiGyrLsbPerDps;
    data.gyro_z = raw_gz / kBmiGyrLsbPerDps;

    // Tilt away from the table plane, and which face is pointing down.
    const float magnitude =
        std::sqrt(data.accel_x * data.accel_x + data.accel_y * data.accel_y + data.accel_z * data.accel_z);
    const float z = magnitude > 0.1f ? std::fabs(data.accel_z) / magnitude : 0.0f;
    data.tilt_degrees = std::acos(z < 1.0f ? z : 1.0f) * 180.0f / 3.14159265f;

    const float ax = std::fabs(data.accel_x);
    const float ay = std::fabs(data.accel_y);
    const float az = std::fabs(data.accel_z);
    const char* face = (ay >= ax && ay >= az) ? (data.accel_y > 0 ? "mặt trước" : "mặt sau")
                     : (ax >= az)            ? (data.accel_x > 0 ? "cạnh phải" : "cạnh trái")
                                              : (data.accel_z > 0 ? "mặt lưng úp xuống"
                                                                  : "mặt kính úp xuống");
    data.posture = std::string(face) + " - nghiêng " + std::to_string(static_cast<int>(data.tilt_degrees)) + "°";
    return true;
}

CoreS3SensorSnapshot SensorMonitor::GetSnapshot() {
    CoreS3SensorSnapshot snapshot;
    if (i2c_mutex_ != nullptr) {
        xSemaphoreTake(i2c_mutex_, pdMS_TO_TICKS(200));
    }

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

    if (i2c_mutex_ != nullptr) {
        xSemaphoreGive(i2c_mutex_);
    }

    // 3. Doc du lieu Mang Wi-Fi
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

    // 4. Doc du lieu He thong & Bo nho
    snapshot.system.free_sram_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.free_psram_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snapshot.system.min_sram_bytes = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    snapshot.system.uptime_seconds = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
    snapshot.system.cpu_freq_mhz = esp_clk_cpu_freq();

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

        cJSON_AddNumberToObject(motion, "tilt_degrees", s.motion.tilt_degrees);
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
            cJSON_AddNumberToObject(root, "tilt_degrees", s.motion.tilt_degrees);
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
